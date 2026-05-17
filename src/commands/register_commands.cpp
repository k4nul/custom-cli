#include "starter/commands/registrars.hpp"

namespace starter {

void register_builtin_commands(
    CLI::App& root,
    const ProjectInfo& project_info,
    std::string& config_path,
    std::ostream& out,
    std::ostream& err,
    bool& command_executed) {
    register_about_command(root, project_info, config_path, out, err, command_executed);
    register_hello_command(root, project_info, config_path, out, err, command_executed);
    register_echo_command(root, project_info, config_path, out, err, command_executed);
    register_config_command(root, project_info, config_path, out, err, command_executed);
    register_doctor_command(root, project_info, config_path, out, err, command_executed);
}

}  // namespace starter
