#include "starter/commands/registrars.hpp"

#include <filesystem>
#include <memory>
#include <ostream>
#include <string>

#include "starter/core/config.hpp"

namespace starter {

namespace {

struct HelloOptions {
    std::string name;
    bool enthusiastic = false;
};

}  // namespace

void register_hello_command(
    CLI::App& root,
    const ProjectInfo& project_info,
    std::string& config_path,
    std::ostream& out,
    std::ostream& err,
    bool& command_executed) {
    (void)project_info;
    (void)err;

    auto options = std::make_shared<HelloOptions>();
    auto* command = root.add_subcommand("hello", "Sample command that uses options plus config defaults.");
    command->add_option("--name", options->name, "Name to greet.");
    command->add_flag("-e,--enthusiastic", options->enthusiastic, "Use a more excited greeting.");

    command->callback([&, options]() {
        command_executed = true;

        bool loaded_from_disk = false;
        const auto config = load_config_or_default(std::filesystem::path(config_path), &loaded_from_disk);
        const auto selected_name = options->name.empty() ? config.default_name : options->name;

        out << "Hello, " << selected_name << (options->enthusiastic ? "!" : ".") << '\n';
        if (!loaded_from_disk && options->name.empty()) {
            out << "Tip: run `config init` to generate " << config_path << " and customize the default name.\n";
        }
    });
}

}  // namespace starter
