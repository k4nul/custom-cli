#pragma once

#include <iosfwd>
#include <string>

#include "starter/core/project_info.hpp"

namespace CLI {
class App;
}

namespace starter {

void configure_cli_app(
    CLI::App& app,
    const ProjectInfo& project_info,
    std::string& config_path,
    std::ostream& out,
    std::ostream& err,
    bool& command_executed,
    bool& shell_requested);

}  // namespace starter
