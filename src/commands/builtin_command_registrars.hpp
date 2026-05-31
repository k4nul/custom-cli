#pragma once

#include "starter/commands/registrars.hpp"

namespace starter {

void register_about_command(
    CLI::App& root,
    const CommandRegistrationContext& context);

void register_config_command(
    CLI::App& root,
    const CommandRegistrationContext& context);

void register_doctor_command(
    CLI::App& root,
    const CommandRegistrationContext& context);

void register_echo_command(
    CLI::App& root,
    const CommandRegistrationContext& context);

void register_hello_command(
    CLI::App& root,
    const CommandRegistrationContext& context);

}  // namespace starter
