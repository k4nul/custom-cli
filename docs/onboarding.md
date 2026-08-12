# Onboarding

This guide is for a maintainer who has just copied or cloned the starter and
needs to get a local build, smoke test, and first customization loop working.

## Prerequisites

- CMake 3.18 or newer
- A C++17 compiler
- `ctest`, which is provided with CMake
- Python 3 for the template instantiation workflow test
- Git when you need reportable full validation, because the repository hygiene
  gate uses `git ls-files` for tracked artifact inspection

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
test -f .editorconfig
test -f .gitattributes
git ls-files 'build-local-*' '.sandbox-user/*'
```

If either policy-file check fails, or if the artifact query prints paths that
still exist in the checkout, the unfiltered CTest run is expected to fail in
`repository_hygiene`. The artifact check is scoped to those legacy tracked
artifact patterns until they are removed in a separate cleanup change. You can
still rebuild into `build/` to inspect current source behavior, but report full
validation as blocked by repository hygiene instead of treating a filtered run
or old artifact output as authoritative.

Use [docs/artifact-hygiene.md](artifact-hygiene.md) when that separate cleanup
package is intentionally selected.

When you need partial source-behavior evidence before that cleanup lands, keep
the artifact preflight output with the report and filter out only the hygiene
entry:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure -R '^(starter_tests|project_completion_state|project_completion_state_regression|template_instantiation_workflow|shell_completion_generation|command_manpage_generation|command_metadata_json_generation|command_markdown_reference_generation|cli_starter_smoke)$'
```

After the artifact preflight prints no paths, use the unfiltered flow:

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
Arrow keys and other terminal special keys are ignored rather than inserted into
the command line; this starter does not provide cursor movement or command
history.

If you start the shell with a config path, that path remains the session
default:

```bash
./build/cli-starter --config ./config/local.json shell
```

Inside the shell, put `--config` before a command when you want one dispatch to
read a different config. The prompt and later commands still use the startup
config path unless those later commands pass their own `--config` value:

```text
starter> --config ./config/alternate.json config show
starter> config show
```

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

You can generate and validate that command with the repository-local
instantiation workflow, and write the matching config template when the copied
project is ready to use the renamed default config file. Start with a dry plan
before writing files so the derived display name, config file, prompt label,
and validation command are visible:

```bash
python3 scripts/instantiate_template.py \
  --binary-name my-cli \
  --display-name "My CLI" \
  --config-file my-cli.json \
  --prompt-label mycli
```

When the plan looks right, either run the printed command yourself or have the
helper execute the generated configure, build, and CTest sequence:

```bash
python3 scripts/instantiate_template.py \
  --binary-name my-cli \
  --display-name "My CLI" \
  --config-file my-cli.json \
  --prompt-label mycli \
  --run-validation
```

Then add `--write-config` when the copied project is ready to commit
`config/my-cli.json`:

```bash
python3 scripts/instantiate_template.py \
  --binary-name my-cli \
  --display-name "My CLI" \
  --config-file my-cli.json \
  --prompt-label mycli \
  --write-config
```

Inspect the generated config before committing it, then rerun validation from
the copied checkout or pass `--repo-root /path/to/copied/starter` when invoking
the helper from another directory:

```bash
git status --short
cat config/my-cli.json
python3 scripts/instantiate_template.py \
  --binary-name my-cli \
  --display-name "My CLI" \
  --config-file my-cli.json \
  --prompt-label mycli \
  --run-validation
```

Those values are written into the generated project config header in the build
tree. The checked-in JSON template remains in `config/cli-starter.json` unless
you intentionally rename or replace it; `--write-config` creates
`config/my-cli.json` as the copied project's matching runtime template.

`CLI_STARTER_CONFIG_FILE` changes the default runtime path under `config/`. If
you set it to `my-cli.json`, use the helper's `--write-config` path or pass
`--config <path>` while the copied project is in transition.
`CLI_STARTER_PROMPT_LABEL` is written by `config init` and is used when a loaded
config has an empty `prompt`. If no config file exists, shell startup uses the
built-in `AppConfig` prompt, currently `starter`, until that file exists. Update
or regenerate the disk config's `prompt` when you want disk-backed shell
sessions to use the renamed prompt consistently.

See [template-instantiation.md](template-instantiation.md) for the full
copy-time workflow, including what the helper changes, what it intentionally
leaves unchanged, and which docs and tests should move with helper changes.

The `cli_starter_smoke` CTest entry checks the built executable's success path,
including `--version`, `about`, `doctor`, config initialization and display,
`hello`, and numbered `echo`, and it fails if a success case returns a non-zero
status, misses the expected stdout pattern, or writes to stderr. It also feeds
redirected stdin into the default shell, the explicit `shell` subcommand, and
an empty-prompt shell config so the built executable covers the banner, prompts,
shell help, command dispatch, prompt-label fallback, and config-backed
prompt/default-name flow. Representative failure paths cover
parser errors and bad config input, where the command must fail, leave stdout
empty, and print the expected stderr guidance. When a copied project
intentionally changes display metadata, about text, command registration,
config behavior, shell startup or redirected shell behavior, or user-facing
parse/config errors, update `cmake/cli_smoke_test.cmake` in the same change.

## Adding The First Real Command

The starter uses compile-time command registration instead of a runtime plugin
loader. To add a command:

1. Add a command implementation under `src/commands/`.
2. Add the command `.cpp` file to the `starter_core` source list in `CMakeLists.txt`.
3. Declare the registrar in `src/commands/builtin_command_registrars.hpp`
   using `CommandRegistrationContext`.
4. Register it from `src/commands/register_commands.cpp`.
5. Add or extend tests under `tests/`, and update
   `cmake/cli_smoke_test.cmake` when the built executable's command path,
   output, error text, or shell dispatch changes.
6. Update user-facing docs for the command, including README, command
   reference, architecture, testing, and troubleshooting notes when the behavior
   changes those areas.

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
- `docs/template-instantiation.md`: copied-starter rename workflow, generated
  config template behavior, safety rules, and validation commands
- `docs/artifact-hygiene.md`: tracked local artifact gate and cleanup runbook
- `docs/ci.md`: GitHub Actions triggers, job commands, and failure triage
- `docs/troubleshooting.md`: common local build and runtime issues
- `docs/maintenance.md`: maintainer checklist for command, config, dependency, and documentation changes
- `docs/migration-from-legacy.md`: historical migration notes
- `third_party/README.md`: vendored dependency versions, source URLs, license
  links, and update guidance
