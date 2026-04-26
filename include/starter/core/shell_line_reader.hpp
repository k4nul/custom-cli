#pragma once

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include "starter/core/completion.hpp"

namespace starter {

using CompletionProvider = std::function<CompletionResult(std::string_view line, std::size_t cursor)>;

std::optional<std::string> read_shell_line(
    const std::string& prompt,
    std::ostream& out,
    const CompletionProvider& completion_provider);

}  // namespace starter
