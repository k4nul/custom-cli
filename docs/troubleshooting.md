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
`starter_tests`, `template_instantiation_workflow`, `cli_starter_smoke`, and
`repository_hygiene`.

Use CTest discovery mode when you need to confirm registration without running
the tests:

```bash
ctest --test-dir build -N
```

If the expected names are missing, reconfigure the build tree with both test
flags set before rebuilding.

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

## CLI Smoke Fails

Run the smoke entry by itself with verbose CTest output:

```bash
ctest --test-dir build --output-on-failure -R '^cli_starter_smoke$' -V
```

The smoke script runs built-executable success cases, redirected shell sessions,
and representative parser/config failures. Inspect
`cmake/cli_smoke_test.cmake` when failures point to changed command output,
stderr routing, shell startup, config behavior, or executable layout.

## Template Instantiation Workflow Fails

Run the workflow entry by itself, then run the Python tests directly when the
CTest output is not enough:

```bash
ctest --test-dir build --output-on-failure -R '^template_instantiation_workflow$'
python3 tests/instantiate_template_tests.py
```

The helper rejects unsafe binary, display, config, prompt, build-directory, and
repository-root values before writing config files or printing a validation
command. If a display name fails, remove double quotes and backslashes; those
characters cannot be written safely to the generated C++ project metadata
header.

When the generated validation command itself is suspect, run it through the
helper from the copied checkout so the configure, build, and CTest steps use the
same plan:

```bash
python3 scripts/instantiate_template.py \
  --binary-name my-cli \
  --display-name "My CLI" \
  --config-file my-cli.json \
  --prompt-label mycli \
  --run-validation
```

## `./build/cli-starter` Does Not Exist

First confirm that the build finished successfully. If it did, check whether
the selected generator is multi-config and use the configuration-specific path,
such as `.\build\Debug\cli-starter.exe`.

If you changed `CLI_STARTER_BINARY_NAME`, the output file uses that configured
name instead of `cli-starter`.

## Config Or Layout Checks Change Outside The Repo Root

The starter resolves runtime paths from the process current working directory.
The default config path is `config/<configured-file-name>` relative to where you
start the process, and `doctor` checks `src`, `include`, `docs`, `config`, and
`third_party` relative to that same directory.

Run examples from the repository root, or pass an explicit config path:

```bash
cd /path/to/copied/starter
./build/cli-starter doctor
./build/cli-starter --config "$PWD/config/local.json" config show
```

## Tracked Local Artifacts Appear In `git ls-files`

The repository ignore rules cover new `build/`, `build-linux/`,
`build-local-*`, `out/`, `cmake-build-*`, `.sandbox-user/`, and local config
files, but ignore rules do not untrack files that were already committed. If
`git ls-files` reports historical build or sandbox paths that still exist in the
checkout, treat them as legacy artifacts:

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

When tracked `build-local-*` or `.sandbox-user/*` artifacts are still present,
the unfiltered CTest run is expected to fail in `repository_hygiene` when tests
run inside a Git worktree with `git` available. Treat that as a repository
hygiene blocker for reportable validation, not as proof that the current source
build or doctest behavior failed. A filtered or focused run can support
investigation, but it does not replace the full unfiltered validation gate.

Use this partial flow only for current source-behavior evidence while the
artifact gate is dirty, and report it with the artifact preflight output:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure -R '^(starter_tests|project_completion_state|project_completion_state_regression|template_instantiation_workflow|shell_completion_generation|command_manpage_generation|command_metadata_json_generation|command_markdown_reference_generation|cli_starter_smoke)$'
```

Removing tracked generated files changes repository contents and should be done
as a separate cleanup task, not as part of routine docs or test-result updates.
The dedicated runbook for that cleanup is
[docs/artifact-hygiene.md](artifact-hygiene.md).

When that cleanup task is intentionally selected, use a fresh branch or change
set and make the removal explicit. Start by confirming that the worktree has no
unrelated edits:

```bash
git status --short
git ls-files 'build-local-*' '.sandbox-user/*'
```

Remove only the tracked generated artifact paths that the hygiene check reports:

```bash
git rm -r --ignore-unmatch -- build-local-* .sandbox-user
git status --short
git ls-files 'build-local-*' '.sandbox-user/*'
```

After cleanup, the final `git ls-files` command should print nothing. Then run
the normal unfiltered validation from a fresh ignored build tree:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The cleanup change should contain the removed generated files and any directly
related documentation update only; it should not mix in command, config, or test
behavior changes.

## Repository Hygiene Fails For Policy Files

`repository_hygiene` checks more than generated artifact paths. It also requires
the repository policy files that keep editor behavior and line endings stable:

```bash
test -f .editorconfig
test -f .gitattributes
```

If CTest reports missing policy files, restore those files before treating the
artifact gate as clean. Do not replace the failure with a filtered CTest run;
the unfiltered validation gate is passing only when the policy files are
present, the tracked artifact query is clean, and the full CMake/CTest flow
passes from a fresh ignored build tree.

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

`doctor` checks repository layout paths and the active config path. A
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

## A Shell Command Uses The Wrong Config

The interactive shell keeps the config path it selected at startup. If you
started the shell with `--config ./config/local.json`, commands typed later use
that path by default:

```bash
./build/cli-starter --config ./config/local.json shell
```

Use an inline global option when one shell command should inspect or use another
config file:

```text
starter> --config ./config/alternate.json config show
```

That inline override applies only to the dispatched command. It does not change
the prompt or the default config path for later shell commands.

## `config init` Writes Somewhere Unexpected

The global `--config <path>` option sets the default config path for
config-backed commands. `config init` writes to that path unless an explicit
`--output <path>` is supplied:

```bash
./build/cli-starter --config ./config/local.json config init
./build/cli-starter --config ./config/local.json config init --output ./config/template.local.json
```

Use `--output` when you want to create a template without changing the active
config path for the command. Prefer ignored paths such as `config/local.json` or
`config/*.local.json` for local experiments; an arbitrary output path creates a
normal worktree file that can appear in `git status`.

The generated file starts from the current `AppConfig` defaults, then applies
the configured prompt label and generated-template `notes` value. It is not a
byte-for-byte copy of `config/cli-starter.json`.

## A Config-Backed Command Reports A JSON Error

When the active config path exists, config-backed commands parse it as JSON. If
the file is malformed, fix the JSON or regenerate a local template:

```bash
./build/cli-starter --config ./config/local.json config init
./build/cli-starter --config ./config/local.json config show
```

The supported fields are `prompt`, `default_name`, `enabled_commands`, and
`notes`. Missing fields fall back to built-in defaults, and unknown top-level
fields are ignored. Malformed JSON or wrong field types are reported as command
errors. Existing config paths must point to regular files no larger than 1 MiB;
directories, device files, and oversized files are rejected before parsing.

## The Shell Exits Before Showing A Prompt

The interactive shell loads the active config before it prints the first prompt.
A missing config file is allowed and uses built-in defaults, but an existing
config file with malformed JSON or wrong field types exits with config error
status `4`:

```bash
./build/cli-starter --config ./config/local.json shell
```

Fix the JSON, correct the field types, or regenerate the local template:

```bash
./build/cli-starter --config ./config/local.json config init
```

If the config's `prompt` field is an empty string, the shell uses the configured
project prompt label instead.

## `config init` Cannot Write The Config File

`config init` creates normal parent directories for the target path, then
truncates and writes the JSON file when the target is new or an existing regular
file. It refuses symlinked parent directories, symlink targets, and non-regular
targets. If it reports a write failure, check that the parent path is writable
and that no parent or target path is a directory symlink, file symlink, or locked
by another process.

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

Arrow keys and other terminal special keys are ignored in interactive mode. They
do not move the cursor, recall command history, or add escape-sequence text to
the command line.

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
