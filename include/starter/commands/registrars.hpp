#pragma once

#include <iosfwd>
#include <string>

#include <CLI/CLI.hpp>

#include "starter/core/project_info.hpp"

namespace starter {

void register_builtin_commands(
    CLI::App& root,
    const ProjectInfo& project_info,
    std::string& config_path,
    std::ostream& out,
    std::ostream& err,
    bool& command_executed);

void register_about_command(
    CLI::App& root,
    const ProjectInfo& project_info,
    std::string& config_path,
    std::ostream& out,
    std::ostream& err,
    bool& command_executed);

void register_config_command(
    CLI::App& root,
    const ProjectInfo& project_info,
    std::string& config_path,
    std::ostream& out,
    std::ostream& err,
    bool& command_executed);

void register_doctor_command(
    CLI::App& root,
    const ProjectInfo& project_info,
    std::string& config_path,
    std::ostream& out,
    std::ostream& err,
    bool& command_executed);

void register_echo_command(
    CLI::App& root,
    const ProjectInfo& project_info,
    std::string& config_path,
    std::ostream& out,
    std::ostream& err,
    bool& command_executed);

void register_hello_command(
    CLI::App& root,
    const ProjectInfo& project_info,
    std::string& config_path,
    std::ostream& out,
    std::ostream& err,
    bool& command_executed);

}  // namespace starter

