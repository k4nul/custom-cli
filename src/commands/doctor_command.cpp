#include "starter/commands/registrars.hpp"

#include <filesystem>
#include <ostream>
#include <string>

#include <CLI/CLI.hpp>

#include "starter/core/config.hpp"

namespace starter {

void register_doctor_command(
    CLI::App& root,
    const ProjectInfo& project_info,
    std::string& config_path,
    std::ostream& out,
    std::ostream& err,
    bool& command_executed) {
    (void)project_info;
    (void)err;

    auto* command = root.add_subcommand("doctor", "Check starter layout and configuration assumptions.");
    command->callback([&]() {
        command_executed = true;

        bool layout_ok = true;
        const auto check_path = [&](const char* label, const std::filesystem::path& path) {
            const bool exists = std::filesystem::exists(path);
            out << '[' << (exists ? "ok" : "missing") << "] " << label << ": " << path.generic_string() << '\n';
            layout_ok = layout_ok && exists;
        };

        check_path("source directory", "src");
        check_path("public headers", "include");
        check_path("docs directory", "docs");
        check_path("config directory", "config");
        check_path("third-party directory", "third_party");

        const auto path = std::filesystem::path(config_path);
        bool loaded_from_disk = false;
        const auto config = load_config_or_default(path, &loaded_from_disk);
        out << '[' << (loaded_from_disk ? "ok" : "warn") << "] config: " << path.generic_string();
        if (loaded_from_disk) {
            out << " loaded from disk\n";
        } else {
            out << " missing; built-in defaults are active\n";
        }
        out << "[info] prompt: " << config.prompt << '\n';
        out << "[info] default name: " << config.default_name << '\n';
        out << (layout_ok ? "Starter layout looks healthy.\n" : "Starter layout is missing recommended files.\n");
    });
}

}  // namespace starter
