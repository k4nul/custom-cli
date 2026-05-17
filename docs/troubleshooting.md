# Troubleshooting

These notes cover local setup issues that can be diagnosed from the starter's
checked-in CMake, config, and command behavior.

## CTest Cannot Find Starter Tests

Make sure tests were enabled at configure time and the build completed:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

`CLI_STARTER_BUILD_TESTS` defaults to `BUILD_TESTING`, and `BUILD_TESTING`
also controls CTest registration through `include(CTest)`. If a previous cache,
toolchain, or preset disabled `BUILD_TESTING`, reconfigure with both flags set
to `ON` instead of only rebuilding the old tree. A healthy tree registers
`starter_tests` and `cli_starter_smoke`.

## Multi-Config Tests Fail To Start

Visual Studio and other multi-config generators put binaries under a
configuration directory. Build and test the same configuration:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The same rule applies when running the executable directly:

```powershell
.\build\Debug\cli-starter.exe about
```

## `./build/cli-starter` Does Not Exist

First confirm that the build finished successfully. If it did, check whether
the selected generator is multi-config and use the configuration-specific path,
such as `.\build\Debug\cli-starter.exe`.

If you changed `CLI_STARTER_BINARY_NAME`, the output file uses that configured
name instead of `cli-starter`.

## Tracked Local Artifacts Appear In `git ls-files`

The repository ignore rules cover new `build/`, `build-local-*`,
`cmake-build-*`, `.sandbox-user/`, and local config files, but ignore rules do
not untrack files that were already committed. If `git ls-files` reports
historical build or sandbox files, treat them as legacy artifacts:

```bash
git ls-files 'build-local-*' '.sandbox-user/*'
```

Do not run old binaries or cite old CTest files from those paths as current
validation. Reconfigure into a fresh ignored build directory and run the normal
CMake/CTest flow before reporting results:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Removing tracked generated files changes repository contents and should be done
as a separate cleanup task, not as part of routine docs or test-result updates.

## A Command Prints `Run with --help`

CLI11 reports parse and validation failures through stderr. Common examples are
an unknown command, a missing required positional argument, an unknown option, or
running a command group without choosing a subcommand:

```bash
./build/cli-starter missing-command
./build/cli-starter echo
./build/cli-starter hello --unknown
./build/cli-starter config
```

Use `--help` for top-level usage, `--help-all` to include subcommand help, or
append `--help` after the command path you are debugging:

```bash
./build/cli-starter --help
./build/cli-starter --help-all
./build/cli-starter config init --help
```

## `hello` Prints A Config Tip

When no config file exists and `hello` is run without `--name`, the command uses
built-in defaults and prints a tip to create a config file. Generate one and
point commands at it:

```bash
./build/cli-starter config init --output ./config/local.json
./build/cli-starter --config ./config/local.json hello
```

`config/local.json` is ignored by default for local experiments.

## `doctor` Warns That Config Is Missing

`doctor` checks repository layout directories and the active config path. A
missing config is a warning, not a broken build, when you have not generated a
local config yet.

Use `config init` to create one:

```bash
./build/cli-starter --config ./config/local.json config init
./build/cli-starter --config ./config/local.json doctor
```

`doctor` is advisory for layout findings: `[warn]` config output and `[missing]`
recommended layout paths still exit successfully so maintainers can inspect the
report. Malformed JSON or wrong field types in an existing config file are
config errors. Use the full CMake/CTest flow when you need validation evidence.

## `config init` Writes Somewhere Unexpected

The global `--config <path>` option sets the default config path for
config-backed commands. `config init` writes to that path unless an explicit
`--output <path>` is supplied:

```bash
./build/cli-starter --config ./config/local.json config init
./build/cli-starter --config ./config/local.json config init --output ./starter-template.json
```

Use `--output` when you want to create a template without changing the active
config path for the command.

The generated file comes from the current `AppConfig` defaults and project
metadata. It is not a byte-for-byte copy of `config/cli-starter.json`.

## A Config-Backed Command Reports A JSON Error

When the active config path exists, config-backed commands parse it as JSON. If
the file is malformed, fix the JSON or regenerate a local template:

```bash
./build/cli-starter --config ./config/local.json config init
./build/cli-starter --config ./config/local.json config show
```

The supported fields are `prompt`, `default_name`, `enabled_commands`, and
`notes`. Missing fields fall back to built-in defaults, but malformed JSON or
wrong field types are reported as command errors.

## `config init` Cannot Write The Config File

`config init` creates parent directories for the target path, then truncates and
writes the JSON file. If it reports a write failure, check that the parent path
is writable and that the target is not a directory or locked by another process.

Prefer the ignored local config path for experiments:

```bash
./build/cli-starter --config ./config/local.json config init
```

## Interactive Input Reports An Input Error

The interactive shell tokenizes input before dispatching commands. Unterminated
or malformed quotes are reported as input errors and do not exit the shell.
Re-enter the command with matching quotes:

```text
starter> hello --name "Ada Lovelace"
```

## Exit Status Reference

The starter reserves these application-level exit statuses:

- `0`: success, including help and version output
- `2`: starter usage error outside CLI11's parse-error handling
- `3`: config write or filesystem I/O failure
- `4`: config read or parse failure
- `5`: unexpected runtime error

CLI11 parse errors, such as missing required arguments or unknown options, can
return CLI11-specific parse-error statuses while still printing the same
stderr-oriented usage guidance.
