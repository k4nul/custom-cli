#include "starter/commands/registrars.hpp"

#include <ostream>
#include <string>

namespace starter {

void register_about_command(
    CLI::App& root,
    const ProjectInfo& project_info,
    std::string& config_path,
    std::ostream& out,
    std::ostream& err,
    bool& command_executed) {
    (void)config_path;
    (void)err;

    auto* command = root.add_subcommand("about", "Describe what this starter provides.");
    command->callback([&]() {
        command_executed = true;
        out << project_info.display_name << " " << project_info.version << '\n';
        out << "Binary name: " << project_info.binary_name << '\n';
        out << "Default config: " << default_config_path(project_info).generic_string() << '\n';
        out << "This repository is a neutral CLI starter with one-shot commands,\n";
        out << "an interactive shell, JSON config scaffolding, and sample commands.\n";
    });
}

}  // namespace starter
