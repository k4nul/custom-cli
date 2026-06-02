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
#include "starter/app/cli_app.hpp"
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

struct CompletionProbe {
    std::string line;
    std::size_t cursor = 0;
};

struct CompletionProbeRunResult {
    ApplicationRunResult run;
    std::vector<starter::CompletionResult> completions;
};

struct ApplicationArguments {
    std::vector<std::string> storage;
    std::vector<char*> argv;
};

ApplicationArguments make_application_arguments(
    const starter::ProjectInfo& project_info,
    std::vector<std::string> arguments) {
    ApplicationArguments result;
    result.storage = {project_info.binary_name};
    result.storage.insert(result.storage.end(), arguments.begin(), arguments.end());

    result.argv.reserve(result.storage.size());
    for (auto& arg : result.storage) {
        result.argv.push_back(arg.data());
    }
    return result;
}

ApplicationRunResult run_application(std::vector<std::string> arguments) {
    std::ostringstream out;
    std::ostringstream err;
    const auto project_info = starter::load_project_info();
    starter::Application application(project_info, out, err);

    auto app_args = make_application_arguments(project_info, std::move(arguments));
    const int exit_code = application.run(
        static_cast<int>(app_args.argv.size()),
        app_args.argv.data());
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

    auto app_args = make_application_arguments(project_info, std::move(arguments));
    const int exit_code = application.run(
        static_cast<int>(app_args.argv.size()),
        app_args.argv.data());
    return {exit_code, out.str(), err.str(), prompts};
}

CompletionProbeRunResult run_application_with_completion_probes(
    std::vector<std::string> arguments,
    std::vector<CompletionProbe> probes) {
    std::ostringstream out;
    std::ostringstream err;
    std::vector<std::string> prompts;
    std::vector<starter::CompletionResult> completions;
    const auto project_info = starter::load_project_info();

    starter::ShellLineReader shell_reader =
        [probes = std::move(probes), &prompts, &completions, first_prompt = true](
            const std::string& prompt,
            std::ostream& shell_out,
            const starter::CompletionProvider& completion_provider) mutable -> std::optional<std::string> {
            prompts.push_back(prompt);
            shell_out << prompt;
            if (first_prompt) {
                first_prompt = false;
                for (const auto& probe : probes) {
                    completions.push_back(completion_provider(probe.line, probe.cursor));
                }
            }
            return "exit";
        };

    starter::Application application(project_info, out, err, std::move(shell_reader));

    auto app_args = make_application_arguments(project_info, std::move(arguments));
    const int exit_code = application.run(
        static_cast<int>(app_args.argv.size()),
        app_args.argv.data());
    return {{exit_code, out.str(), err.str(), prompts}, completions};
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
        starter::configure_cli_app(
            app,
            project_info,
            config_path,
            out,
            err,
            command_executed,
            shell_requested);
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

void write_oversized_config_file(const fs::path& path) {
    constexpr std::size_t max_config_file_size = 1024U * 1024U;
    write_text_file(path, std::string(max_config_file_size + 1U, 'x'));
}

std::string quote_shell_path(const fs::path& path) {
    return "\"" + path.generic_string() + "\"";
}

std::string generated_config_template_notes() {
    return "Rename values and trim sample commands once you start customizing the starter.";
}

void check_generated_config_template(const starter::AppConfig& config) {
    const auto project_info = starter::load_project_info();
    const starter::AppConfig defaults;

    CHECK(config.prompt == project_info.prompt_label);
    CHECK(config.default_name == defaults.default_name);
    CHECK(config.enabled_commands == defaults.enabled_commands);
    CHECK(config.notes == generated_config_template_notes());
    CHECK(config.notes != defaults.notes);
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

TEST_CASE("tokenizer ignores surrounding whitespace and splits mixed whitespace") {
    CHECK(starter::tokenize_command_line("") == std::vector<std::string>{});
    CHECK(starter::tokenize_command_line(" \t\r\n  ") == std::vector<std::string>{});

    const auto tokens = starter::tokenize_command_line("  hello\tworld\n\"two words\"  ");
    const std::vector<std::string> expected = {"hello", "world", "two words"};

    CHECK(tokens == expected);
}

TEST_CASE("tokenizer handles escaped characters inside and outside quotes") {
    const auto tokens =
        starter::tokenize_command_line(R"(echo one\ two "quoted \"name\"" 'single \'quote\'' path\\tail)");
    const std::vector<std::string> expected = {
        "echo",
        "one two",
        "quoted \"name\"",
        "single 'quote'",
        "path\\tail",
    };

    CHECK(tokens == expected);
}

TEST_CASE("tokenizer combines adjacent quoted and unquoted fragments") {
    const auto tokens =
        starter::tokenize_command_line(R"(hello "Ada"'-'Lovelace unquoted" suffix" 'prefix'"suffix")");
    const std::vector<std::string> expected = {
        "hello",
        "Ada-Lovelace",
        "unquoted suffix",
        "prefixsuffix",
    };

    CHECK(tokens == expected);
}

TEST_CASE("join_tokens handles empty single and custom-delimited token lists") {
    CHECK(starter::join_tokens({}).empty());
    CHECK(starter::join_tokens({"one"}) == "one");
    CHECK(starter::join_tokens({"one", "two words", "three"}, " | ") == "one | two words | three");
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

TEST_CASE("config parsing keeps defaults for omitted fields") {
    const auto parsed = starter::parse_config(R"({"default_name":"Grace"})");
    const starter::AppConfig defaults;

    CHECK(parsed.prompt == defaults.prompt);
    CHECK(parsed.default_name == "Grace");
    CHECK(parsed.enabled_commands == defaults.enabled_commands);
    CHECK(parsed.notes == defaults.notes);
}

TEST_CASE("config parsing ignores unknown top-level fields") {
    const starter::AppConfig defaults;
    const auto parsed = starter::parse_config(
        R"({
            "prompt":"custom",
            "experimental":{"prompt":7},
            "extra_commands":["hidden"],
            "enabled_commands":["hello"]
        })");

    CHECK(parsed.prompt == "custom");
    CHECK(parsed.default_name == defaults.default_name);
    CHECK(parsed.enabled_commands == std::vector<std::string>{"hello"});
    CHECK(parsed.notes == defaults.notes);

    const auto serialized = starter::serialize_config(parsed);
    CHECK_FALSE(contains_text(serialized, "experimental"));
    CHECK_FALSE(contains_text(serialized, "extra_commands"));
    CHECK(contains_text(serialized, R"("prompt": "custom")"));
    CHECK(contains_text(serialized, R"("enabled_commands": [)"));
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

TEST_CASE("config read rejects non-regular paths") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "config-directory.json";
    fs::create_directories(config_path);
    bool caught = false;

    try {
        (void)starter::load_config_or_throw(config_path);
    } catch (const starter::ConfigReadError& error) {
        caught = true;
        CHECK(contains_text(error.what(), "config path is not a regular file"));
        CHECK(contains_text(error.what(), config_path.generic_string()));
    }

    CHECK(caught);
}

TEST_CASE("config read rejects oversized files before parsing") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "oversized.json";
    constexpr std::size_t max_config_file_size = 1024U * 1024U;
    write_text_file(config_path, std::string(max_config_file_size + 1U, 'x'));
    bool caught = false;

    try {
        (void)starter::load_config_or_throw(config_path);
    } catch (const starter::ConfigReadError& error) {
        caught = true;
        CHECK(contains_text(error.what(), "config file is too large"));
        CHECK(contains_text(error.what(), config_path.generic_string()));
        CHECK(contains_text(error.what(), std::to_string(max_config_file_size)));
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

TEST_CASE("load_config_or_default returns defaults for missing files without creating them") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "profiles" / "missing.json";
    const starter::AppConfig defaults;
    const auto result = starter::load_config_with_source(config_path);
    bool loaded_from_disk = true;

    const auto config = starter::load_config_or_default(config_path, &loaded_from_disk);

    CHECK_FALSE(result.loaded_from_disk);
    CHECK(result.config.prompt == defaults.prompt);
    CHECK(result.config.default_name == defaults.default_name);
    CHECK(result.config.enabled_commands == defaults.enabled_commands);
    CHECK(result.config.notes == defaults.notes);
    CHECK_FALSE(loaded_from_disk);
    CHECK(config.prompt == defaults.prompt);
    CHECK(config.default_name == defaults.default_name);
    CHECK(config.enabled_commands == defaults.enabled_commands);
    CHECK(config.notes == defaults.notes);
    CHECK_FALSE(fs::exists(config_path));
}

TEST_CASE("load_config_or_default marks disk-backed configs as loaded") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "custom.json";
    write_text_file(
        config_path,
        R"({
            "prompt":"ops",
            "default_name":"Ada",
            "enabled_commands":["doctor","about"],
            "notes":"loaded from disk"
        })");
    const auto result = starter::load_config_with_source(config_path);
    bool loaded_from_disk = false;

    const auto config = starter::load_config_or_default(config_path, &loaded_from_disk);
    const std::vector<std::string> expected_commands = {"doctor", "about"};

    CHECK(result.loaded_from_disk);
    CHECK(result.config.prompt == "ops");
    CHECK(result.config.default_name == "Ada");
    CHECK(result.config.enabled_commands == expected_commands);
    CHECK(result.config.notes == "loaded from disk");
    CHECK(loaded_from_disk);
    CHECK(config.prompt == "ops");
    CHECK(config.default_name == "Ada");
    CHECK(config.enabled_commands == expected_commands);
    CHECK(config.notes == "loaded from disk");
}

TEST_CASE("write_config_template creates parent directories for nested config paths") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "profiles" / "team" / "starter.json";

    starter::AppConfig config;
    config.prompt = "team";
    config.default_name = "Grace";
    config.enabled_commands = {"hello", "doctor"};
    config.notes = "nested config";

    starter::write_config_template(config_path, config);

    const std::vector<std::string> expected_commands = {"hello", "doctor"};
    CHECK(fs::is_directory(config_path.parent_path()));
    const auto loaded = starter::load_config_or_throw(config_path);
    CHECK(loaded.prompt == "team");
    CHECK(loaded.default_name == "Grace");
    CHECK(loaded.enabled_commands == expected_commands);
    CHECK(loaded.notes == "nested config");
}

TEST_CASE("write_config_template truncates stale config contents") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "starter.json";
    write_text_file(
        config_path,
        "{\n"
        "  \"prompt\": \"stale\",\n"
        "  \"default_name\": \"stale\",\n"
        "  \"enabled_commands\": [\"stale\"],\n"
        "  \"notes\": \"stale\",\n"
        "  \"legacy\": true\n"
        "}\n");

    starter::AppConfig replacement;
    replacement.prompt = "fresh";
    replacement.default_name = "Ada";
    replacement.enabled_commands = {"hello"};
    replacement.notes = "current";

    starter::write_config_template(config_path, replacement);

    const auto text = [&]() {
        std::ifstream input(config_path);
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }();
    const auto loaded = starter::load_config_or_throw(config_path);
    const std::vector<std::string> expected_commands = {"hello"};

    CHECK(loaded.prompt == "fresh");
    CHECK(loaded.default_name == "Ada");
    CHECK(loaded.enabled_commands == expected_commands);
    CHECK(loaded.notes == "current");
    CHECK_FALSE(contains_text(text, "legacy"));
    CHECK_FALSE(contains_text(text, "stale"));
}

TEST_CASE("application accepts hello subcommand options from argv order") {
    const auto result = run_application({"hello", "--name", "starter user"});

    CHECK(result.exit_code == 0);
    CHECK(result.out == "Hello, starter user.\n");
    CHECK(result.err.empty());
}

TEST_CASE("application supports enthusiastic hello flag") {
    const auto result = run_application({"hello", "--enthusiastic", "--name", "Ada"});

    CHECK(result.exit_code == 0);
    CHECK(result.out == "Hello, Ada!\n");
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

TEST_CASE("interactive shell exits cleanly on EOF without an explicit exit command") {
    TemporaryDirectory temporary_directory;
    const CurrentPathGuard current_path(temporary_directory.path());

    const auto result = run_application_with_scripted_shell({}, {});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, "Interactive mode. Type 'help' to inspect commands or 'exit' to quit.\n"));
    CHECK(result.err.empty());
    CHECK(result.prompts == std::vector<std::string>{"starter> "});
}

TEST_CASE("interactive shell reports malformed startup config before prompting") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "bad.json";
    write_text_file(config_path, R"({"default_name":)");

    const auto result = run_application_with_scripted_shell(
        {"--config", config_path.string(), "shell"},
        {"hello --name Ada"});

    CHECK(result.exit_code == starter::to_int(starter::ExitCode::config_error));
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "parse error"));
    CHECK(result.prompts.empty());
}

TEST_CASE("interactive shell falls back to project prompt when disk prompt is empty") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "custom.json";

    starter::AppConfig config;
    config.prompt = "";
    config.default_name = "Grace";
    starter::write_config_template(config_path, config);

    const auto result = run_application_with_scripted_shell(
        {"--config", config_path.string(), "shell"},
        {"hello", "exit"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, "Hello, Grace.\n"));
    CHECK_FALSE(contains_text(result.out, "Using built-in defaults"));
    CHECK(result.err.empty());
    CHECK(result.prompts == std::vector<std::string>{"starter> ", "starter> "});
}

TEST_CASE("interactive shell rejects wrong-type startup config before prompting") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "wrong-type.json";
    write_text_file(config_path, R"({"prompt":7})");

    const auto result = run_application_with_scripted_shell(
        {"--config", config_path.string(), "shell"},
        {"exit"});

    CHECK(result.exit_code == starter::to_int(starter::ExitCode::config_error));
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "type must be string"));
    CHECK(result.prompts.empty());
}

TEST_CASE("interactive shell reports non-regular startup config before prompting") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "config-directory.json";
    fs::create_directories(config_path);

    const auto result = run_application_with_scripted_shell(
        {"--config", config_path.string(), "shell"},
        {"exit"});

    CHECK(result.exit_code == starter::to_int(starter::ExitCode::config_error));
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "config path is not a regular file"));
    CHECK(contains_text(result.err, config_path.generic_string()));
    CHECK(result.prompts.empty());
}

TEST_CASE("interactive shell ignores blank and whitespace-only input before dispatching commands") {
    TemporaryDirectory temporary_directory;
    const CurrentPathGuard current_path(temporary_directory.path());

    const auto result = run_application_with_scripted_shell({}, {"", " \t  ", "hello --name Ada"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, "Hello, Ada.\n"));
    CHECK(result.err.empty());
    CHECK(result.prompts == std::vector<std::string>{"starter> ", "starter> ", "starter> ", "starter> "});
}

TEST_CASE("interactive shell exit and quit commands stop the session without dispatch errors") {
    TemporaryDirectory temporary_directory;
    const CurrentPathGuard current_path(temporary_directory.path());

    for (const auto& command : std::vector<std::string>{"exit", "quit"}) {
        const auto result = run_application_with_scripted_shell({}, {command});

        CHECK(result.exit_code == 0);
        CHECK_FALSE(contains_text(result.out, "Run with --help"));
        CHECK_FALSE(contains_text(result.err, "command finished with exit code"));
        CHECK(result.err.empty());
        CHECK(result.prompts == std::vector<std::string>{"starter> "});
    }
}

TEST_CASE("interactive shell EOF after a command error still exits the shell successfully") {
    TemporaryDirectory temporary_directory;
    const CurrentPathGuard current_path(temporary_directory.path());

    const auto result = run_application_with_scripted_shell({}, {"missing-command"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.err, "missing-command"));
    CHECK(contains_text(result.err, "command finished with exit code "));
    CHECK(result.prompts == std::vector<std::string>{"starter> ", "starter> "});
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

TEST_CASE("interactive shell config show uses startup config path") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "profiles" / "shell.json";
    starter::AppConfig config;
    config.prompt = "custom";
    config.default_name = "Grace";
    config.enabled_commands = {"hello", "config"};
    config.notes = "shell config";
    starter::write_config_template(config_path, config);

    const auto result = run_application_with_scripted_shell(
        {"--config", config_path.string(), "shell"},
        {"config show", "exit"});

    CHECK(result.exit_code == 0);
    CHECK_FALSE(contains_text(result.out, "Using built-in defaults"));
    CHECK(contains_text(result.out, "Config path: " + config_path.generic_string() + '\n'));
    CHECK(contains_text(result.out, "Source: disk\n"));
    CHECK(contains_text(result.out, "Prompt: custom\n"));
    CHECK(contains_text(result.out, "Default name: Grace\n"));
    CHECK(contains_text(result.out, "Enabled commands: hello, config\n"));
    CHECK(contains_text(result.out, "Notes: shell config\n"));
    CHECK(result.err.empty());
    CHECK(result.prompts == std::vector<std::string>{"custom> ", "custom> "});
}

TEST_CASE("interactive shell config init defaults to startup config path") {
    TemporaryDirectory temporary_directory;
    const CurrentPathGuard current_path(temporary_directory.path());
    const auto config_path = temporary_directory.path() / "profiles" / "generated.json";
    const auto default_config_path = temporary_directory.path() / "config" / "cli-starter.json";

    const auto result = run_application_with_scripted_shell(
        {"--config", config_path.string(), "shell"},
        {"config init", "config show", "exit"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, "Using built-in defaults until " + config_path.generic_string() + " exists.\n"));
    CHECK(contains_text(result.out, "Wrote config template to " + config_path.generic_string() + '\n'));
    CHECK(contains_text(result.out, "Config path: " + config_path.generic_string() + '\n'));
    CHECK(contains_text(result.out, "Source: disk\n"));
    CHECK(result.err.empty());
    CHECK(result.prompts == std::vector<std::string>{"starter> ", "starter> ", "starter> "});
    CHECK(fs::exists(config_path));
    CHECK_FALSE(fs::exists(default_config_path));
}

TEST_CASE("interactive shell config init explicit output keeps startup config path active") {
    TemporaryDirectory temporary_directory;
    const CurrentPathGuard current_path(temporary_directory.path());
    const auto config_path = temporary_directory.path() / "profiles" / "active.json";
    const auto output_path = temporary_directory.path() / "generated" / "explicit.json";

    const auto result = run_application_with_scripted_shell(
        {"--config", config_path.string(), "shell"},
        {"config init --output " + output_path.string(), "config show", "exit"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, "Wrote config template to " + output_path.generic_string() + '\n'));
    CHECK(contains_text(result.out, "Config path: " + config_path.generic_string() + '\n'));
    CHECK(contains_text(result.out, "Source: built-in defaults\n"));
    CHECK(result.err.empty());
    CHECK(result.prompts == std::vector<std::string>{"starter> ", "starter> ", "starter> "});
    CHECK(fs::exists(output_path));
    CHECK_FALSE(fs::exists(config_path));
}

TEST_CASE("interactive shell applies inline config overrides to one command") {
    TemporaryDirectory temporary_directory;
    const auto session_config_path = temporary_directory.path() / "session.json";
    const auto alternate_config_path = temporary_directory.path() / "alternate.json";

    starter::AppConfig session_config;
    session_config.prompt = "session";
    session_config.default_name = "Grace";
    starter::write_config_template(session_config_path, session_config);

    starter::AppConfig alternate_config;
    alternate_config.prompt = "alternate";
    alternate_config.default_name = "Ada";
    starter::write_config_template(alternate_config_path, alternate_config);

    const auto result = run_application_with_scripted_shell(
        {"--config", session_config_path.string(), "shell"},
        {"--config " + quote_shell_path(alternate_config_path) + " hello", "exit"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, "Hello, Ada.\n"));
    CHECK_FALSE(contains_text(result.out, "Using built-in defaults"));
    CHECK(result.err.empty());
    CHECK(result.prompts == std::vector<std::string>{"session> ", "session> "});
}

TEST_CASE("interactive shell keeps startup config after inline config overrides") {
    TemporaryDirectory temporary_directory;
    const auto session_config_path = temporary_directory.path() / "session.json";
    const auto alternate_config_path = temporary_directory.path() / "alternate.json";

    starter::AppConfig session_config;
    session_config.prompt = "session";
    session_config.default_name = "Grace";
    starter::write_config_template(session_config_path, session_config);

    starter::AppConfig alternate_config;
    alternate_config.prompt = "alternate";
    alternate_config.default_name = "Ada";
    starter::write_config_template(alternate_config_path, alternate_config);

    const auto result = run_application_with_scripted_shell(
        {"--config", session_config_path.string(), "shell"},
        {
            "--config " + quote_shell_path(alternate_config_path) + " config show",
            "config show",
            "hello",
            "exit",
        });

    const auto alternate_report = "Config path: " + alternate_config_path.generic_string() + '\n';
    const auto session_report = "Config path: " + session_config_path.generic_string() + '\n';
    const auto alternate_position = result.out.find(alternate_report);
    const auto session_position = result.out.find(session_report);

    CHECK(result.exit_code == 0);
    REQUIRE(alternate_position != std::string::npos);
    REQUIRE(session_position != std::string::npos);
    CHECK(alternate_position < session_position);
    CHECK(contains_text(result.out, "Prompt: alternate\n"));
    CHECK(contains_text(result.out, "Default name: Ada\n"));
    CHECK(contains_text(result.out, "Prompt: session\n"));
    CHECK(contains_text(result.out, "Default name: Grace\n"));
    CHECK(contains_text(result.out, "Hello, Grace.\n"));
    CHECK_FALSE(contains_text(result.out, "Using built-in defaults"));
    CHECK(result.err.empty());
    CHECK(result.prompts == std::vector<std::string>{"session> ", "session> ", "session> ", "session> "});
}

TEST_CASE("interactive shell recovers from inline config parse errors") {
    TemporaryDirectory temporary_directory;
    const auto session_config_path = temporary_directory.path() / "session.json";
    const auto bad_config_path = temporary_directory.path() / "bad.json";

    starter::AppConfig session_config;
    session_config.prompt = "session";
    session_config.default_name = "Grace";
    starter::write_config_template(session_config_path, session_config);
    write_text_file(bad_config_path, R"({"default_name":)");

    const auto result = run_application_with_scripted_shell(
        {"--config", session_config_path.string(), "shell"},
        {"--config " + quote_shell_path(bad_config_path) + " hello", "hello", "exit"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, "Hello, Grace.\n"));
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "parse error"));
    CHECK(contains_text(
        result.err,
        "command finished with exit code " + std::to_string(starter::to_int(starter::ExitCode::config_error))));
    CHECK(result.prompts == std::vector<std::string>{"session> ", "session> ", "session> "});
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

TEST_CASE("interactive shell exposes command completion through line reader") {
    TemporaryDirectory temporary_directory;
    const CurrentPathGuard current_path(temporary_directory.path());

    const auto result = run_application_with_completion_probes(
        {},
        {
            {"", 0},
            {"he", std::string("he").size()},
            {"shell ", std::string("shell ").size()},
        });

    CHECK(result.run.exit_code == 0);
    CHECK(result.run.err.empty());
    CHECK(result.run.prompts == std::vector<std::string>{"starter> "});
    REQUIRE(result.completions.size() == 3);

    const auto& root = result.completions[0];
    CHECK(root.prefix.empty());
    CHECK(root.replace_begin == 0);
    CHECK(root.replace_end == 0);
    CHECK(contains(root.candidates, "about"));
    CHECK(contains(root.candidates, "hello"));
    CHECK(contains(root.candidates, "echo"));
    CHECK(contains(root.candidates, "config"));
    CHECK(contains(root.candidates, "doctor"));
    CHECK(contains(root.candidates, "shell"));
    CHECK(contains(root.candidates, "help"));
    CHECK(contains(root.candidates, "exit"));
    CHECK(contains(root.candidates, "quit"));

    const auto& help_or_hello = result.completions[1];
    CHECK(help_or_hello.prefix == "he");
    CHECK(help_or_hello.replace_begin == 0);
    CHECK(help_or_hello.replace_end == std::string("he").size());
    CHECK(help_or_hello.candidates == std::vector<std::string>{"hello", "help"});

    const auto& shell_context = result.completions[2];
    CHECK(shell_context.prefix.empty());
    CHECK(shell_context.replace_begin == std::string("shell ").size());
    CHECK(shell_context.replace_end == std::string("shell ").size());
    CHECK(shell_context.candidates.empty());
}

TEST_CASE("interactive shell scopes completion probes through application command context") {
    TemporaryDirectory temporary_directory;
    const CurrentPathGuard current_path(temporary_directory.path());
    const std::string config_line = "config ";
    const std::string hello_options_line = "hello --";
    const std::string config_init_options_line = "config init --";
    const std::string midline = "config i --later";
    const auto midline_cursor = std::string("config i").size();

    const auto result = run_application_with_completion_probes(
        {},
        {
            {config_line, config_line.size()},
            {hello_options_line, hello_options_line.size()},
            {config_init_options_line, config_init_options_line.size()},
            {midline, midline_cursor},
        });

    CHECK(result.run.exit_code == 0);
    CHECK(result.run.err.empty());
    REQUIRE(result.completions.size() == 4);

    const auto& config_subcommands = result.completions[0];
    CHECK(config_subcommands.prefix.empty());
    CHECK(config_subcommands.candidates == std::vector<std::string>{"init", "show"});
    CHECK_FALSE(contains(config_subcommands.candidates, "hello"));
    CHECK_FALSE(contains(config_subcommands.candidates, "help"));

    const auto& hello_options = result.completions[1];
    CHECK(contains(hello_options.candidates, "--name"));
    CHECK(contains(hello_options.candidates, "--enthusiastic"));
    CHECK_FALSE(contains(hello_options.candidates, "--config"));
    CHECK_FALSE(contains(hello_options.candidates, "--output"));

    const auto& config_init_options = result.completions[2];
    CHECK(contains(config_init_options.candidates, "--output"));
    CHECK(contains(config_init_options.candidates, "--help"));
    CHECK(contains(config_init_options.candidates, "--help-all"));
    CHECK_FALSE(contains(config_init_options.candidates, "--config"));
    CHECK_FALSE(contains(config_init_options.candidates, "--name"));

    const auto& cursor_scoped_completion = result.completions[3];
    CHECK(cursor_scoped_completion.prefix == "i");
    CHECK(cursor_scoped_completion.replace_begin == std::string("config ").size());
    CHECK(cursor_scoped_completion.replace_end == midline_cursor);
    CHECK(cursor_scoped_completion.candidates == std::vector<std::string>{"init"});
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

TEST_CASE("config-backed hello supports enthusiastic default name") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "custom.json";

    starter::AppConfig config;
    config.default_name = "Grace";
    starter::write_config_template(config_path, config);

    const auto result = run_application({"--config", config_path.string(), "hello", "-e"});

    CHECK(result.exit_code == 0);
    CHECK(result.out == "Hello, Grace!\n");
    CHECK(result.err.empty());
}

TEST_CASE("explicit hello name overrides disk config default") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "custom.json";

    starter::AppConfig config;
    config.default_name = "Grace";
    starter::write_config_template(config_path, config);

    const auto result = run_application({"--config", config_path.string(), "hello", "--name", "Ada"});

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

TEST_CASE("explicit hello name suppresses missing config guidance") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "missing.json";

    const auto result = run_application({"--config", config_path.string(), "hello", "--name", "Ada"});

    CHECK(result.exit_code == 0);
    CHECK(result.out == "Hello, Ada.\n");
    CHECK(result.err.empty());
    CHECK_FALSE(fs::exists(config_path));
}

TEST_CASE("enabled commands remains informational for config-backed hello") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "custom.json";

    starter::AppConfig config;
    config.default_name = "Ada";
    config.enabled_commands = {"about", "doctor"};
    starter::write_config_template(config_path, config);

    const auto result = run_application({"--config", config_path.string(), "hello"});

    CHECK(result.exit_code == 0);
    CHECK(result.out == "Hello, Ada.\n");
    CHECK(result.err.empty());
}

TEST_CASE("enabled commands does not gate commands that ignore config") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "custom.json";

    starter::AppConfig config;
    config.enabled_commands = {"hello"};
    starter::write_config_template(config_path, config);

    const auto result = run_application({"--config", config_path.string(), "echo", "--uppercase", "still", "runs"});

    CHECK(result.exit_code == 0);
    CHECK(result.out == "STILL RUNS\n");
    CHECK(result.err.empty());
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
    check_generated_config_template(config);
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

    const auto config = starter::load_config_or_throw(output_path);
    check_generated_config_template(config);
}

TEST_CASE("config init generated template is immediately consumable by config show") {
    TemporaryDirectory temporary_directory;
    const CurrentPathGuard current_path(temporary_directory.path());
    const auto config_path = temporary_directory.path() / "profiles" / "generated.json";
    const auto project_info = starter::load_project_info();
    const starter::AppConfig defaults;

    const auto init_result = run_application({"--config", config_path.string(), "config", "init"});
    REQUIRE(init_result.exit_code == 0);
    CHECK(init_result.out == "Wrote config template to " + config_path.generic_string() + '\n');
    CHECK(init_result.err.empty());

    const auto show_result = run_application({"--config", config_path.string(), "config", "show"});

    CHECK(show_result.exit_code == 0);
    CHECK(contains_text(show_result.out, "Config path: " + config_path.generic_string() + '\n'));
    CHECK(contains_text(show_result.out, "Source: disk\n"));
    CHECK(contains_text(show_result.out, "Prompt: " + project_info.prompt_label + '\n'));
    CHECK(contains_text(show_result.out, "Default name: " + defaults.default_name + '\n'));
    CHECK(contains_text(
        show_result.out,
        "Enabled commands: " + starter::join_tokens(defaults.enabled_commands, ", ") + '\n'));
    CHECK(contains_text(show_result.out, "Notes: " + generated_config_template_notes() + '\n'));
    CHECK(show_result.err.empty());
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

TEST_CASE("config show reports disk enabled command list verbatim") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "custom.json";

    starter::AppConfig config;
    config.prompt = "project";
    config.default_name = "Grace";
    config.enabled_commands = {"doctor", "about"};
    config.notes = "custom notes";
    starter::write_config_template(config_path, config);

    const auto result = run_application({"--config", config_path.string(), "config", "show"});
    std::ostringstream expected;
    expected << "Config path: " << config_path.generic_string() << '\n';
    expected << "Source: disk\n";
    expected << "Prompt: project\n";
    expected << "Default name: Grace\n";
    expected << "Enabled commands: doctor, about\n";
    expected << "Notes: custom notes\n";

    CHECK(result.exit_code == 0);
    CHECK(result.out == expected.str());
    CHECK(result.err.empty());
}

TEST_CASE("config show omits unknown disk config fields") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "custom.json";
    write_text_file(
        config_path,
        R"({
            "prompt":"project",
            "default_name":"Grace",
            "unknown":"ignored",
            "future":{"nested":true}
        })");

    const auto result = run_application({"--config", config_path.string(), "config", "show"});

    CHECK(result.exit_code == 0);
    CHECK(contains_text(result.out, "Source: disk\n"));
    CHECK(contains_text(result.out, "Prompt: project\n"));
    CHECK(contains_text(result.out, "Default name: Grace\n"));
    CHECK_FALSE(contains_text(result.out, "unknown"));
    CHECK_FALSE(contains_text(result.out, "future"));
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

TEST_CASE("config show reports non-regular disk config through stderr") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "config-directory.json";
    fs::create_directories(config_path);

    const auto result = run_application({"--config", config_path.string(), "config", "show"});

    CHECK(result.exit_code == starter::to_int(starter::ExitCode::config_error));
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "config path is not a regular file"));
    CHECK(contains_text(result.err, config_path.generic_string()));
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

TEST_CASE("config-backed hello reports oversized disk config before explicit name") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "oversized.json";
    write_oversized_config_file(config_path);

    const auto result = run_application({"--config", config_path.string(), "hello", "--name", "Ada"});

    CHECK(result.exit_code == starter::to_int(starter::ExitCode::config_error));
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "config file is too large"));
    CHECK(contains_text(result.err, config_path.generic_string()));
    CHECK(contains_text(result.err, "1048576"));
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

TEST_CASE("doctor reports oversized disk config through stderr") {
    TemporaryDirectory temporary_directory;
    create_recommended_starter_layout(temporary_directory.path());
    const CurrentPathGuard current_path(temporary_directory.path());
    const auto config_path = temporary_directory.path() / "config" / "oversized.json";
    write_oversized_config_file(config_path);

    const auto result = run_application({"--config", config_path.string(), "doctor"});

    CHECK(result.exit_code == starter::to_int(starter::ExitCode::config_error));
    CHECK(contains_text(result.out, "[ok] source directory: src\n"));
    CHECK(contains_text(result.out, "[ok] third-party directory: third_party\n"));
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "config file is too large"));
    CHECK(contains_text(result.err, config_path.generic_string()));
    CHECK_FALSE(contains_text(result.out, "[info] prompt:"));
    CHECK_FALSE(contains_text(result.out, "Starter layout looks healthy."));
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

TEST_CASE("tab completion includes visible subcommand aliases once") {
    CLI::App app{"completion aliases"};
    app.add_subcommand("deploy", "Deploy artifacts.")->alias("dp");
    app.add_subcommand("doctor", "Inspect the environment.")->alias("diag");

    const auto root = starter::resolve_completion("", 0, app, {"dp", "help"});

    const std::vector<std::string> expected = {"deploy", "dp", "doctor", "diag", "help"};
    CHECK(root.candidates == expected);
    CHECK(std::count(root.candidates.begin(), root.candidates.end(), "dp") == 1);

    const auto d_prefix = starter::resolve_completion("d", 1, app, {"dp", "help"});
    CHECK(d_prefix.candidates == std::vector<std::string>{"deploy", "dp", "doctor", "diag"});
}

TEST_CASE("tab completion resolves subcommand aliases as command contexts") {
    CLI::App app{"completion alias contexts"};
    std::string root_profile;
    app.add_option("--root-profile", root_profile, "Root profile path.");

    auto* config_command = app.add_subcommand("config", "Manage configuration.");
    config_command->alias("cfg");
    std::string profile;
    config_command->add_option("--profile", profile, "Config profile name.");
    config_command->add_subcommand("init", "Initialize configuration.");

    const std::string subcommand_line = "cfg i";
    const auto subcommand_completion =
        starter::resolve_completion(subcommand_line, subcommand_line.size(), app, {});
    CHECK(subcommand_completion.prefix == "i");
    CHECK(subcommand_completion.replace_begin == std::string("cfg ").size());
    CHECK(subcommand_completion.replace_end == subcommand_line.size());
    CHECK(subcommand_completion.candidates == std::vector<std::string>{"init"});

    const std::string option_line = "cfg --p";
    const auto option_completion = starter::resolve_completion(option_line, option_line.size(), app, {});
    CHECK(option_completion.prefix == "--p");
    CHECK(contains(option_completion.candidates, "--profile"));
    CHECK_FALSE(contains(option_completion.candidates, "--root-profile"));
}

TEST_CASE("tab completion omits hidden silent and disabled subcommands") {
    CLI::App app{"completion visibility"};
    app.add_subcommand("visible", "Visible command.");
    app.add_subcommand("hidden", "Hidden command.")->group("");
    app.add_subcommand("silent", "Silent command.")->silent();
    app.add_subcommand("disabled", "Disabled command.")->disabled();

    const auto root = starter::resolve_completion("", 0, app, {});

    CHECK(root.candidates == std::vector<std::string>{"visible"});
    CHECK(starter::resolve_completion("h", 1, app, {}).candidates.empty());
    CHECK(starter::resolve_completion("s", 1, app, {}).candidates.empty());
    CHECK(starter::resolve_completion("d", 1, app, {}).candidates.empty());
}

TEST_CASE("tab completion omits hidden and positional options") {
    CLI::App app{"completion option visibility"};
    std::string visible;
    std::string hidden;
    std::string positional;
    bool verbose = false;

    app.add_option("--visible", visible, "Visible option.");
    app.add_option("--hidden", hidden, "Hidden option.")->group("");
    app.add_option("positional", positional, "Positional value.");
    app.add_flag("--verbose", verbose, "Verbose output.");

    const auto options = starter::resolve_completion("--", 2, app, {});

    CHECK(contains(options.candidates, "--visible"));
    CHECK(contains(options.candidates, "--verbose"));
    CHECK_FALSE(contains(options.candidates, "--hidden"));
    CHECK_FALSE(contains(options.candidates, "positional"));
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

TEST_CASE("tab completion keeps command option context after option values") {
    CompletionAppFixture fixture;

    const std::string hello_line = "hello --name Ada --e";
    const auto hello_options =
        starter::resolve_completion(hello_line, hello_line.size(), fixture.app, fixture.shell_commands);
    CHECK(hello_options.prefix == "--e");
    CHECK(hello_options.replace_begin == std::string("hello --name Ada ").size());
    CHECK(hello_options.replace_end == hello_line.size());
    CHECK(contains(hello_options.candidates, "--enthusiastic"));
    CHECK_FALSE(contains(hello_options.candidates, "--config"));
    CHECK_FALSE(contains(hello_options.candidates, "--output"));

    const std::string config_help_line = "config init --output generated.json --h";
    const auto config_help_options =
        starter::resolve_completion(config_help_line, config_help_line.size(), fixture.app, fixture.shell_commands);
    CHECK(config_help_options.prefix == "--h");
    CHECK(config_help_options.replace_begin == std::string("config init --output generated.json ").size());
    CHECK(config_help_options.replace_end == config_help_line.size());
    CHECK(contains(config_help_options.candidates, "--help"));
    CHECK(contains(config_help_options.candidates, "--help-all"));
    CHECK_FALSE(contains(config_help_options.candidates, "--config"));
    CHECK_FALSE(contains(config_help_options.candidates, "--name"));

    const std::string quoted_output_line = R"(config init --output "path with spaces.json" --o)";
    const auto quoted_output_options =
        starter::resolve_completion(quoted_output_line, quoted_output_line.size(), fixture.app, fixture.shell_commands);
    CHECK(quoted_output_options.prefix == "--o");
    CHECK(quoted_output_options.replace_begin == quoted_output_line.rfind("--o"));
    CHECK(quoted_output_options.replace_end == quoted_output_line.size());
    REQUIRE(quoted_output_options.candidates.size() == 1);
    CHECK(quoted_output_options.candidates.front() == "--output");
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

TEST_CASE("tab completion uses cursor position for replacement ranges") {
    CompletionAppFixture fixture;
    const std::string line = "config i --later";
    const auto cursor = std::string("config i").size();

    const auto completion = starter::resolve_completion(line, cursor, fixture.app, fixture.shell_commands);

    CHECK(completion.prefix == "i");
    CHECK(completion.replace_begin == std::string("config ").size());
    CHECK(completion.replace_end == cursor);
    REQUIRE(completion.candidates.size() == 1);
    CHECK(completion.candidates.front() == "init");

    starter::TabCompletionState state;
    const auto action = starter::choose_tab_completion(completion, line, cursor, state);
    CHECK(action.kind == starter::CompletionActionKind::replace);
    CHECK(action.replacement == "init");
    CHECK(action.replace_begin == std::string("config ").size());
    CHECK(action.replace_end == cursor);
    CHECK_FALSE(state.primed);
}

TEST_CASE("tab completion clamps cursor beyond line length") {
    const std::vector<std::string> commands = {"help", "hello", "doctor"};

    const auto completion = starter::resolve_completion("hel", 99, commands);

    CHECK(completion.prefix == "hel");
    CHECK(completion.replace_begin == 0);
    CHECK(completion.replace_end == std::string("hel").size());
    CHECK(completion.candidates == std::vector<std::string>{"help", "hello"});
}

TEST_CASE("tab completion uses token boundaries after leading and repeated whitespace") {
    CompletionAppFixture fixture;

    const auto root_completion =
        starter::resolve_completion("  h", std::string("  h").size(), fixture.app, fixture.shell_commands);
    CHECK(root_completion.prefix == "h");
    CHECK(root_completion.replace_begin == 2);
    CHECK(root_completion.replace_end == std::string("  h").size());
    CHECK(contains(root_completion.candidates, "hello"));
    CHECK(contains(root_completion.candidates, "help"));
    CHECK_FALSE(contains(root_completion.candidates, "config"));

    const auto config_completion = starter::resolve_completion(
        "config   s",
        std::string("config   s").size(),
        fixture.app,
        fixture.shell_commands);
    CHECK(config_completion.prefix == "s");
    CHECK(config_completion.replace_begin == std::string("config   ").size());
    CHECK(config_completion.replace_end == std::string("config   s").size());
    CHECK(config_completion.candidates == std::vector<std::string>{"show"});
}

TEST_CASE("tab completion keeps no-match requests as no-change") {
    starter::TabCompletionState state;
    const std::vector<std::string> commands = {"help", "hello"};

    const auto completion = starter::resolve_completion("z", 1, commands);
    const auto first_action = starter::choose_tab_completion(completion, "z", 1, state);
    CHECK(first_action.kind == starter::CompletionActionKind::no_change);
    CHECK(first_action.candidates.empty());
    CHECK(state.primed);

    const auto second_action = starter::choose_tab_completion(completion, "z", 1, state);
    CHECK(second_action.kind == starter::CompletionActionKind::no_change);
    CHECK(second_action.candidates.empty());
    CHECK(state.primed);
    CHECK(state.line == "z");
    CHECK(state.cursor == 1);
}

TEST_CASE("tab completion treats edited input as a new primed request") {
    starter::TabCompletionState state;
    const std::vector<std::string> commands = {"help", "hello"};

    const auto first_completion = starter::resolve_completion("hel", 3, commands);
    const auto first_action = starter::choose_tab_completion(first_completion, "hel", 3, state);
    CHECK(first_action.kind == starter::CompletionActionKind::no_change);
    CHECK(state.primed);
    CHECK(state.line == "hel");

    const auto edited_completion = starter::resolve_completion("helx", 4, commands);
    const auto edited_action = starter::choose_tab_completion(edited_completion, "helx", 4, state);
    CHECK(edited_action.kind == starter::CompletionActionKind::no_change);
    CHECK(edited_action.candidates.empty());
    CHECK(state.primed);
    CHECK(state.line == "helx");
    CHECK(state.cursor == 4);
    CHECK(state.prefix == "helx");
}

TEST_CASE("tab completion offers subcommands after a trailing space") {
    CompletionAppFixture fixture;
    const std::string line = "config ";

    const auto completion = starter::resolve_completion(line, line.size(), fixture.app, fixture.shell_commands);

    CHECK(completion.prefix.empty());
    CHECK(completion.replace_begin == line.size());
    CHECK(completion.replace_end == line.size());
    CHECK(contains(completion.candidates, "init"));
    CHECK(contains(completion.candidates, "show"));
    CHECK_FALSE(contains(completion.candidates, "hello"));
    CHECK_FALSE(contains(completion.candidates, "help"));
}

TEST_CASE("tab completion tracks shared-prefix edits in primed state") {
    starter::TabCompletionState state;
    const std::vector<std::string> commands = {"help", "hello"};
    const std::string line = "h trailing";

    const auto completion = starter::resolve_completion(line, 1, commands);
    const auto action = starter::choose_tab_completion(completion, line, 1, state);

    CHECK(action.kind == starter::CompletionActionKind::replace);
    CHECK(action.replacement == "hel");
    CHECK(action.replace_begin == 0);
    CHECK(action.replace_end == 1);
    CHECK(state.primed);
    CHECK(state.line == "hel trailing");
    CHECK(state.cursor == 3);
    CHECK(state.replace_begin == 0);
    CHECK(state.replace_end == 3);
    CHECK(state.prefix == "hel");
}

TEST_CASE("tab completion resets primed state after replace and list actions") {
    const std::vector<std::string> commands = {"help", "hello"};
    starter::TabCompletionState state;

    const auto h_completion = starter::resolve_completion("h", 1, commands);
    const auto h_action = starter::choose_tab_completion(h_completion, "h", 1, state);
    CHECK(h_action.kind == starter::CompletionActionKind::replace);
    CHECK(state.primed);

    const auto help_completion = starter::resolve_completion("help", 4, commands);
    const auto help_action = starter::choose_tab_completion(help_completion, "help", 4, state);
    CHECK(help_action.kind == starter::CompletionActionKind::replace);
    CHECK_FALSE(state.primed);
    CHECK(state.line.empty());
    CHECK(state.cursor == 0);

    const auto hel_completion = starter::resolve_completion("hel", 3, commands);
    const auto first_hel_action = starter::choose_tab_completion(hel_completion, "hel", 3, state);
    CHECK(first_hel_action.kind == starter::CompletionActionKind::no_change);
    CHECK(state.primed);

    const auto second_hel_action = starter::choose_tab_completion(hel_completion, "hel", 3, state);
    CHECK(second_hel_action.kind == starter::CompletionActionKind::list);
    CHECK_FALSE(state.primed);
    CHECK(state.line.empty());
    CHECK(state.cursor == 0);
}
