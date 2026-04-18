#pragma once

namespace starter {

enum class ExitCode : int {
    success = 0,
    usage_error = 2,
    io_error = 3,
    config_error = 4,
    runtime_error = 5,
};

constexpr int to_int(ExitCode code) {
    return static_cast<int>(code);
}

}  // namespace starter

