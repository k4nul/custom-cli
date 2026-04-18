#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace starter {

std::vector<std::string> tokenize_command_line(std::string_view line);
std::string join_tokens(const std::vector<std::string>& tokens, std::string_view delimiter = " ");

}  // namespace starter

