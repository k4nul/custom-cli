#include "starter/commands/registrars.hpp"

namespace starter {

void register_builtin_commands(
    CLI::App& root,
    const CommandRegistrationContext& context) {
    register_about_command(root, context);
    register_hello_command(root, context);
    register_echo_command(root, context);
    register_config_command(root, context);
    register_doctor_command(root, context);
}

}  // namespace starter
