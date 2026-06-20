#include "starter/app/cli_app.hpp"

#include <string>

#include <CLI/CLI.hpp>

#include "starter/commands/registrars.hpp"

namespace starter {

void configure_cli_app(
    CLI::App& app,
    const CliAppContext& context) {
    const auto& project_info = context.project_info;
    auto& config_path = context.config_path;
    auto& out = context.out;
    auto& err = context.err;
    auto& command_executed = context.command_executed;
    auto& shell_requested = context.shell_requested;

    app.set_help_all_flag("--help-all", "Show help for all subcommands.");
    app.set_version_flag("--version", project_info.display_name + " " + project_info.version);
    app.add_option("-c,--config", config_path, "Path to the JSON configuration file.");

    auto* shell_command = app.add_subcommand("shell", "Start the interactive shell.");
    shell_command->callback([&]() {
        command_executed = true;
        shell_requested = true;
    });

    CommandRegistrationContext command_context{
        project_info,
        config_path,
        out,
        err,
        command_executed,
    };
    register_builtin_commands(app, command_context);
}

}  // namespace starter
