#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

#include "starter/core/project_info.hpp"
#include "starter/core/shell_line_reader.hpp"

namespace starter {

class Application {
public:
    Application(
        ProjectInfo project_info,
        std::ostream& out,
        std::ostream& err,
        ShellLineReader shell_line_reader = {});

    int run(int argc, char** argv);

private:
    int dispatch(std::vector<std::string> args, bool interactive_mode);
    bool dispatch_shell_tokens(std::vector<std::string> tokens);
    int run_shell(const std::filesystem::path& config_path);
    std::vector<std::string> shell_completion_commands() const;

    ProjectInfo project_info_;
    std::ostream& out_;
    std::ostream& err_;
    ShellLineReader shell_line_reader_;
    std::string active_shell_config_path_;
};

}  // namespace starter
