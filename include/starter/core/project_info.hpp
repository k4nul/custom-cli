#pragma once

#include <filesystem>
#include <string>

namespace starter {

struct ProjectInfo {
    std::string display_name;
    std::string binary_name;
    std::string version;
    std::string config_file_name;
    std::string prompt_label;
};

ProjectInfo load_project_info();
std::filesystem::path default_config_path(const ProjectInfo& info);

}  // namespace starter

