#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

#include "starter/core/project_info.hpp"

namespace CLI {
class App;
}

namespace starter {

class Application {
public:
    Application(ProjectInfo project_info, std::ostream& out, std::ostream& err);

    int run(int argc, char** argv);

private:
    void configure_cli_app(CLI::App& app, std::string& config_path, bool& command_executed, bool& shell_requested);
    int dispatch(std::vector<std::string> args, bool interactive_mode);
    int run_shell(const std::filesystem::path& config_path);
    std::vector<std::string> shell_completion_commands() const;

    ProjectInfo project_info_;
    std::ostream& out_;
    std::ostream& err_;
    std::string active_shell_config_path_;
};

}  // namespace starter
