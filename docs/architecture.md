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
- Command registrars: add subcommands to the root parser in one place
- `AppConfig`: starter configuration object serialized as JSON
- Tokenizer: shell-mode command-line splitter for quoted input

## Extension Points

- Add a new command file under `src/commands/`
- Declare its registrar in `include/starter/commands/registrars.hpp`
- Register it from `src/commands/register_commands.cpp`
- If the command adds new config needs, update `AppConfig`, `config/`, JSON parsing and serialization,
  and `describe_config`

Command availability is controlled by compile-time registration. The `enabled_commands` config field is
currently serialized and displayed, but it is not used as a runtime allowlist.

## Command Flow

1. `src/main.cpp` loads project metadata and creates `Application`.
2. `Application` builds a fresh CLI parser for each dispatch.
3. Command registrars attach subcommands and callbacks.
4. Callbacks read config and produce output through shared streams.
5. Shell mode tokenizes user input and reuses the same dispatch path.

## Adding A Command

1. Create `src/commands/example_command.cpp`.
2. Define a small registrar function that adds a `CLI11` subcommand.
3. Keep option state local to the command via owned state objects.
4. Register the command centrally.
5. Add docs and at least one test when the command changes user-facing behavior.

## Why There Is No Runtime Plugin Layer

The legacy runtime plugin loader was tightly coupled to unavailable infrastructure and product-specific commands. For a starter repository, compile-time command registration is easier to understand, easier to build, and easier to copy into a new project.
