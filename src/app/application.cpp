#include "starter/app/application.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <CLI/CLI.hpp>

#include "starter/app/cli_app.hpp"
#include "starter/core/completion.hpp"
#include "starter/core/config.hpp"
#include "starter/core/exit_code.hpp"
#include "starter/core/shell_line_reader.hpp"
#include "starter/core/tokenize.hpp"

namespace starter {

namespace {

std::string shell_prompt_for(const AppConfig& config, const ProjectInfo& project_info) {
    return config.prompt.empty() ? project_info.prompt_label : config.prompt;
}

std::string cli_app_description(const ProjectInfo& project_info) {
    return project_info.display_name + " - generic C++ CLI starter";
}

bool is_shell_exit_command(const std::string& command) {
    return command == "exit" || command == "quit";
}

std::vector<std::string> make_shell_help_args(const std::vector<std::string>& tokens) {
    if (tokens.size() == 1) {
        return {"--help"};
    }

    std::vector<std::string> help_args(tokens.begin() + 1, tokens.end());
    help_args.push_back("--help");
    return help_args;
}

struct ConfiguredCliApp {
    ConfiguredCliApp(
        const ProjectInfo& project_info,
        std::string selected_config_path,
        std::ostream& out,
        std::ostream& err)
        : config_path(std::move(selected_config_path)),
          app(cli_app_description(project_info)) {
        const CliAppContext context{
            project_info,
            config_path,
            out,
            err,
            command_executed,
            shell_requested,
        };
        configure_cli_app(app, context);
    }

    std::string config_path;
    bool command_executed = false;
    bool shell_requested = false;
    CLI::App app;
};

}  // namespace

Application::Application(
    ProjectInfo project_info,
    std::ostream& out,
    std::ostream& err,
    ShellLineReader shell_line_reader)
    : project_info_(std::move(project_info)),
      out_(out),
      err_(err),
      shell_line_reader_(std::move(shell_line_reader)) {
    if (!shell_line_reader_) {
        shell_line_reader_ = read_shell_line;
    }
}

int Application::run(int argc, char** argv) {
    if (argc > 1) {
        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(argc - 1));
        for (int index = 1; index < argc; ++index) {
            args.emplace_back(argv[index]);
        }
        return dispatch(std::move(args), false, default_config_path(project_info_).string());
    }

    return run_shell(default_config_path(project_info_));
}

int Application::dispatch(
    std::vector<std::string> args,
    bool interactive_mode,
    std::string config_path) {
    ConfiguredCliApp cli(project_info_, std::move(config_path), out_, err_);

    try {
        std::reverse(args.begin(), args.end());
        cli.app.parse(args);
    } catch (const CLI::ParseError& error) {
        return cli.app.exit(error, out_, err_);
    } catch (const ConfigWriteError& error) {
        err_ << "error: " << error.what() << '\n';
        return to_int(ExitCode::io_error);
    } catch (const ConfigReadError& error) {
        err_ << "error: " << error.what() << '\n';
        return to_int(ExitCode::config_error);
    } catch (const ConfigParseError& error) {
        err_ << "error: " << error.what() << '\n';
        return to_int(ExitCode::config_error);
    } catch (const std::exception& error) {
        err_ << "error: " << error.what() << '\n';
        return to_int(ExitCode::runtime_error);
    }

    if (cli.shell_requested) {
        return run_shell(std::filesystem::path(cli.config_path));
    }

    if (!cli.command_executed && !interactive_mode && !args.empty()) {
        err_ << cli.app.help() << '\n';
        return to_int(ExitCode::usage_error);
    }

    return to_int(ExitCode::success);
}

bool Application::dispatch_shell_tokens(
    std::vector<std::string> tokens,
    const std::string& config_path) {
    if (tokens.empty()) {
        return true;
    }

    if (is_shell_exit_command(tokens.front())) {
        return false;
    }

    if (tokens.front() == "help") {
        (void)dispatch(make_shell_help_args(tokens), true, config_path);
        return true;
    }

    const int result = dispatch(std::move(tokens), true, config_path);
    if (result != to_int(ExitCode::success)) {
        err_ << "command finished with exit code " << result << '\n';
    }
    return true;
}

int Application::run_shell(const std::filesystem::path& config_path) {
    ConfigLoadResult loaded_config;
    try {
        loaded_config = load_config_with_source(config_path);
    } catch (const std::exception& error) {
        err_ << "error: " << error.what() << '\n';
        return to_int(ExitCode::config_error);
    }
    const auto prompt = shell_prompt_for(loaded_config.config, project_info_);

    out_ << project_info_.display_name << " " << project_info_.version << '\n';
    out_ << "Interactive mode. Type 'help' to inspect commands or 'exit' to quit.\n";
    if (!loaded_config.loaded_from_disk) {
        out_ << "Using built-in defaults until " << config_path.generic_string() << " exists.\n";
    }

    const auto shell_config_path = config_path.string();
    ConfiguredCliApp completion_cli(project_info_, shell_config_path, out_, err_);
    const auto shell_commands = shell_completion_commands();
    const auto completion_provider = [&](std::string_view current_line, std::size_t cursor) {
        return resolve_completion(current_line, cursor, completion_cli.app, shell_commands);
    };
    const auto prompt_text = prompt + "> ";

    std::string line;
    while (true) {
        const auto next_line = shell_line_reader_(prompt_text, out_, completion_provider);
        if (!next_line.has_value()) {
            out_ << '\n';
            break;
        }
        line = *next_line;

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

        if (!dispatch_shell_tokens(std::move(tokens), shell_config_path)) {
            break;
        }
    }

    return to_int(ExitCode::success);
}

std::vector<std::string> Application::shell_completion_commands() const {
    return {"help", "exit", "quit"};
}

}  // namespace starter
