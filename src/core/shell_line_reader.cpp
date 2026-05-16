#include "starter/core/shell_line_reader.hpp"

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace starter {
namespace {

constexpr int tab_key = 9;
constexpr int enter_key = 13;
constexpr int newline_key = 10;
constexpr int backspace_key = 8;
constexpr int delete_key = 127;
constexpr int ctrl_c = 3;
constexpr int ctrl_d = 4;

bool stdin_is_interactive() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

#ifndef _WIN32
class TerminalModeGuard {
public:
    TerminalModeGuard() {
        if (tcgetattr(STDIN_FILENO, &original_) != 0) {
            return;
        }

        auto raw = original_;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        active_ = tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
    }

    TerminalModeGuard(const TerminalModeGuard&) = delete;
    TerminalModeGuard& operator=(const TerminalModeGuard&) = delete;

    ~TerminalModeGuard() {
        if (active_) {
            (void)tcsetattr(STDIN_FILENO, TCSANOW, &original_);
        }
    }

    bool active() const {
        return active_;
    }

private:
    termios original_{};
    bool active_ = false;
};
#endif

int read_key() {
#ifdef _WIN32
    const int key = _getch();
    if (key == 0 || key == 224) {
        (void)_getch();
        return 0;
    }
    return key;
#else
    return std::getchar();
#endif
}

bool is_printable(int key) {
    return key >= 32 && key != delete_key;
}

void redraw_line(
    const std::string& prompt,
    const std::string& line,
    std::ostream& out,
    std::size_t previous_line_size) {
    out << '\r' << prompt << line;
    if (previous_line_size > line.size()) {
        out << std::string(previous_line_size - line.size(), ' ');
        out << '\r' << prompt << line;
    }
    out.flush();
}

void print_candidates(const std::vector<std::string>& candidates, std::ostream& out) {
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        if (index != 0) {
            out << "  ";
        }
        out << candidates[index];
    }
    out << '\n';
}

std::optional<std::string> read_fallback_line(const std::string& prompt, std::ostream& out) {
    out << prompt;
    out.flush();

    std::string line;
    if (!std::getline(std::cin, line)) {
        return std::nullopt;
    }
    return line;
}

}  // namespace

std::optional<std::string> read_shell_line(
    const std::string& prompt,
    std::ostream& out,
    const CompletionProvider& completion_provider) {
    if (!stdin_is_interactive()) {
        return read_fallback_line(prompt, out);
    }

#ifndef _WIN32
    TerminalModeGuard terminal_mode;
    if (!terminal_mode.active()) {
        return read_fallback_line(prompt, out);
    }
#endif

    std::string line;
    TabCompletionState tab_state;

    out << prompt;
    out.flush();

    while (true) {
        const int key = read_key();
        if (key == EOF || key == ctrl_c || (key == ctrl_d && line.empty())) {
            return std::nullopt;
        }

        if (key == enter_key || key == newline_key) {
            out << '\n';
            return line;
        }

        if (key == backspace_key || key == delete_key) {
            if (!line.empty()) {
                line.pop_back();
                out << "\b \b";
                out.flush();
                reset_tab_completion(tab_state);
            }
            continue;
        }

        if (key == tab_key) {
            if (!completion_provider) {
                continue;
            }

            const auto completion = completion_provider(line, line.size());
            const auto action = choose_tab_completion(completion, line, line.size(), tab_state);
            if (action.kind == CompletionActionKind::replace) {
                const auto previous_size = line.size();
                line.replace(
                    action.replace_begin,
                    action.replace_end - action.replace_begin,
                    action.replacement);
                redraw_line(prompt, line, out, previous_size);
            } else if (action.kind == CompletionActionKind::list) {
                out << '\n';
                print_candidates(action.candidates, out);
                out << prompt << line;
                out.flush();
            }
            continue;
        }

        if (is_printable(key)) {
            line.push_back(static_cast<char>(key));
            out << static_cast<char>(key);
            out.flush();
            reset_tab_completion(tab_state);
        }
    }
}

}  // namespace starter
