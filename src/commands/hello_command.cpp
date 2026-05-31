#include "builtin_command_registrars.hpp"

#include <filesystem>
#include <memory>
#include <ostream>
#include <string>

#include <CLI/CLI.hpp>

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
    const CommandRegistrationContext& context) {
    auto& config_path = context.config_path;
    auto& out = context.out;
    auto& command_executed = context.command_executed;

    auto options = std::make_shared<HelloOptions>();
    auto* command = root.add_subcommand(
        "hello",
        "Sample command that uses options plus config defaults.");
    command->add_option("--name", options->name, "Name to greet.");
    command->add_flag("-e,--enthusiastic", options->enthusiastic, "Use a more excited greeting.");

    command->callback([&, options]() {
        command_executed = true;

        const auto loaded_config = load_config_with_source(std::filesystem::path(config_path));
        const auto selected_name =
            options->name.empty() ? loaded_config.config.default_name : options->name;

        out << "Hello, " << selected_name << (options->enthusiastic ? "!" : ".") << '\n';
        if (!loaded_config.loaded_from_disk && options->name.empty()) {
            out << "Tip: run `config init` to generate " << config_path
                << " and customize the default name.\n";
        }
    });
}

}  // namespace starter
