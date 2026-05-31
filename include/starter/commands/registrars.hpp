#pragma once

#include <iosfwd>
#include <string>

#include "starter/core/project_info.hpp"

namespace CLI {
class App;
}

namespace starter {

struct CommandRegistrationContext {
    const ProjectInfo& project_info;
    std::string& config_path;
    std::ostream& out;
    std::ostream& err;
    bool& command_executed;
};

void register_builtin_commands(
    CLI::App& root,
    const CommandRegistrationContext& context);

}  // namespace starter
