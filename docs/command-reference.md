# Command Reference

This reference describes the starter's current command surface. It is grounded in
the root CLI setup in `src/app/application.cpp`, the sample-command registrations
in `src/commands/register_commands.cpp`, and the config schema in
`include/starter/core/config.hpp`.

## Invocation Model

Run a one-shot command by passing the command path after the executable:

```bash
./build/cli-starter hello --name Ada
./build/cli-starter config show
```

Running the executable with no command starts the interactive shell. The `shell`
subcommand starts the same mode explicitly, which is useful when you want to pass
global options first:

```bash
./build/cli-starter
./build/cli-starter --config ./config/local.json shell
```

The default executable name is controlled by the `CLI_STARTER_BINARY_NAME` CMake
cache variable. If that value is changed, replace `cli-starter` in examples with
the configured output name.

## Global Options

Global options are registered on the root command and should be placed before the
command path.

| Option | Behavior |
| --- | --- |
| `--version` | Print the configured display name and version. |
| `--help` | Print top-level help. |
| `--help-all` | Print top-level help plus subcommand help. |
| `-c, --config <path>` | Use a non-default JSON config path. |

## Built-In Commands

### `about`

Prints the configured display name, version, binary name, default config path,
and a short description of the starter.

```bash
./build/cli-starter about
```

### `hello`

Prints a greeting. Use `--name <value>` to provide the name directly, or omit it
to read `default_name` from the active config. If the active config file is
missing and no `--name` is supplied, the command uses the built-in default and
prints a tip to generate a config file.

```bash
./build/cli-starter hello --name Ada
./build/cli-starter --config ./config/local.json hello
./build/cli-starter hello --name Ada --enthusiastic
```

Options:

- `--name <value>`: name to greet.
- `-e, --enthusiastic`: use an exclamation point instead of a period.

### `echo`

Echoes one or more required positional arguments. By default, values are joined
with spaces. `--numbered` prints one value per line with a one-based index.

```bash
./build/cli-starter echo starter ready
./build/cli-starter echo --uppercase starter ready
./build/cli-starter echo --numbered one two
```

Options and arguments:

- `--uppercase`: convert output values to uppercase.
- `--numbered`: print each token on its own numbered line.
- `text...`: required positional text to echo.

### `config init`

Writes a JSON config template. If `--output` is omitted, the command writes to
the active config path, including a path supplied through the global `--config`
option. If `--output` is supplied, that output path wins for the write.

```bash
./build/cli-starter --config ./config/local.json config init
./build/cli-starter --config ./config/local.json config init --output ./starter-template.json
```

Options:

- `-o, --output <path>`: write the generated template to a specific path.

The generated file starts from current `AppConfig` defaults, then applies the
configured prompt label and generated-template `notes` value. It is not a
byte-for-byte copy of `config/cli-starter.json`.

### `config show`

Prints the effective config path, source, prompt, default name, enabled command
list, and notes. Missing config files use built-in defaults. Existing config
files must be JSON objects with supported field types.

```bash
./build/cli-starter --config ./config/local.json config show
```

### `doctor`

Checks starter layout assumptions and the active config path. It reports the
presence of `src`, `include`, `docs`, `config`, and `third_party`, then reports
whether the active config was loaded from disk or built-in defaults are active.

```bash
./build/cli-starter doctor
./build/cli-starter --config ./config/local.json doctor
```

Missing layout paths and a missing config file are advisory findings printed to
stdout. Malformed JSON or wrong config field types are config errors.

### `shell`

Starts the interactive shell explicitly. This is equivalent to running the
executable with no command, except it lets callers set global options first.

```bash
./build/cli-starter --config ./config/local.json shell
```

Inside the shell:

- `help` prints top-level help.
- `help <command...>` prints command-specific help.
- `exit` and `quit` end the shell.
- Other input is tokenized and dispatched through the same command path as
  one-shot execution.

Malformed shell input, such as an unterminated quote, is reported as an input
error and the shell continues. A command that returns a non-zero status reports
that status, then the shell prompts for the next command.

## Interactive Completion

Completion is available only when the shell is attached to an interactive
terminal and terminal raw mode is available. Redirected input still works, but it
uses ordinary line reads without `Tab` completion.

Completion candidates come from a fresh CLI11 parser configured with the same
commands and global options as normal dispatch. Root completion includes
registered commands plus shell-only `help`, `exit`, and `quit`. Subcommand
completion follows the current command context, and option completion is scoped
to the active command.

## Config Reference

The default config path is `config/<configured-file-name>`, where the configured
file name defaults to `cli-starter.json`.

Supported fields:

| Field | Type | Default | Behavior |
| --- | --- | --- | --- |
| `prompt` | string | `starter` | Interactive shell prompt from built-in defaults, or from disk when a config file is loaded. |
| `default_name` | string | `world` | Name used by `hello` when `--name` is omitted. |
| `enabled_commands` | string array | `about`, `hello`, `echo`, `config`, `doctor` | Serialized and shown by `config show`; not a runtime allowlist. |
| `notes` | string | `Customize this file after copying the starter.` | Informational text shown by `config show`. |

Missing fields fall back to built-in defaults. Malformed JSON, a non-object JSON
document, or wrong field types are reported as config errors.

## Exit Statuses

The starter reserves these application-level exit statuses:

| Status | Meaning |
| --- | --- |
| `0` | Success, including help and version output. |
| `2` | Starter usage error outside CLI11 parse-error handling. |
| `3` | Config write or filesystem I/O failure. |
| `4` | Config read or parse failure. |
| `5` | Unexpected runtime error. |

CLI11 parse errors, such as unknown commands or missing required arguments, may
return CLI11-specific statuses while still printing usage guidance.
