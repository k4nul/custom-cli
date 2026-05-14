#include "starter/core/completion.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <CLI/CLI.hpp>

#include "starter/core/tokenize.hpp"

namespace starter {
namespace {

bool is_space(char value) {
    return std::isspace(static_cast<unsigned char>(value)) != 0;
}

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

void append_unique(std::vector<std::string>& values, std::string value) {
    if (value.empty()) {
        return;
    }
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(std::move(value));
    }
}

std::vector<std::string> split_option_names(const std::string& names) {
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start <= names.size()) {
        const auto comma = names.find(',', start);
        const auto end = comma == std::string::npos ? names.size() : comma;
        auto name = names.substr(start, end - start);
        if (starts_with(name, "-")) {
            result.push_back(std::move(name));
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return result;
}

bool is_visible_subcommand(const CLI::App* subcommand) {
    return subcommand != nullptr && !subcommand->get_disabled() && !subcommand->get_silent()
        && !subcommand->get_name().empty() && !subcommand->get_group().empty();
}

void append_subcommand_names(const CLI::App& app, std::vector<std::string>& candidates) {
    const auto subcommands = app.get_subcommands([](const CLI::App* subcommand) {
        return is_visible_subcommand(subcommand);
    });
    for (const auto* subcommand : subcommands) {
        append_unique(candidates, subcommand->get_name());
        for (const auto& alias : subcommand->get_aliases()) {
            append_unique(candidates, alias);
        }
    }
}

const CLI::App* find_subcommand(const CLI::App& app, const std::string& token) {
    const auto subcommands = app.get_subcommands([](const CLI::App* subcommand) {
        return is_visible_subcommand(subcommand);
    });
    for (const auto* subcommand : subcommands) {
        if (subcommand->get_name() == token) {
            return subcommand;
        }
        const auto& aliases = subcommand->get_aliases();
        if (std::find(aliases.begin(), aliases.end(), token) != aliases.end()) {
            return subcommand;
        }
    }
    return nullptr;
}

void append_option_names(const CLI::App& app, std::vector<std::string>& candidates) {
    const auto options = app.get_options([](const CLI::Option* option) {
        return option != nullptr && option->nonpositional() && !option->get_group().empty();
    });
    for (const auto* option : options) {
        for (auto name : split_option_names(option->get_name(false, true, true))) {
            append_unique(candidates, std::move(name));
        }
    }
}

CompletionResult make_base_result(std::string_view line, std::size_t cursor) {
    const auto clamped_cursor = std::min(cursor, line.size());
    std::size_t token_begin = clamped_cursor;
    while (token_begin > 0 && !is_space(line[token_begin - 1])) {
        --token_begin;
    }

    CompletionResult result;
    result.replace_begin = token_begin;
    result.replace_end = clamped_cursor;
    result.prefix = std::string(line.substr(token_begin, clamped_cursor - token_begin));
    return result;
}

std::vector<std::string> completed_tokens_before(std::string_view line, std::size_t token_begin) {
    try {
        return tokenize_command_line(std::string(line.substr(0, token_begin)));
    } catch (const std::exception&) {
        return {};
    }
}

std::vector<std::string> matching_candidates(
    const std::vector<std::string>& candidates,
    std::string_view prefix) {
    std::vector<std::string> matches;
    for (const auto& candidate : candidates) {
        if (starts_with(candidate, prefix)) {
            append_unique(matches, candidate);
        }
    }
    return matches;
}

std::string longest_common_prefix(const std::vector<std::string>& candidates) {
    if (candidates.empty()) {
        return {};
    }

    std::string prefix = candidates.front();
    for (std::size_t index = 1; index < candidates.size(); ++index) {
        const auto& candidate = candidates[index];
        std::size_t length = 0;
        while (length < prefix.size() && length < candidate.size() && prefix[length] == candidate[length]) {
            ++length;
        }
        prefix.resize(length);
        if (prefix.empty()) {
            break;
        }
    }
    return prefix;
}

std::string line_after_replacement(
    std::string_view line,
    std::size_t replace_begin,
    std::size_t replace_end,
    const std::string& replacement) {
    std::string updated(line);
    updated.replace(replace_begin, replace_end - replace_begin, replacement);
    return updated;
}

const CLI::App& command_context_for(const CLI::App& app, const std::vector<std::string>& context_tokens) {
    const CLI::App* current = &app;
    for (const auto& token : context_tokens) {
        if (starts_with(token, "-")) {
            break;
        }
        const auto* next = find_subcommand(*current, token);
        if (next == nullptr) {
            break;
        }
        current = next;
    }
    return *current;
}

bool is_root_context(const CLI::App& root, const CLI::App& current) {
    return &root == &current;
}

}  // namespace

CompletionResult resolve_completion(
    std::string_view line,
    std::size_t cursor,
    const std::vector<std::string>& root_commands) {
    auto result = make_base_result(line, cursor);
    result.candidates = matching_candidates(root_commands, result.prefix);
    return result;
}

CompletionResult resolve_completion(
    std::string_view line,
    std::size_t cursor,
    const CLI::App& app,
    const std::vector<std::string>& shell_commands) {
    auto result = make_base_result(line, cursor);
    const auto context_tokens = completed_tokens_before(line, result.replace_begin);
    const auto& current_app = command_context_for(app, context_tokens);

    std::vector<std::string> candidates;
    if (starts_with(result.prefix, "-")) {
        append_option_names(current_app, candidates);
    } else {
        append_subcommand_names(current_app, candidates);
        if (is_root_context(app, current_app)) {
            for (const auto& command : shell_commands) {
                append_unique(candidates, command);
            }
        }
    }

    result.candidates = matching_candidates(candidates, result.prefix);
    return result;
}

CompletionAction choose_tab_completion(
    const CompletionResult& completion,
    std::string_view line,
    std::size_t cursor,
    TabCompletionState& state) {
    CompletionAction action;
    action.candidates = completion.candidates;
    action.replace_begin = completion.replace_begin;
    action.replace_end = completion.replace_end;

    if (completion.candidates.size() == 1) {
        action.kind = CompletionActionKind::replace;
        action.replacement = completion.candidates.front();
        reset_tab_completion(state);
        return action;
    }

    const auto shared_prefix = longest_common_prefix(completion.candidates);
    if (shared_prefix.size() > completion.prefix.size()) {
        action.kind = CompletionActionKind::replace;
        action.replacement = shared_prefix;

        state.primed = true;
        state.line = line_after_replacement(line, completion.replace_begin, completion.replace_end, shared_prefix);
        state.cursor = completion.replace_begin + shared_prefix.size();
        state.replace_begin = completion.replace_begin;
        state.replace_end = state.cursor;
        state.prefix = shared_prefix;
        return action;
    }

    const bool same_request = state.primed && state.line == line && state.cursor == cursor
        && state.replace_begin == completion.replace_begin && state.replace_end == completion.replace_end
        && state.prefix == completion.prefix;
    if (same_request && !completion.candidates.empty()) {
        action.kind = CompletionActionKind::list;
        reset_tab_completion(state);
        return action;
    }

    state.primed = true;
    state.line = std::string(line);
    state.cursor = cursor;
    state.replace_begin = completion.replace_begin;
    state.replace_end = completion.replace_end;
    state.prefix = completion.prefix;
    return action;
}

void reset_tab_completion(TabCompletionState& state) {
    state = TabCompletionState{};
}

}  // namespace starter
