#pragma once

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include "starter/core/completion.hpp"

namespace starter {

using CompletionProvider = std::function<CompletionResult(
    std::string_view line,
    std::size_t cursor)>;
using PendingKeyReader = std::function<std::optional<int>()>;
using ShellLineReader = std::function<std::optional<std::string>(
    const std::string&,
    std::ostream&,
    const CompletionProvider&)>;

std::optional<int> normalize_shell_line_key(
    int key,
    const PendingKeyReader& read_pending_key);

std::optional<std::string> read_shell_line(
    const std::string& prompt,
    std::ostream& out,
    const CompletionProvider& completion_provider);

}  // namespace starter
