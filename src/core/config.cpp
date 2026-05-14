#include "starter/core/config.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
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

}  // namespace

std::string serialize_config(const AppConfig& config) {
    const json document = config;
    return document.dump(2) + '\n';
}

AppConfig parse_config(std::string_view text) {
    const json document = json::parse(text.begin(), text.end());
    AppConfig config;
    from_json(document, config);
    return config;
}

AppConfig load_config_or_throw(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open config file: " + path.generic_string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parse_config(buffer.str());
}

AppConfig load_config_or_default(const std::filesystem::path& path, bool* loaded) {
    if (!std::filesystem::exists(path)) {
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
    if (path.has_parent_path() && !path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to write config file: " + path.generic_string());
    }

    output << serialize_config(config);
}

std::string describe_config(const std::filesystem::path& path, const AppConfig& config, bool loaded_from_disk) {
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
