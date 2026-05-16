#include "starter/core/tokenize.hpp"

#include <cctype>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace starter {

std::vector<std::string> tokenize_command_line(std::string_view line) {
    std::vector<std::string> tokens;
    std::string current;
    char quote_character = '\0';
    bool escaping = false;
    bool token_started = false;

    for (char character : line) {
        if (escaping) {
            current.push_back(character);
            escaping = false;
            token_started = true;
            continue;
        }

        if (character == '\\') {
            escaping = true;
            continue;
        }

        if (quote_character != '\0') {
            if (character == quote_character) {
                quote_character = '\0';
            } else {
                current.push_back(character);
            }
            continue;
        }

        if (character == '"' || character == '\'') {
            quote_character = character;
            token_started = true;
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(character)) != 0) {
            if (token_started) {
                tokens.push_back(current);
                current.clear();
                token_started = false;
            }
            continue;
        }

        current.push_back(character);
        token_started = true;
    }

    if (escaping) {
        throw std::runtime_error("trailing escape character in command line");
    }

    if (quote_character != '\0') {
        throw std::runtime_error("unterminated quote in command line");
    }

    if (token_started) {
        tokens.push_back(current);
    }

    return tokens;
}

std::string join_tokens(const std::vector<std::string>& tokens, std::string_view delimiter) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (index != 0) {
            stream << delimiter;
        }
        stream << tokens[index];
    }
    return stream.str();
}

}  // namespace starter
