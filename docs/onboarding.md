# Onboarding

This guide is for a maintainer who has just copied or cloned the starter and
needs to get a local build, smoke test, and first customization loop working.

## Prerequisites

- CMake 3.18 or newer
- A C++17 compiler
- `ctest`, which is provided with CMake

On Linux, a normal CMake toolchain with `g++` or `clang++` is enough. On
Windows, use Visual Studio, Build Tools for Visual Studio, or another
CMake-supported C++ toolchain.

The command parser, JSON library, and test framework are vendored under
`third_party/`, so the starter does not require a package-manager bootstrap.

## First Build

Use an out-of-source build directory. Prefer `build/` for normal local work. The
ignore rules cover `build/`, `build-linux/`, `build-local-*`, `out/`,
`cmake-build-*`, and `.sandbox-user/` for new local build products and sandbox
telemetry.

Ignore rules do not remove files that were already committed, so a checkout may
still contain historical `build-local-*` or `.sandbox-user/` paths. Treat those
paths as legacy local artifacts, not source inputs or validation evidence. If
they are present, rebuild into a fresh ignored build directory before validating
behavior:

```bash
git ls-files 'build-local-*' '.sandbox-user/*'
```

If that command prints paths that still exist in the checkout, the unfiltered
CTest run is expected to fail in `repository_hygiene` when tests run inside a Git
worktree with `git` available. The check is scoped to those legacy tracked
artifact patterns until they are removed in a separate cleanup change. You can
still rebuild into `build/` to inspect current source behavior, but report full
validation as blocked by artifact hygiene instead of treating a filtered run or
old artifact output as authoritative.

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The explicit `BUILD_TESTING=ON` flag keeps CTest registration enabled even if a
previous cache or preset disabled CMake's default test support.

The default executable name is `cli-starter`. With single-config generators,
run the executable under `build/` from the repository root:

```bash
./build/cli-starter --version
./build/cli-starter about
```

With multi-config generators such as Visual Studio, build and run a specific
configuration:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\cli-starter.exe --version
.\build\Debug\cli-starter.exe about
```

## Local Smoke Test

After the executable builds, run the starter commands that exercise the main
user flows:

```bash
./build/cli-starter doctor
./build/cli-starter config init --output ./config/local.json
./build/cli-starter --config ./config/local.json config show
./build/cli-starter --config ./config/local.json hello
./build/cli-starter echo --uppercase starter ready
```

`config/local.json` and `config/*.local.json` are ignored so maintainers can
keep local config experiments out of commits.

## Interactive Shell

Running the executable with no command starts the interactive shell:

```bash
./build/cli-starter
```

Useful first commands inside the shell:

```text
starter> help
starter> hello --name Ada
starter> config show
starter> exit
```

The shell reuses the same command dispatch path as one-shot execution. It also
adds shell-only `help`, `exit`, and `quit` commands and provides `Tab`
completion for root commands, subcommands, and options when stdin is an
interactive terminal. Redirected shell input still works, but it uses plain line
reading without interactive completion.

## Customization Loop

The first customization pass usually changes names before replacing behavior.
Set the CMake cache variables during configure:

```bash
cmake -S . -B build \
  -DCLI_STARTER_BINARY_NAME=my-cli \
  -DCLI_STARTER_DISPLAY_NAME="My CLI" \
  -DCLI_STARTER_CONFIG_FILE=my-cli.json \
  -DCLI_STARTER_PROMPT_LABEL=mycli \
  -DBUILD_TESTING=ON \
  -DCLI_STARTER_BUILD_TESTS=ON
```

Those values are written into the generated project config header in the build
tree. The checked-in JSON template remains in `config/cli-starter.json` until
you intentionally rename or replace it.

`CLI_STARTER_CONFIG_FILE` changes the default runtime path under `config/`. If
you set it to `my-cli.json`, copy or rename the template to
`config/my-cli.json`, or pass `--config <path>` while the copied project is in
transition. `CLI_STARTER_PROMPT_LABEL` is the fallback shell prompt, but a disk
config's `prompt` field wins when the config file exists, so update both when
you want the prompt to stay consistent.

The `cli_starter_smoke` CTest entry checks the built executable's success path,
including `--version`, `about`, `doctor`, config initialization and display,
`hello`, and numbered `echo`, and it fails if a success case returns a non-zero
status, misses the expected stdout pattern, or writes to stderr. It also checks
representative failure paths for parser errors and bad config input, where the
command must fail, leave stdout empty, and print the expected stderr guidance.
When a copied project intentionally changes display metadata, about text,
command registration, config behavior, or user-facing parse/config errors,
update `cmake/cli_smoke_test.cmake` in the same change.

## Adding The First Real Command

The starter uses compile-time command registration instead of a runtime plugin
loader. To add a command:

1. Add a command implementation under `src/commands/`.
2. Add the command `.cpp` file to the `starter_core` source list in `CMakeLists.txt`.
3. Declare the registrar in `include/starter/commands/registrars.hpp`.
4. Register it from `src/commands/register_commands.cpp`.
5. Add or extend tests under `tests/`.
6. Update user-facing docs for the command, including README, architecture,
   testing, and troubleshooting notes when the behavior changes those areas.

Keep sample commands only as long as they help the copied project. Once real
commands exist, remove or rewrite the samples that no longer match the new
tool's purpose.

## Documentation Map

- `README.md`: quick start, command list, configuration, and extension summary
- `docs/project-overview.md`: repository purpose and scope
- `docs/command-reference.md`: global options, built-in commands, config
  fields, shell behavior, and exit statuses
- `docs/architecture.md`: component layout and command flow
- `docs/testing.md`: test targets, validation commands, and coverage notes
- `docs/troubleshooting.md`: common local build and runtime issues
- `docs/maintenance.md`: maintainer checklist for command, config, dependency, and documentation changes
- `docs/migration-from-legacy.md`: historical migration notes
- `third_party/README.md`: vendored dependency versions, source URLs, license
  links, and update guidance
