#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace starter {

class ConfigReadError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ConfigWriteError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ConfigParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct AppConfig {
    std::string prompt = "starter";
    std::string default_name = "world";
    std::vector<std::string> enabled_commands = {"about", "hello", "echo", "config", "doctor"};
    std::string notes = "Customize this file after copying the starter.";
};

std::string serialize_config(const AppConfig& config);
AppConfig parse_config(std::string_view text);
AppConfig load_config_or_throw(const std::filesystem::path& path);
AppConfig load_config_or_default(const std::filesystem::path& path, bool* loaded = nullptr);
void write_config_template(const std::filesystem::path& path, const AppConfig& config);
std::string describe_config(const std::filesystem::path& path, const AppConfig& config, bool loaded_from_disk);

}  // namespace starter
