#include "starter/commands/registrars.hpp"

#include <filesystem>
#include <memory>
#include <ostream>
#include <string>

#include <CLI/CLI.hpp>

#include "starter/core/config.hpp"

namespace starter {

namespace {

struct ConfigInitOptions {
    std::string output_path;
};

}  // namespace

void register_config_command(
    CLI::App& root,
    const ProjectInfo& project_info,
    std::string& config_path,
    std::ostream& out,
    std::ostream& err,
    bool& command_executed) {
    (void)err;

    auto* config_command = root.add_subcommand("config", "Write or inspect starter configuration.");
    config_command->require_subcommand(1);

    auto init_options = std::make_shared<ConfigInitOptions>();

    auto* init_command = config_command->add_subcommand("init", "Write a starter config template.");
    init_command->add_option(
        "-o,--output",
        init_options->output_path,
        "Output path for the generated JSON file.");
    init_command->callback([&, init_options]() {
        command_executed = true;

        AppConfig config_template;
        config_template.prompt = project_info.prompt_label;
        config_template.notes = "Rename values and trim sample commands once you start customizing the starter.";

        const auto output_path = std::filesystem::path(
            init_options->output_path.empty() ? config_path : init_options->output_path);
        write_config_template(output_path, config_template);
        out << "Wrote config template to " << output_path.generic_string() << '\n';
    });

    auto* show_command = config_command->add_subcommand("show", "Print the effective starter configuration.");
    show_command->callback([&]() {
        command_executed = true;
        const auto path = std::filesystem::path(config_path);
        bool loaded_from_disk = false;
        const auto config = load_config_or_default(path, &loaded_from_disk);
        out << describe_config(path, config, loaded_from_disk);
    });
}

}  // namespace starter
