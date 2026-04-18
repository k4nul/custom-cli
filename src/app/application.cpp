#include "starter/app/application.hpp"

#include <algorithm>
#include <iostream>
#include <utility>

#include <CLI/CLI.hpp>

#include "starter/commands/registrars.hpp"
#include "starter/core/config.hpp"
#include "starter/core/exit_code.hpp"
#include "starter/core/tokenize.hpp"

namespace starter {

Application::Application(ProjectInfo project_info, std::ostream& out, std::ostream& err)
    : project_info_(std::move(project_info)), out_(out), err_(err) {}

int Application::run(int argc, char** argv) {
    if (argc > 1) {
        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(argc - 1));
        for (int index = 1; index < argc; ++index) {
            args.emplace_back(argv[index]);
        }
        return dispatch(std::move(args), false);
    }

    return run_shell(default_config_path(project_info_));
}

int Application::dispatch(std::vector<std::string> args, bool interactive_mode) {
    std::string config_path = interactive_mode && !active_shell_config_path_.empty()
        ? active_shell_config_path_
        : default_config_path(project_info_).string();
    bool command_executed = false;
    bool shell_requested = false;

    CLI::App app(project_info_.display_name + " - generic C++ CLI starter");
    app.set_help_all_flag("--help-all", "Show help for all subcommands.");
    app.set_version_flag("--version", project_info_.display_name + " " + project_info_.version);
    app.add_option("-c,--config", config_path, "Path to the JSON configuration file.");

    auto* shell_command = app.add_subcommand("shell", "Start the interactive shell.");
    shell_command->callback([&]() {
        command_executed = true;
        shell_requested = true;
    });

    register_builtin_commands(app, project_info_, config_path, out_, err_, command_executed);

    try {
        std::reverse(args.begin(), args.end());
        app.parse(args);
    } catch (const CLI::ParseError& error) {
        return app.exit(error);
    } catch (const std::exception& error) {
        err_ << "error: " << error.what() << '\n';
        return to_int(ExitCode::runtime_error);
    }

    if (shell_requested) {
        return run_shell(std::filesystem::path(config_path));
    }

    if (!command_executed && !interactive_mode && !args.empty()) {
        err_ << app.help() << '\n';
        return to_int(ExitCode::usage_error);
    }

    return to_int(ExitCode::success);
}

int Application::run_shell(const std::filesystem::path& config_path) {
    active_shell_config_path_ = config_path.string();
    bool loaded_from_disk = false;
    AppConfig config;
    try {
        config = load_config_or_default(config_path, &loaded_from_disk);
    } catch (const std::exception& error) {
        err_ << "error: " << error.what() << '\n';
        active_shell_config_path_.clear();
        return to_int(ExitCode::config_error);
    }
    const auto prompt = config.prompt.empty() ? project_info_.prompt_label : config.prompt;

    out_ << project_info_.display_name << " " << project_info_.version << '\n';
    out_ << "Interactive mode. Type 'help' to inspect commands or 'exit' to quit.\n";
    if (!loaded_from_disk) {
        out_ << "Using built-in defaults until " << config_path.generic_string() << " exists.\n";
    }

    std::string line;
    while (true) {
        out_ << prompt << "> ";
        out_.flush();

        if (!std::getline(std::cin, line)) {
            out_ << '\n';
            break;
        }

        if (line.empty()) {
            continue;
        }

        std::vector<std::string> tokens;
        try {
            tokens = tokenize_command_line(line);
        } catch (const std::exception& error) {
            err_ << "input error: " << error.what() << '\n';
            continue;
        }

        if (tokens.empty()) {
            continue;
        }

        if (tokens.front() == "exit" || tokens.front() == "quit") {
            break;
        }

        if (tokens.front() == "help") {
            if (tokens.size() == 1) {
                (void)dispatch({"--help"}, true);
            } else {
                std::vector<std::string> help_args(tokens.begin() + 1, tokens.end());
                help_args.push_back("--help");
                (void)dispatch(help_args, true);
            }
            continue;
        }

        const int result = dispatch(std::move(tokens), true);
        if (result != to_int(ExitCode::success)) {
            err_ << "command finished with exit code " << result << '\n';
        }
    }

    active_shell_config_path_.clear();
    return to_int(ExitCode::success);
}

}  // namespace starter
