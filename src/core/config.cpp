#include "starter/core/config.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

namespace starter {

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
    }
    if (const auto iterator = document.find("default_name"); iterator != document.end()) {
        config.default_name = iterator->get<std::string>();
    }
    if (const auto iterator = document.find("enabled_commands"); iterator != document.end()) {
        config.enabled_commands = iterator->get<std::vector<std::string>>();
    }
    if (const auto iterator = document.find("notes"); iterator != document.end()) {
        config.notes = iterator->get<std::string>();
    }
}

namespace {

using json = nlohmann::json;

std::string join_commands(const std::vector<std::string>& commands) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < commands.size(); ++index) {
        if (index != 0) {
            stream << ", ";
        }
        stream << commands[index];
    }
    return stream.str();
}

bool config_file_exists(const std::filesystem::path& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        throw ConfigReadError(
            "failed to inspect config file: " + path.generic_string() + ": " + error.message());
    }
    return exists;
}

}  // namespace

std::string serialize_config(const AppConfig& config) {
    const json document = config;
    return document.dump(2) + '\n';
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
    std::ifstream input(path);
    if (!input) {
        throw ConfigReadError("failed to open config file: " + path.generic_string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad()) {
        throw ConfigReadError("failed to read config file: " + path.generic_string());
    }
    return parse_config(buffer.str());
}

AppConfig load_config_or_default(const std::filesystem::path& path, bool* loaded) {
    if (!config_file_exists(path)) {
        if (loaded != nullptr) {
            *loaded = false;
        }
        return AppConfig{};
    }

    if (loaded != nullptr) {
        *loaded = true;
    }
    return load_config_or_throw(path);
}

void write_config_template(const std::filesystem::path& path, const AppConfig& config) {
    try {
        if (path.has_parent_path() && !path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path());
        }
    } catch (const std::filesystem::filesystem_error& error) {
        throw ConfigWriteError(
            "failed to prepare config directory for " + path.generic_string() + ": "
            + error.what());
    }

    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw ConfigWriteError("failed to write config file: " + path.generic_string());
    }

    output << serialize_config(config);
    if (!output) {
        throw ConfigWriteError("failed to write config file: " + path.generic_string());
    }
}

std::string describe_config(
    const std::filesystem::path& path,
    const AppConfig& config,
    bool loaded_from_disk) {
    std::ostringstream stream;
    stream << "Config path: " << path.generic_string() << '\n';
    stream << "Source: " << (loaded_from_disk ? "disk" : "built-in defaults") << '\n';
    stream << "Prompt: " << config.prompt << '\n';
    stream << "Default name: " << config.default_name << '\n';
    stream << "Enabled commands: " << join_commands(config.enabled_commands) << '\n';
    stream << "Notes: " << config.notes << '\n';
    return stream.str();
}

}  // namespace starter
