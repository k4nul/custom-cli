# Architecture

## Directory Layout

- `src/app/`: application lifecycle and dispatch flow
- `src/commands/`: command implementations and registrar wiring
- `src/core/`: shared helpers such as config loading, tokenization, and completion
- `include/starter/`: public headers for the starter
- `cmake/`: generated project metadata templates
- `config/`: default config template location
- `tests/`: starter behavior tests
- `third_party/`: vendored header-only dependencies and license texts

## Core Components

- `Application`: owns one-shot execution, interactive shell mode, and global options
- Shell line reader: handles interactive input and completion; `Application` can receive a scripted
  reader for deterministic lifecycle tests
- Command registrars: add subcommands to the root parser in one place
- `AppConfig`: starter configuration object serialized as JSON
- Tokenizer: shell-mode command-line splitter for quoted input

## Interactive Shell And Completion

`Application::run` starts the interactive shell when no argv command is
provided. The `shell` subcommand starts the same mode explicitly, which lets a
caller provide global options first, such as `--config ./config/local.json shell`.

Shell startup loads the active config path once to choose the prompt. If the
file does not exist, the shell uses built-in defaults and tells the user which
config path would be used. Inside the shell:

- `help` dispatches to top-level `--help`.
- `help <command...>` dispatches to that command path with `--help` appended.
- `exit` and `quit` leave the shell without going through CLI11 command
  parsing.
- All other input is tokenized and dispatched through the same command path as
  one-shot argv execution.

Malformed shell input, such as an unterminated quote, is reported as an input
error and the shell keeps running. If a dispatched command returns a non-zero
exit code, the shell reports that exit code and then prompts for the next
command.

Completion is derived from a fresh CLI11 parser configured with the same
commands and global options as normal dispatch. Root completion includes the
registered CLI commands plus shell-only `help`, `exit`, and `quit`. Subcommand
completion follows the current command context, while option completion is
scoped to the active command when the current token starts with `-`.

The line reader performs completion only for an interactive terminal. If stdin
is not interactive, or raw terminal mode cannot be enabled on POSIX systems, it
falls back to ordinary line reads without interactive completion.

## Extension Points

- Add a new command file under `src/commands/`
- Add the command source to the manually maintained `starter_core` source list in `CMakeLists.txt`
- Declare its registrar in `include/starter/commands/registrars.hpp`
- Register it from `src/commands/register_commands.cpp`
- If the command adds new config needs, update `AppConfig`, `config/`, JSON parsing and serialization,
  and `describe_config`

Command availability is controlled by compile-time CLI wiring. `Application`
registers the root `shell` command directly, and `register_builtin_commands`
registers the sample commands from `src/commands/`. The `enabled_commands`
config field is currently serialized and displayed, but it is not used as a
runtime allowlist.

## Command Flow

1. `src/main.cpp` loads project metadata and creates `Application`.
2. `Application` builds a fresh CLI parser for each dispatch.
3. Command registrars attach subcommands and callbacks.
4. Callbacks read config and produce output through shared streams.
5. Shell mode tokenizes user input and reuses the same dispatch path.

## Adding A Command

1. Create `src/commands/example_command.cpp`.
2. Add the new `.cpp` file to the `starter_core` source list in `CMakeLists.txt`.
3. Define a small registrar function that adds a `CLI11` subcommand.
4. Keep option state local to the command via owned state objects.
5. Register the command centrally.
6. Add docs and at least one test when the command changes user-facing behavior.

## Why There Is No Runtime Plugin Layer

The legacy runtime plugin loader was tightly coupled to unavailable infrastructure and product-specific
commands. For a starter repository, compile-time command registration is easier to understand, easier
to build, and easier to copy into a new project.
