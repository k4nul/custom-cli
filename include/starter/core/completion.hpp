#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace CLI {
class App;
}

namespace starter {

enum class CompletionActionKind {
    no_change,
    replace,
    list,
};

struct CompletionResult {
    std::vector<std::string> candidates;
    std::size_t replace_begin = 0;
    std::size_t replace_end = 0;
    std::string prefix;
};

struct CompletionAction {
    CompletionActionKind kind = CompletionActionKind::no_change;
    std::string replacement;
    std::vector<std::string> candidates;
    std::size_t replace_begin = 0;
    std::size_t replace_end = 0;
};

struct TabCompletionState {
    bool primed = false;
    std::string line;
    std::size_t cursor = 0;
    std::size_t replace_begin = 0;
    std::size_t replace_end = 0;
    std::string prefix;
};

CompletionResult resolve_completion(
    std::string_view line,
    std::size_t cursor,
    const std::vector<std::string>& root_commands);

CompletionResult resolve_completion(
    std::string_view line,
    std::size_t cursor,
    const CLI::App& app,
    const std::vector<std::string>& shell_commands);

CompletionAction choose_tab_completion(
    const CompletionResult& completion,
    std::string_view line,
    std::size_t cursor,
    TabCompletionState& state);

void reset_tab_completion(TabCompletionState& state);

}  // namespace starter
