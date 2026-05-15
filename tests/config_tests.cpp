#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <CLI/CLI.hpp>

#include "starter/app/application.hpp"
#include "starter/commands/registrars.hpp"
#include "starter/core/completion.hpp"
#include "starter/core/config.hpp"
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
    return {exit_code, out.str(), err.str()};
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

TEST_CASE("application routes parse errors through configured stream") {
    const auto result = run_application({"missing-command"});

    CHECK(result.exit_code != 0);
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "missing-command"));
    CHECK(contains_text(result.err, "Run with --help"));
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

    CHECK(result.exit_code != 0);
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "parse error"));
}

TEST_CASE("config show reports wrong-type disk config through stderr") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "wrong-type.json";
    write_text_file(config_path, R"({"enabled_commands":"hello"})");

    const auto result = run_application({"--config", config_path.string(), "config", "show"});

    CHECK(result.exit_code != 0);
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "type must be array"));
}

TEST_CASE("config-backed hello reports malformed disk config through stderr") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "bad.json";
    write_text_file(config_path, R"({"default_name":)");

    const auto result = run_application({"--config", config_path.string(), "hello"});

    CHECK(result.exit_code != 0);
    CHECK(result.out.empty());
    CHECK(contains_text(result.err, "error: "));
    CHECK(contains_text(result.err, "parse error"));
}

TEST_CASE("config-backed hello reports wrong-type disk config through stderr") {
    TemporaryDirectory temporary_directory;
    const auto config_path = temporary_directory.path() / "wrong-type.json";
    write_text_file(config_path, R"({"default_name":42})");

    const auto result = run_application({"--config", config_path.string(), "hello"});

    CHECK(result.exit_code != 0);
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
    std::ostringstream out;
    std::ostringstream err;
    const auto project_info = starter::load_project_info();
    std::string config_path = "cli-starter.json";
    bool command_executed = false;
    bool shell_requested = false;
    CLI::App app(project_info.display_name + " - generic C++ CLI starter");
    configure_starter_app(app, project_info, config_path, out, err, command_executed, shell_requested);
    const std::vector<std::string> shell_commands = {"help", "exit", "quit"};

    const auto root = starter::resolve_completion("", 0, app, shell_commands);
    CHECK(contains(root.candidates, "about"));
    CHECK(contains(root.candidates, "hello"));
    CHECK(contains(root.candidates, "echo"));
    CHECK(contains(root.candidates, "config"));
    CHECK(contains(root.candidates, "doctor"));
    CHECK(contains(root.candidates, "shell"));
    CHECK(contains(root.candidates, "help"));
    CHECK(contains(root.candidates, "exit"));
    CHECK(contains(root.candidates, "quit"));

    const auto config_init = starter::resolve_completion("config i", 8, app, shell_commands);
    REQUIRE(config_init.candidates.size() == 1);
    CHECK(config_init.candidates.front() == "init");

    const auto config_show = starter::resolve_completion("config s", 8, app, shell_commands);
    REQUIRE(config_show.candidates.size() == 1);
    CHECK(config_show.candidates.front() == "show");

    const auto hello_name = starter::resolve_completion("hello --n", 9, app, shell_commands);
    REQUIRE(hello_name.candidates.size() == 1);
    CHECK(hello_name.candidates.front() == "--name");

    const auto hello_short_enthusiastic = starter::resolve_completion("hello -e", 8, app, shell_commands);
    REQUIRE(hello_short_enthusiastic.candidates.size() == 1);
    CHECK(hello_short_enthusiastic.candidates.front() == "-e");

    const auto hello_long_enthusiastic = starter::resolve_completion("hello --e", 9, app, shell_commands);
    REQUIRE(hello_long_enthusiastic.candidates.size() == 1);
    CHECK(hello_long_enthusiastic.candidates.front() == "--enthusiastic");
}
