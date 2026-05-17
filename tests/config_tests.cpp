#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <CLI/CLI.hpp>

#include "starter/app/application.hpp"
#include "starter/commands/registrars.hpp"
#include "starter/core/completion.hpp"
#include "starter/core/config.hpp"
#include "starter/core/exit_code.hpp"
#include "starter/core/project_info.hpp"
#include "starter/core/tokenize.hpp"

namespace {

namespace fs = std::filesystem;

bool contains(const std::vector<std::string>& values, const std::string& expected) {
    return std::find(values.begin(), values.end(), expected) != values.end();
}

bool contains_text(const std::string& value, const std::string& expected) {
    return value.find(expected) != std::string::npos;
}

std::size_t next_temp_directory_id() {
    static std::size_t counter = 0;
    return ++counter;
}

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = fs::temp_directory_path()
            / ("cli-starter-tests-" + std::to_string(timestamp) + "-" + std::to_string(next_temp_directory_id()));
        fs::create_directories(path_);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    ~TemporaryDirectory() {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }

    const fs::path& path() const {
        return path_;
    }

private:
    fs::path path_;
};

class CurrentPathGuard {
public:
    explicit CurrentPathGuard(const fs::path& path) : original_path_(fs::current_path()) {
        fs::current_path(path);
    }

    CurrentPathGuard(const CurrentPathGuard&) = delete;
    CurrentPathGuard& operator=(const CurrentPathGuard&) = delete;

    ~CurrentPathGuard() {
        std::error_code ignored;
        fs::current_path(original_path_, ignored);
    }

private:
    fs::path original_path_;
};

struct ApplicationRunResult {
    int exit_code = 0;
    std::string out;
    std::string err;
    std::vector<std::string> prompts;
};

ApplicationRunResult run_application(std::vector<std::string> arguments) {
    std::ostringstream out;
    std::ostringstream err;
    const auto project_info = starter::load_project_info();
    starter::Application application(project_info, out, err);

    std::vector<std::string> arg_storage = {project_info.binary_name};
    arg_storage.insert(arg_storage.end(), arguments.begin(), arguments.end());

    std::vector<char*> argv;
    argv.reserve(arg_storage.size());
    for (auto& arg : arg_storage) {
        argv.push_back(arg.data());
    }

    const int exit_code = application.run(static_cast<int>(argv.size()), argv.data());
    return {exit_code, out.str(), err.str(), {}};
}

ApplicationRunResult run_application_with_scripted_shell(
    std::vector<std::string> arguments,
    std::vector<std::string> shell_lines) {
    std::ostringstream out;
    std::ostringstream err;
    std::vector<std::string> prompts;
    const auto project_info = starter::load_project_info();

    starter::ShellLineReader shell_reader =
        [shell_lines = std::move(shell_lines), &prompts, next_line = std::size_t{0}](
            const std::string& prompt,
            std::ostream& shell_out,
            const starter::CompletionProvider&) mutable -> std::optional<std::string> {
            prompts.push_back(prompt);
            shell_out << prompt;
            if (next_line >= shell_lines.size()) {
                return std::nullopt;
            }
            return shell_lines[next_line++];
        };

    starter::Application application(project_info, out, err, std::move(shell_reader));

    std::vector<std::string> arg_storage = {project_info.binary_name};
    arg_storage.insert(arg_storage.end(), arguments.begin(), arguments.end());

    std::vector<char*> argv;
    argv.reserve(arg_storage.size());
    for (auto& arg : arg_storage) {
        argv.push_back(arg.data());
    }

    const int exit_code = application.run(static_cast<int>(argv.size()), argv.data());
    return {exit_code, out.str(), err.str(), prompts};
}

void configure_starter_app(
    CLI::App& app,
    const starter::ProjectInfo& project_info,
    std::string& config_path,
    std::ostream& out,
    std::ostream& err,
    bool& command_executed,
    bool& shell_requested) {
    app.set_help_all_flag("--help-all", "Show help for all subcommands.");
    app.set_version_flag("--version", project_info.display_name + " " + project_info.version);
    app.add_option("-c,--config", config_path, "Path to the JSON configuration file.");

    auto* shell_command = app.add_subcommand("shell", "Start the interactive shell.");
    shell_command->callback([&]() {
        command_executed = true;
        shell_requested = true;
    });

    starter::register_builtin_commands(app, project_info, config_path, out, err, command_executed);
}

struct CompletionAppFixture {
    starter::ProjectInfo project_info = starter::load_project_info();
    std::string config_path = "cli-starter.json";
    std::ostringstream out;
    std::ostringstream err;
    bool command_executed = false;
    bool shell_requested = false;
    CLI::App app{project_info.display_name + " - generic C++ CLI starter"};
    std::vector<std::string> shell_commands = {"help", "exit", "quit"};

    CompletionAppFixture() {
        configure_starter_app(app, project_info, config_path, out, err, command_executed, shell_requested);
    }
};

void create_recommended_starter_layout(const fs::path& root) {
    const std::vector<std::string> directories = {"src", "include", "docs", "config", "third_party"};
    for (const auto& directory : directories) {
        fs::create_directories(root / directory);
    }
}

void write_text_file(const fs::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::trunc);
    output << text;
}

}  // namespace

TEST_CASE("tokenizer preserves quoted groups") {
    const auto tokens = starter::tokenize_command_line("hello --name \"starter user\" 'quoted value'");

    REQUIRE(tokens.size() == 4);
    CHECK(tokens[0] == "hello");
    CHECK(tokens[1] == "--name");
    CHECK(tokens[2] == "starter user");
    CHECK(tokens[3] == "quoted value");
}

TEST_CASE("tokenizer preserves empty quoted arguments") {
    const auto tokens = starter::tokenize_command_line("echo \"\" '' bare\"\" \"two words\"");
    const std::vector<std::string> expected = {"echo", "", "", "bare", "two words"};

    CHECK(tokens == expected);
}

TEST_CASE("tokenizer reports malformed shell input") {
    CHECK_THROWS_WITH_AS(
        starter::tokenize_command_line("hello --name \"starter user"),
        "unterminated quote in command line",
        std::runtime_error);
    CHECK_THROWS_WITH_AS(
        starter::tokenize_command_line("hello --name starter\\"),
        "trailing escape character in command line",
        std::runtime_error);
}

TEST_CASE("config can round-trip through JSON") {
    starter::AppConfig config;
    config.prompt = "custom";
    config.default_name = "engineer";
    config.enabled_commands = {"hello", "echo"};
    config.notes = "Round-trip test";

    const auto serialized = starter::serialize_config(config);
    const auto parsed = starter::parse_config(serialized);
    const std::vector<std::string> expected_commands = {"hello", "echo"};

    CHECK(parsed.prompt == "custom");
    CHECK(parsed.default_name == "engineer");
    CHECK(parsed.enabled_commands == expected_commands);
    CHECK(parsed.notes == "Round-trip test");
}

TEST_CASE("config parsing rejects wrong-type fields") {
    const std::vector<std::string> invalid_configs = {
        R"({"prompt":123})",
        R"({"default_name":true})",
        R"({"enabled_commands":"hello"})",
        R"({"enabled_commands":["hello",7]})",
        R"({"notes":{"text":"bad"}})",
    };

    for (const auto& invalid_config : invalid_configs) {
        CHECK_THROWS_AS(starter::parse_config(invalid_config), std::exception);
    }
}

TEST_CASE("config parsing rejects non-object documents") {
    const std::vector<std::string> invalid_configs = {
        "[]",
        "42",
        "null",
        R"("hello")",
    };

    for (const auto& invalid_config : invalid_configs) {
        CHECK_THROWS_WITH_AS(
            starter::parse_config(invalid_config),
            "config root must be a JSON object",
            starter::ConfigParseError);
    }
}

TEST_CASE("config read failures use typed errors") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "missing.json";
    bool caught = false;

    try {
        (void)starter::load_config_or_throw(config_path);
    } catch (const starter::ConfigReadError& error) {
        caught = true;
        CHECK(contains_text(error.what(), "failed to open config file"));
        CHECK(contains_text(error.what(), config_path.generic_string()));
    }

    CHECK(caught);
}

TEST_CASE("config write failures use typed errors") {
    TemporaryDirectory temporary_directory;
    const auto blocking_parent = temporary_directory.path() / "config-parent";
    write_text_file(blocking_parent, "not a directory");
    const auto config_path = blocking_parent / "starter.json";
    bool caught = false;

    try {
        starter::write_config_template(config_path, starter::AppConfig{});
    } catch (const starter::ConfigWriteError& error) {
        caught = true;
        CHECK(contains_text(error.what(), "failed"));
        CHECK(contains_text(error.what(), config_path.generic_string()));
    }

    CHECK(caught);
    std::error_code ignored;
    CHECK_FALSE(fs::exists(config_path, ignored));
}

TEST_CASE("application accepts hello subcommand options from argv order") {
    const auto result = run_application({"hello", "--name", "starter user"});

    CHECK(result.exit_code == 0);
    CHECK(result.out == "Hello, starter user.\n");
    CHECK(result.err.empty());
}

TEST_CASE("application echoes positional text") {
    const auto result = run_application({"echo", "one", "two words", "three"});

    CHECK(result.exit_code == 0);
    CHECK(result.out == "one two words three\n");
    CHECK(result.err.empty());
}

TEST_CASE("application echoes uppercase positional text") {
    const auto result = run_application({"echo", "--uppercase", "mixed", "Case"});

    CHECK(result.exit_code == 0);
    CHECK(result.out == "MIXED CASE\n");
    CHECK(result.err.empty());
}

TEST_CASE("application echoes numbered positional text") {
    const auto result = run_application({"echo", "--numbered", "one", "two"});

    CHECK(result.exit_code == 0);
    CHECK(result.out == "1. one\n2. two\n");
    CHECK(result.err.empty());
}

TEST_CASE("application echoes uppercase numbered positional text") {
    const auto result = run_application({"echo", "--uppercase", "--numbered", "one", "two"});

    CHECK(result.exit_code == 0);
    CHECK(result.out == "1. ONE\n2. TWO\n");
    CHECK(result.err.empty());
}

TEST_CASE("application routes version output through configured stream") {
    const auto project_info = starter::load_project_info();
    const auto result = run_application({"--version"});

    CHECK(result.exit_code == 0);
    CHECK(result.out == project_info.display_name + " " + project_info.version + "\n");
    CHECK(result.err.empty());
}

TEST_CASE("application routes help output through configured stream") {
    const auto result = run_application({"--help"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, "Usage:"));
    CHECK(contains_text(result.out, "hello"));
    CHECK(contains_text(result.out, "--help-all"));
    CHECK(result.err.empty());
}

TEST_CASE("application routes help-all output through configured stream") {
    const auto result = run_application({"--help-all"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, "Usage:"));
    CHECK(contains_text(result.out, "Start the interactive shell."));
    CHECK(contains_text(result.out, "Sample command that uses options plus config defaults."));
    CHECK(contains_text(result.out, "--enthusiastic"));
    CHECK(contains_text(result.out, "Echo text to demonstrate positional arguments."));
    CHECK(contains_text(result.out, "--uppercase"));
    CHECK(contains_text(result.out, "Write or inspect starter configuration."));
    CHECK(contains_text(result.out, "init"));
    CHECK(contains_text(result.out, "show"));
    CHECK(result.err.empty());
}

TEST_CASE("application routes parse errors through configured stream") {
    const auto result = run_application({"missing-command"});

    CHECK(result.exit_code != 0);
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "missing-command"));
    CHECK(contains_text(result.err, "Run with --help"));
}

TEST_CASE("application reports missing echo text through stderr") {
    const auto result = run_application({"echo"});

    CHECK(result.exit_code != 0);
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "text is required"));
    CHECK(contains_text(result.err, "Run with --help"));
}

TEST_CASE("application reports unknown options through stderr") {
    const auto result = run_application({"hello", "--unknown"});

    CHECK(result.exit_code != 0);
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "not expected"));
    CHECK(contains_text(result.err, "--unknown"));
    CHECK(contains_text(result.err, "Run with --help"));
}

TEST_CASE("application reports missing config subcommand through stderr") {
    const auto result = run_application({"config"});

    CHECK(result.exit_code != 0);
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "A subcommand is required"));
    CHECK(contains_text(result.err, "Run with --help"));
}

TEST_CASE("interactive shell runs no-argv sessions through the normal dispatch path") {
    TemporaryDirectory temporary_directory;
    const CurrentPathGuard current_path(temporary_directory.path());
    const auto project_info = starter::load_project_info();

    const auto result = run_application_with_scripted_shell({}, {"help", "hello --name Ada", "exit"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, project_info.display_name + " " + project_info.version + "\n"));
    CHECK(contains_text(result.out, "Interactive mode. Type 'help' to inspect commands or 'exit' to quit.\n"));
    CHECK(contains_text(result.out, "Using built-in defaults until config/cli-starter.json exists.\n"));
    CHECK(contains_text(result.out, "Usage:"));
    CHECK(contains_text(result.out, "Hello, Ada.\n"));
    CHECK(result.err.empty());
    CHECK(result.prompts == std::vector<std::string>{"starter> ", "starter> ", "starter> "});
}

TEST_CASE("interactive shell reuses disk config and recovers from malformed input") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "custom.json";
    starter::AppConfig config;
    config.prompt = "custom";
    config.default_name = "Grace";
    starter::write_config_template(config_path, config);

    const auto result = run_application_with_scripted_shell(
        {"--config", config_path.string(), "shell"},
        {"hello", "hello --name \"broken", "echo --numbered \"\" two", "quit"});

    CHECK(result.exit_code == 0);
    CHECK_FALSE(contains_text(result.out, "Using built-in defaults"));
    CHECK(contains_text(result.out, "Hello, Grace.\n"));
    CHECK(contains_text(result.out, "1. \n2. two\n"));
    CHECK(contains_text(result.err, "input error: unterminated quote in command line\n"));
    CHECK_FALSE(contains_text(result.err, "command finished with exit code"));
    CHECK(result.prompts == std::vector<std::string>{"custom> ", "custom> ", "custom> ", "custom> "});
}

TEST_CASE("interactive shell routes command-specific help through normal dispatch") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "missing.json";

    const auto result = run_application_with_scripted_shell(
        {"--config", config_path.string(), "shell"},
        {"help hello", "help config init", "exit"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, "Sample command that uses options plus config defaults."));
    CHECK(contains_text(result.out, "--name"));
    CHECK(contains_text(result.out, "--enthusiastic"));
    CHECK(contains_text(result.out, "Write a starter config template."));
    CHECK(contains_text(result.out, "--output"));
    CHECK(result.err.empty());
    CHECK(result.prompts == std::vector<std::string>{"starter> ", "starter> ", "starter> "});
}

TEST_CASE("interactive shell reports parse failures and keeps the session alive") {
    const auto result = run_application_with_scripted_shell(
        {},
        {"missing-command", "hello --name Ada", "exit"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, "Hello, Ada.\n"));
    CHECK(contains_text(result.err, "missing-command"));
    CHECK(contains_text(result.err, "Run with --help"));
    CHECK(contains_text(result.err, "command finished with exit code "));
    CHECK(result.prompts == std::vector<std::string>{"starter> ", "starter> ", "starter> "});
}

TEST_CASE("interactive shell reports command failures and keeps the session alive") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "missing.json";
    const auto blocking_parent = temporary_directory.path() / "config-parent";
    write_text_file(blocking_parent, "not a directory");
    const auto output_path = blocking_parent / "custom.json";

    const auto result = run_application_with_scripted_shell(
        {"--config", config_path.string(), "shell"},
        {"config init --output " + output_path.string(), "hello --name Ada", "exit"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, "Hello, Ada.\n"));
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "failed to prepare config directory"));
    CHECK(contains_text(
        result.err,
        "command finished with exit code " + std::to_string(starter::to_int(starter::ExitCode::io_error))));
    CHECK(result.prompts == std::vector<std::string>{"starter> ", "starter> ", "starter> "});

    std::error_code ignored;
    CHECK_FALSE(fs::exists(output_path, ignored));
}

TEST_CASE("about command reports starter metadata") {
    const auto project_info = starter::load_project_info();
    const auto result = run_application({"about"});
    const std::string expected = project_info.display_name + " " + project_info.version
        + "\nBinary name: " + project_info.binary_name
        + "\nDefault config: " + starter::default_config_path(project_info).generic_string()
        + "\nThis repository is a neutral CLI starter with one-shot commands,\n"
          "an interactive shell, JSON config scaffolding, and sample commands.\n";

    CHECK(result.exit_code == 0);
    CHECK(result.out == expected);
    CHECK(result.err.empty());
}

TEST_CASE("application reads custom config path for config-backed commands") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "custom.json";

    starter::AppConfig config;
    config.default_name = "Ada";
    starter::write_config_template(config_path, config);

    const auto result = run_application({"--config", config_path.string(), "hello"});

    CHECK(result.exit_code == 0);
    CHECK(result.out == "Hello, Ada.\n");
    CHECK(result.err.empty());
}

TEST_CASE("application explains missing config defaults for hello") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "missing.json";

    const auto result = run_application({"--config", config_path.string(), "hello"});
    const std::string expected = "Hello, world.\nTip: run `config init` to generate "
        + config_path.generic_string() + " and customize the default name.\n";

    CHECK(result.exit_code == 0);
    CHECK(result.out == expected);
    CHECK(result.err.empty());
    CHECK_FALSE(fs::exists(config_path));
}

TEST_CASE("config init honors global config path by default") {
    TemporaryDirectory temporary_directory;
    const CurrentPathGuard current_path(temporary_directory.path());
    const auto config_path = temporary_directory.path() / "profiles" / "custom.json";

    const auto result = run_application({"--config", config_path.string(), "config", "init"});

    CHECK(result.exit_code == 0);
    CHECK(result.out == "Wrote config template to " + config_path.generic_string() + '\n');
    CHECK(result.err.empty());
    CHECK(fs::exists(config_path));
    CHECK_FALSE(fs::exists(temporary_directory.path() / "config" / "cli-starter.json"));

    const auto config = starter::load_config_or_throw(config_path);
    CHECK(config.prompt == starter::load_project_info().prompt_label);
}

TEST_CASE("config init explicit output overrides global config path") {
    TemporaryDirectory temporary_directory;
    const CurrentPathGuard current_path(temporary_directory.path());
    const auto config_path = temporary_directory.path() / "profiles" / "custom.json";
    const auto output_path = temporary_directory.path() / "explicit" / "starter.json";

    const auto result =
        run_application({"--config", config_path.string(), "config", "init", "--output", output_path.string()});

    CHECK(result.exit_code == 0);
    CHECK(result.out == "Wrote config template to " + output_path.generic_string() + '\n');
    CHECK(result.err.empty());
    CHECK(fs::exists(output_path));
    CHECK_FALSE(fs::exists(config_path));
}

TEST_CASE("config init reports write failures through stderr") {
    TemporaryDirectory temporary_directory;
    const auto blocking_parent = temporary_directory.path() / "config-parent";
    write_text_file(blocking_parent, "not a directory");
    const auto config_path = blocking_parent / "custom.json";

    const auto result = run_application({"--config", config_path.string(), "config", "init"});

    CHECK(result.exit_code == starter::to_int(starter::ExitCode::io_error));
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "failed"));
    CHECK(contains_text(result.err, config_path.generic_string()));
}

TEST_CASE("config show describes built-in defaults when config is missing") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "missing.json";
    const starter::AppConfig defaults;

    const auto result = run_application({"--config", config_path.string(), "config", "show"});
    std::ostringstream expected;
    expected << "Config path: " << config_path.generic_string() << '\n';
    expected << "Source: built-in defaults\n";
    expected << "Prompt: " << defaults.prompt << '\n';
    expected << "Default name: " << defaults.default_name << '\n';
    expected << "Enabled commands: about, hello, echo, config, doctor\n";
    expected << "Notes: " << defaults.notes << '\n';

    CHECK(result.exit_code == 0);
    CHECK(result.out == expected.str());
    CHECK(result.err.empty());
    CHECK_FALSE(fs::exists(config_path));
}

TEST_CASE("config show applies defaults for omitted disk config fields") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "partial.json";
    const starter::AppConfig defaults;
    write_text_file(config_path, R"({"default_name":"Grace"})");

    const auto result = run_application({"--config", config_path.string(), "config", "show"});
    std::ostringstream expected;
    expected << "Config path: " << config_path.generic_string() << '\n';
    expected << "Source: disk\n";
    expected << "Prompt: " << defaults.prompt << '\n';
    expected << "Default name: Grace\n";
    expected << "Enabled commands: about, hello, echo, config, doctor\n";
    expected << "Notes: " << defaults.notes << '\n';

    CHECK(result.exit_code == 0);
    CHECK(result.out == expected.str());
    CHECK(result.err.empty());
}

TEST_CASE("config show reports malformed disk config through stderr") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "bad.json";
    write_text_file(config_path, R"({"default_name":)");

    const auto result = run_application({"--config", config_path.string(), "config", "show"});

    CHECK(result.exit_code == starter::to_int(starter::ExitCode::config_error));
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "parse error"));
}

TEST_CASE("config show reports non-object disk config through stderr") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "array.json";
    write_text_file(config_path, R"(["hello"])");

    const auto result = run_application({"--config", config_path.string(), "config", "show"});

    CHECK(result.exit_code == starter::to_int(starter::ExitCode::config_error));
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "config root must be a JSON object"));
}

TEST_CASE("config show reports wrong-type disk config through stderr") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "wrong-type.json";
    write_text_file(config_path, R"({"enabled_commands":"hello"})");

    const auto result = run_application({"--config", config_path.string(), "config", "show"});

    CHECK(result.exit_code == starter::to_int(starter::ExitCode::config_error));
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "type must be array"));
}

TEST_CASE("config-backed hello reports malformed disk config through stderr") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "bad.json";
    write_text_file(config_path, R"({"default_name":)");

    const auto result = run_application({"--config", config_path.string(), "hello"});

    CHECK(result.exit_code == starter::to_int(starter::ExitCode::config_error));
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "parse error"));
}

TEST_CASE("config-backed hello reports wrong-type disk config through stderr") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "wrong-type.json";
    write_text_file(config_path, R"({"default_name":42})");

    const auto result = run_application({"--config", config_path.string(), "hello"});

    CHECK(result.exit_code == starter::to_int(starter::ExitCode::config_error));
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "type must be string"));
}

TEST_CASE("doctor reports healthy starter layout with missing config warning") {
    TemporaryDirectory temporary_directory;
    create_recommended_starter_layout(temporary_directory.path());
    const CurrentPathGuard current_path(temporary_directory.path());
    const auto config_path = temporary_directory.path() / "config" / "local.json";

    const auto result = run_application({"--config", config_path.string(), "doctor"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, "[ok] source directory: src\n"));
    CHECK(contains_text(result.out, "[ok] public headers: include\n"));
    CHECK(contains_text(result.out, "[ok] docs directory: docs\n"));
    CHECK(contains_text(result.out, "[ok] config directory: config\n"));
    CHECK(contains_text(result.out, "[ok] third-party directory: third_party\n"));
    CHECK(contains_text(
        result.out,
        "[warn] config: " + config_path.generic_string() + " missing; built-in defaults are active\n"));
    CHECK(contains_text(result.out, "[info] prompt: starter\n"));
    CHECK(contains_text(result.out, "[info] default name: world\n"));
    CHECK(contains_text(result.out, "Starter layout looks healthy.\n"));
    CHECK(result.err.empty());
}

TEST_CASE("doctor reports disk config and missing recommended layout") {
    TemporaryDirectory temporary_directory;
    const CurrentPathGuard current_path(temporary_directory.path());
    const auto config_path = temporary_directory.path() / "config" / "local.json";

    starter::AppConfig config;
    config.prompt = "project";
    config.default_name = "Ada";
    starter::write_config_template(config_path, config);

    const auto result = run_application({"--config", config_path.string(), "doctor"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, "[missing] source directory: src\n"));
    CHECK(contains_text(result.out, "[missing] public headers: include\n"));
    CHECK(contains_text(result.out, "[missing] docs directory: docs\n"));
    CHECK(contains_text(result.out, "[ok] config directory: config\n"));
    CHECK(contains_text(result.out, "[missing] third-party directory: third_party\n"));
    CHECK(contains_text(result.out, "[ok] config: " + config_path.generic_string() + " loaded from disk\n"));
    CHECK(contains_text(result.out, "[info] prompt: project\n"));
    CHECK(contains_text(result.out, "[info] default name: Ada\n"));
    CHECK(contains_text(result.out, "Starter layout is missing recommended files.\n"));
    CHECK(result.err.empty());
}

TEST_CASE("doctor reports malformed disk config through stderr") {
    TemporaryDirectory temporary_directory;
    create_recommended_starter_layout(temporary_directory.path());
    const CurrentPathGuard current_path(temporary_directory.path());
    const auto config_path = temporary_directory.path() / "config" / "bad.json";
    write_text_file(config_path, R"({"default_name":)");

    const auto result = run_application({"--config", config_path.string(), "doctor"});

    CHECK(result.exit_code == starter::to_int(starter::ExitCode::config_error));
    CHECK(contains_text(result.out, "[ok] source directory: src\n"));
    CHECK(contains_text(result.out, "[ok] third-party directory: third_party\n"));
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "parse error"));
    CHECK_FALSE(contains_text(result.out, "Starter layout looks healthy."));
}

TEST_CASE("doctor reports wrong-type disk config through stderr") {
    TemporaryDirectory temporary_directory;
    create_recommended_starter_layout(temporary_directory.path());
    const CurrentPathGuard current_path(temporary_directory.path());
    const auto config_path = temporary_directory.path() / "config" / "wrong-type.json";
    write_text_file(config_path, R"({"prompt":7})");

    const auto result = run_application({"--config", config_path.string(), "doctor"});

    CHECK(result.exit_code == starter::to_int(starter::ExitCode::config_error));
    CHECK(contains_text(result.out, "[ok] config directory: config\n"));
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "type must be string"));
    CHECK_FALSE(contains_text(result.out, "[info] prompt:"));
}

TEST_CASE("tab completion filters root command prefixes") {
    const std::vector<std::string> commands = {"abcd", "efgh", "abab"};

    const auto blank = starter::resolve_completion("", 0, commands);
    CHECK(blank.candidates == commands);

    const auto a_prefix = starter::resolve_completion("a", 1, commands);
    const std::vector<std::string> expected_a = {"abcd", "abab"};
    CHECK(a_prefix.candidates == expected_a);

    const auto e_prefix = starter::resolve_completion("e", 1, commands);
    REQUIRE(e_prefix.candidates.size() == 1);
    CHECK(e_prefix.candidates.front() == "efgh");

    starter::TabCompletionState state;
    const auto blank_first_tab = starter::choose_tab_completion(blank, "", 0, state);
    CHECK(blank_first_tab.kind == starter::CompletionActionKind::no_change);

    const auto blank_second_tab = starter::choose_tab_completion(blank, "", 0, state);
    CHECK(blank_second_tab.kind == starter::CompletionActionKind::list);
    CHECK(blank_second_tab.candidates == commands);

    const auto e_tab = starter::choose_tab_completion(e_prefix, "e", 1, state);
    CHECK(e_tab.kind == starter::CompletionActionKind::replace);
    CHECK(e_tab.replacement == "efgh");
}

TEST_CASE("tab completion expands shared candidate prefixes") {
    starter::TabCompletionState state;
    const std::vector<std::string> help_commands = {"help", "hello"};

    const auto h_prefix = starter::resolve_completion("h", 1, help_commands);
    const auto h_tab = starter::choose_tab_completion(h_prefix, "h", 1, state);
    CHECK(h_tab.kind == starter::CompletionActionKind::replace);
    CHECK(h_tab.replacement == "hel");

    const auto hel_prefix = starter::resolve_completion("hel", 3, help_commands);
    const auto hel_second_tab = starter::choose_tab_completion(hel_prefix, "hel", 3, state);
    CHECK(hel_second_tab.kind == starter::CompletionActionKind::list);
    CHECK(hel_second_tab.candidates == help_commands);

    const std::vector<std::string> mixed_commands = {"help", "hello", "happy"};
    const auto mixed_h_prefix = starter::resolve_completion("h", 1, mixed_commands);
    const auto mixed_h_tab = starter::choose_tab_completion(mixed_h_prefix, "h", 1, state);
    CHECK(mixed_h_tab.kind == starter::CompletionActionKind::no_change);
}

TEST_CASE("tab completion reflects starter commands subcommands and options") {
    CompletionAppFixture fixture;

    const auto root = starter::resolve_completion("", 0, fixture.app, fixture.shell_commands);
    CHECK(contains(root.candidates, "about"));
    CHECK(contains(root.candidates, "hello"));
    CHECK(contains(root.candidates, "echo"));
    CHECK(contains(root.candidates, "config"));
    CHECK(contains(root.candidates, "doctor"));
    CHECK(contains(root.candidates, "shell"));
    CHECK(contains(root.candidates, "help"));
    CHECK(contains(root.candidates, "exit"));
    CHECK(contains(root.candidates, "quit"));

    const auto config_init = starter::resolve_completion("config i", 8, fixture.app, fixture.shell_commands);
    REQUIRE(config_init.candidates.size() == 1);
    CHECK(config_init.candidates.front() == "init");

    const auto config_show = starter::resolve_completion("config s", 8, fixture.app, fixture.shell_commands);
    REQUIRE(config_show.candidates.size() == 1);
    CHECK(config_show.candidates.front() == "show");

    const auto hello_name = starter::resolve_completion("hello --n", 9, fixture.app, fixture.shell_commands);
    REQUIRE(hello_name.candidates.size() == 1);
    CHECK(hello_name.candidates.front() == "--name");

    const auto hello_short_enthusiastic =
        starter::resolve_completion("hello -e", 8, fixture.app, fixture.shell_commands);
    REQUIRE(hello_short_enthusiastic.candidates.size() == 1);
    CHECK(hello_short_enthusiastic.candidates.front() == "-e");

    const auto hello_long_enthusiastic =
        starter::resolve_completion("hello --e", 9, fixture.app, fixture.shell_commands);
    REQUIRE(hello_long_enthusiastic.candidates.size() == 1);
    CHECK(hello_long_enthusiastic.candidates.front() == "--enthusiastic");
}

TEST_CASE("tab completion keeps option candidates scoped to the active command") {
    CompletionAppFixture fixture;

    const auto root_options = starter::resolve_completion("--", 2, fixture.app, fixture.shell_commands);
    CHECK(contains(root_options.candidates, "--config"));
    CHECK(contains(root_options.candidates, "--help"));
    CHECK(contains(root_options.candidates, "--help-all"));
    CHECK(contains(root_options.candidates, "--version"));
    CHECK_FALSE(contains(root_options.candidates, "--name"));
    CHECK_FALSE(contains(root_options.candidates, "--output"));

    const auto hello_options = starter::resolve_completion("hello --", 8, fixture.app, fixture.shell_commands);
    CHECK(contains(hello_options.candidates, "--name"));
    CHECK(contains(hello_options.candidates, "--enthusiastic"));
    CHECK_FALSE(contains(hello_options.candidates, "--config"));
    CHECK_FALSE(contains(hello_options.candidates, "--output"));

    const auto config_init_options =
        starter::resolve_completion("config init --", 14, fixture.app, fixture.shell_commands);
    CHECK(contains(config_init_options.candidates, "--output"));
    CHECK_FALSE(contains(config_init_options.candidates, "--config"));
    CHECK_FALSE(contains(config_init_options.candidates, "--name"));
}

TEST_CASE("tab completion falls back to root options when prior context is malformed") {
    CompletionAppFixture fixture;
    const std::string line = "hello \"unterminated --";

    const auto completion = starter::resolve_completion(line, line.size(), fixture.app, fixture.shell_commands);

    CHECK(completion.prefix == "--");
    CHECK(contains(completion.candidates, "--config"));
    CHECK(contains(completion.candidates, "--help"));
    CHECK_FALSE(contains(completion.candidates, "--name"));
}
