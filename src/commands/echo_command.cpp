#include "starter/commands/registrars.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include "starter/core/tokenize.hpp"

namespace starter {

namespace {

struct EchoOptions {
    std::vector<std::string> text;
    bool uppercase = false;
    bool numbered = false;
};

std::string uppercase_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return value;
}

}  // namespace

void register_echo_command(
    CLI::App& root,
    const ProjectInfo& project_info,
    std::string& config_path,
    std::ostream& out,
    std::ostream& err,
    bool& command_executed) {
    (void)project_info;
    (void)config_path;
    (void)err;

    auto options = std::make_shared<EchoOptions>();
    auto* command = root.add_subcommand("echo", "Echo text to demonstrate positional arguments.");
    command->add_flag("--uppercase", options->uppercase, "Render the output in uppercase.");
    command->add_flag("--numbered", options->numbered, "Print each token on its own numbered line.");
    command->add_option("text", options->text, "Text to echo back.")->required()->expected(-1);

    command->callback([&, options]() {
        command_executed = true;

        std::vector<std::string> values = options->text;
        if (options->uppercase) {
            for (auto& value : values) {
                value = uppercase_copy(value);
            }
        }

        if (options->numbered) {
            for (std::size_t index = 0; index < values.size(); ++index) {
                out << (index + 1) << ". " << values[index] << '\n';
            }
            return;
        }

        out << join_tokens(values) << '\n';
    });
}

}  // namespace starter
