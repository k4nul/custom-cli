#include "starter/core/config.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

namespace starter {

namespace {

constexpr std::size_t max_config_text_field_bytes = 4096U;
constexpr std::size_t max_enabled_commands = 64U;
constexpr std::size_t max_enabled_command_bytes = 256U;

void reject_oversized_string(
    const std::string& field_name,
    const std::string& value,
    std::size_t max_size) {
    if (value.size() > max_size) {
        throw ConfigParseError(
            "config field " + field_name + " exceeds "
            + std::to_string(max_size) + " bytes");
    }
}

void reject_oversized_enabled_commands(const std::vector<std::string>& commands) {
    if (commands.size() > max_enabled_commands) {
        throw ConfigParseError(
            "config field enabled_commands exceeds "
            + std::to_string(max_enabled_commands) + " entries");
    }

    for (const auto& command : commands) {
        reject_oversized_string("enabled_commands entry", command, max_enabled_command_bytes);
    }
}

}  // namespace

std::string escape_for_display(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        switch (byte) {
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (byte < 0x20 || byte == 0x7F) {
                constexpr char hex_digits[] = "0123456789abcdef";
                escaped += "\\x";
                escaped.push_back(hex_digits[(byte >> 4U) & 0x0FU]);
                escaped.push_back(hex_digits[byte & 0x0FU]);
            } else {
                escaped.push_back(character);
            }
            break;
        }
    }

    return escaped;
}

void to_json(nlohmann::json& document, const AppConfig& config) {
    document = nlohmann::json{
        {"prompt", config.prompt},
        {"default_name", config.default_name},
        {"enabled_commands", config.enabled_commands},
        {"notes", config.notes},
    };
}

void from_json(const nlohmann::json& document, AppConfig& config) {
    AppConfig defaults;
    config = defaults;

    if (const auto iterator = document.find("prompt"); iterator != document.end()) {
        config.prompt = iterator->get<std::string>();
        reject_oversized_string("prompt", config.prompt, max_config_text_field_bytes);
    }
    if (const auto iterator = document.find("default_name"); iterator != document.end()) {
        config.default_name = iterator->get<std::string>();
        reject_oversized_string("default_name", config.default_name, max_config_text_field_bytes);
    }
    if (const auto iterator = document.find("enabled_commands"); iterator != document.end()) {
        config.enabled_commands = iterator->get<std::vector<std::string>>();
        reject_oversized_enabled_commands(config.enabled_commands);
    }
    if (const auto iterator = document.find("notes"); iterator != document.end()) {
        config.notes = iterator->get<std::string>();
        reject_oversized_string("notes", config.notes, max_config_text_field_bytes);
    }
}

namespace {

using json = nlohmann::json;

constexpr std::size_t max_config_file_size_bytes = 1024U * 1024U;
constexpr std::string_view generated_config_template_notes =
    "Rename values and trim sample commands once you start customizing the starter.";

std::string join_commands(const std::vector<std::string>& commands) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < commands.size(); ++index) {
        if (index != 0) {
            stream << ", ";
        }
        stream << escape_for_display(commands[index]);
    }
    return stream.str();
}

std::string display_path(const std::filesystem::path& path) {
    return escape_for_display(path.generic_string());
}

bool config_file_exists(const std::filesystem::path& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        throw ConfigReadError(
            "failed to inspect config file: " + display_path(path) + ": " + error.message());
    }
    return exists;
}

std::string config_file_too_large_message(
    const std::filesystem::path& path,
    std::uintmax_t observed_size) {
    return "config file is too large: " + display_path(path) + " ("
        + std::to_string(observed_size) + " bytes, max "
        + std::to_string(max_config_file_size_bytes) + " bytes)";
}

std::uintmax_t inspect_config_file_for_read(const std::filesystem::path& path) {
    std::error_code error;
    const bool regular_file = std::filesystem::is_regular_file(path, error);
    if (error) {
        throw ConfigReadError(
            "failed to inspect config file: " + display_path(path) + ": "
            + error.message());
    }
    if (!regular_file) {
        throw ConfigReadError("config path is not a regular file: " + display_path(path));
    }

    const auto file_size = std::filesystem::file_size(path, error);
    if (error) {
        throw ConfigReadError(
            "failed to inspect config file: " + display_path(path) + ": "
            + error.message());
    }
    if (file_size > max_config_file_size_bytes) {
        throw ConfigReadError(config_file_too_large_message(path, file_size));
    }
    return file_size;
}

std::string read_config_text(
    const std::filesystem::path& path,
    std::uintmax_t inspected_size) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw ConfigReadError("failed to open config file: " + display_path(path));
    }

    std::string text;
    text.reserve(inspected_size);

    std::array<char, 4096> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto bytes_read = input.gcount();
        if (bytes_read <= 0) {
            continue;
        }

        const auto incoming_size = static_cast<std::size_t>(bytes_read);
        if (text.size() > max_config_file_size_bytes - incoming_size) {
            throw ConfigReadError(config_file_too_large_message(
                path,
                text.size() + incoming_size));
        }
        text.append(buffer.data(), incoming_size);
    }

    if (input.bad()) {
        throw ConfigReadError("failed to read config file: " + display_path(path));
    }
    return text;
}

void inspect_or_create_config_output_directory(
    const std::filesystem::path& directory,
    const std::filesystem::path& output_path) {
    std::error_code error;
    auto status = std::filesystem::symlink_status(directory, error);
    bool missing = false;
    if (error) {
        if (error == std::errc::no_such_file_or_directory) {
            error.clear();
            missing = true;
        } else {
            throw ConfigWriteError(
                "failed to inspect config directory for " + display_path(output_path) + ": "
                + display_path(directory) + ": " + error.message());
        }
    }

    if (missing || !std::filesystem::exists(status)) {
        error.clear();
        std::filesystem::create_directory(directory, error);
        if (error) {
            throw ConfigWriteError(
                "failed to prepare config directory for " + display_path(output_path) + ": "
                + display_path(directory) + ": " + error.message());
        }

        status = std::filesystem::symlink_status(directory, error);
        if (error) {
            throw ConfigWriteError(
                "failed to inspect config directory for " + display_path(output_path) + ": "
                + display_path(directory) + ": " + error.message());
        }
    }

    if (std::filesystem::is_symlink(status)) {
        throw ConfigWriteError(
            "config directory must not be a symlink: " + display_path(directory));
    }

    if (!std::filesystem::is_directory(status)) {
        throw ConfigWriteError(
            "failed to prepare config directory for " + display_path(output_path) + ": "
            + display_path(directory) + " is not a directory");
    }
}

void prepare_config_output_directory(
    const std::filesystem::path& directory,
    const std::filesystem::path& output_path) {
    if (directory.empty()) {
        return;
    }

    std::filesystem::path current;
    const auto root_name = directory.root_name();
    const auto root_directory = directory.root_directory();

    for (const auto& component : directory) {
        if (component.empty()) {
            continue;
        }
        current /= component;
        if ((!root_name.empty() && component == root_name)
            || (!root_directory.empty() && component == root_directory)) {
            continue;
        }

        inspect_or_create_config_output_directory(current, output_path);
    }
}

void inspect_config_file_for_write(const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error) {
        if (error == std::errc::no_such_file_or_directory) {
            return;
        }
        throw ConfigWriteError(
            "failed to inspect config output path: " + display_path(path) + ": "
            + error.message());
    }

    if (!std::filesystem::exists(status)) {
        return;
    }

    if (std::filesystem::is_symlink(status)) {
        throw ConfigWriteError(
            "config output path must not be a symlink: " + display_path(path));
    }

    if (!std::filesystem::is_regular_file(status)) {
        throw ConfigWriteError(
            "config output path is not a regular file: " + display_path(path));
    }
}

}  // namespace

std::string serialize_config(const AppConfig& config) {
    const json document = config;
    return document.dump(2) + '\n';
}

AppConfig make_generated_config_template(std::string_view prompt_label) {
    AppConfig config_template;
    config_template.prompt = std::string(prompt_label);
    config_template.notes = std::string(generated_config_template_notes);
    return config_template;
}

AppConfig parse_config(std::string_view text) {
    try {
        const json document = json::parse(text.begin(), text.end());
        if (!document.is_object()) {
            throw ConfigParseError("config root must be a JSON object");
        }

        AppConfig config;
        from_json(document, config);
        return config;
    } catch (const ConfigParseError&) {
        throw;
    } catch (const json::exception& error) {
        throw ConfigParseError(error.what());
    }
}

AppConfig load_config_or_throw(const std::filesystem::path& path) {
    if (!config_file_exists(path)) {
        throw ConfigReadError("failed to open config file: " + display_path(path));
    }

    const auto inspected_size = inspect_config_file_for_read(path);
    return parse_config(read_config_text(path, inspected_size));
}

ConfigLoadResult load_config_with_source(const std::filesystem::path& path) {
    if (!config_file_exists(path)) {
        return {};
    }

    return {load_config_or_throw(path), true};
}

AppConfig load_config_or_default(const std::filesystem::path& path, bool* loaded) {
    const auto result = load_config_with_source(path);

    if (loaded != nullptr) {
        *loaded = result.loaded_from_disk;
    }
    return result.config;
}

void write_config_template(const std::filesystem::path& path, const AppConfig& config) {
    if (path.has_parent_path() && !path.parent_path().empty()) {
        prepare_config_output_directory(path.parent_path(), path);
    }

    inspect_config_file_for_write(path);

    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw ConfigWriteError("failed to write config file: " + display_path(path));
    }

    output << serialize_config(config);
    if (!output) {
        throw ConfigWriteError("failed to write config file: " + display_path(path));
    }
}

std::string describe_config(
    const std::filesystem::path& path,
    const ConfigLoadResult& loaded_config) {
    return describe_config(path, loaded_config.config, loaded_config.loaded_from_disk);
}

std::string describe_config(
    const std::filesystem::path& path,
    const AppConfig& config,
    bool loaded_from_disk) {
    std::ostringstream stream;
    stream << "Config path: " << display_path(path) << '\n';
    stream << "Source: " << (loaded_from_disk ? "disk" : "built-in defaults") << '\n';
    stream << "Prompt: " << escape_for_display(config.prompt) << '\n';
    stream << "Default name: " << escape_for_display(config.default_name) << '\n';
    stream << "Enabled commands: " << join_commands(config.enabled_commands) << '\n';
    stream << "Notes: " << escape_for_display(config.notes) << '\n';
    return stream.str();
}

}  // namespace starter
