#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <sstream>
#include <string>
#include <vector>

#include "starter/app/application.hpp"
#include "starter/core/config.hpp"
#include "starter/core/project_info.hpp"
#include "starter/core/tokenize.hpp"

TEST_CASE("tokenizer preserves quoted groups") {
    const auto tokens = starter::tokenize_command_line("hello --name \"starter user\" 'quoted value'");

    REQUIRE(tokens.size() == 4);
    CHECK(tokens[0] == "hello");
    CHECK(tokens[1] == "--name");
    CHECK(tokens[2] == "starter user");
    CHECK(tokens[3] == "quoted value");
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

TEST_CASE("application accepts hello subcommand options from argv order") {
    std::ostringstream out;
    std::ostringstream err;
    starter::Application application(starter::load_project_info(), out, err);

    std::vector<std::string> arg_storage = {"cli-starter", "hello", "--name", "starter user"};
    std::vector<char*> argv;
    argv.reserve(arg_storage.size());
    for (auto& arg : arg_storage) {
        argv.push_back(arg.data());
    }

    const int result = application.run(static_cast<int>(argv.size()), argv.data());

    CHECK(result == 0);
    CHECK(out.str() == "Hello, starter user.\n");
    CHECK(err.str().empty());
}
