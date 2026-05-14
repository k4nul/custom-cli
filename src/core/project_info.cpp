#include "starter/core/project_info.hpp"

#include <filesystem>

#include "starter/project_config.hpp"

namespace starter {

ProjectInfo load_project_info() {
    return ProjectInfo{
        STARTER_PROJECT_DISPLAY_NAME,
        STARTER_PROJECT_BINARY_NAME,
        STARTER_PROJECT_VERSION,
        STARTER_PROJECT_CONFIG_FILE,
        STARTER_PROJECT_PROMPT_LABEL,
    };
}

std::filesystem::path default_config_path(const ProjectInfo& info) {
    return std::filesystem::path("config") / info.config_file_name;
}

}  // namespace starter
