# Testing

The starter uses CTest for test discovery and doctest for assertions. Most
behavior tests link through the `starter_core` library, a CTest smoke case runs
the built executable to catch packaging and command-wiring regressions, and a
repository hygiene case blocks the tracked legacy artifact patterns it is
configured to inspect.

## Test Targets

`CMakeLists.txt` defines one test executable and three CTest entries:

- `starter_tests`: builds from `tests/config_tests.cpp`
- `cli_starter_smoke`: runs the built CLI through version, about, doctor,
  config, hello, and echo smoke checks
- `repository_hygiene`: when running inside a Git worktree with `git` available,
  fails if tracked legacy artifact paths matching `build-local-*` or
  `.sandbox-user/*` are still present in the checkout

The test target and CTest entries are created only when
`CLI_STARTER_BUILD_TESTS` is enabled. That option defaults to CMake's
`BUILD_TESTING` value because the project includes `CTest`. Keep
`BUILD_TESTING` enabled too; a cache or preset that turns it off can prevent
CTest from registering tests even if the test target is compiled.

## Standard Validation

Start the normal local validation pass by checking the tracked artifact patterns
that `repository_hygiene` enforces:

```bash
git ls-files 'build-local-*' '.sandbox-user/*'
```

Report test results from a build tree created for the current validation pass.
Do not use existing `build-local-*` executables or cached CTest files as proof
that the current source still builds, even when those paths appear in the
checkout. The ignore rules prevent new local artifacts from being added, but
they do not make historical tracked artifacts authoritative.

If the artifact check returns paths that still exist in the checkout, full
unfiltered validation is blocked because `repository_hygiene` is expected to fail
until the tracked generated artifacts are removed. Build and focused test runs
can still help diagnose source changes, but report them as partial evidence and
keep the artifact cleanup as a separate repository hygiene task.

If the artifact check prints no paths, use this flow:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Leave the CTest command unfiltered for reportable validation so
`starter_tests`, `cli_starter_smoke`, and `repository_hygiene` run. Use focused
doctest filters only for local iteration.

The tracked GitHub Actions workflow at `.github/workflows/ci.yml` mirrors this
validation on Linux and Windows. The Linux job configures, builds with
`cmake --build build --parallel`, and runs unfiltered CTest; the Windows job uses
the same configure step, then builds `Debug` with `--parallel` and runs
`ctest --test-dir build -C Debug --output-on-failure`. Report local results from
the flow above before publishing source changes.

For multi-config generators, build and test the same configuration:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Running through CTest is preferred because it exercises the registered suite,
but the generated test executable can also be useful while iterating:

```bash
./build/starter_tests
```

For Windows single-config generators:

```powershell
.\build\starter_tests.exe
```

For Visual Studio-style multi-config layouts:

```powershell
.\build\Debug\starter_tests.exe
```

## Current Coverage

`tests/config_tests.cpp` covers the starter's reusable behavior:

- tokenizing shell input, including quoted groups, empty quoted arguments, mixed
  whitespace, escaped characters, adjacent quoted and unquoted fragments,
  malformed input, and shared token joining helpers,
- JSON config serialization, parsing, strict top-level object validation, and typed read/write failures,
- custom config paths for config-backed commands and explicit `hello --name`
  overrides for disk config defaults,
- `config init` output-path behavior and write failure reporting,
- `config show` fallback output when no config file exists,
- `config show` defaults for omitted disk config fields,
- malformed, non-object, and wrong-type disk config errors for `config show` and config-backed `hello`,
- informational `enabled_commands` behavior, including config-backed commands
  still running when omitted from the list and non-config-backed commands still
  running when the list names only another command,
- `hello` command dispatch through `Application`, including enthusiastic output,
  config-backed default names, explicit-name overrides, and missing-config guidance,
- `echo` command dispatch for positional text, uppercase output, numbered output,
  and combined uppercase numbered output,
- top-level `--version`, `--help`, `--help-all`, and parse-error stream routing,
- CLI11 validation errors for missing `echo` text, unknown options, and missing `config` subcommands,
- `about` output and `doctor` behavior for healthy layouts, missing recommended layout paths,
  missing local config warnings, malformed config JSON, and wrong-type config fields,
- scripted interactive shell lifecycle coverage for no-argv shell entry, EOF,
  blank input, disk-backed prompts, shell-only `help`, `exit`, and `quit`
  handling, command-specific help routing, malformed input recovery, parse-error
  recovery, command-failure recovery, and command dispatch reuse,
- root command completion, including registered CLI commands plus shell-only `help`, `exit`, and `quit`,
- subcommand completion for `config init` and `config show`,
- scoped option completion for root options, `hello`, and `config init`, and
- completion fallback to root options when earlier shell context is malformed,
- completion replacement ranges based on cursor position,
- trailing-space subcommand suggestions,
- shared-prefix completion priming, and
- primed-state reset after replacement and listing actions.

The `cli_starter_smoke` CTest entry covers the built executable path for
`--version`, `about`, `doctor`, config initialization and display, `hello`, and
numbered `echo`. Each smoke case must exit successfully, match its expected
stdout pattern, and leave stderr empty. These checks intentionally cover the
default success path; when display metadata, about text, command registration,
or config behavior changes, update `cmake/cli_smoke_test.cmake` with the related
docs and tests.

When it runs inside a Git worktree with `git` available, the
`repository_hygiene` CTest entry checks the checkout for tracked legacy artifact
paths matching `build-local-*` and `.sandbox-user/*`, so old generated files do
not become source or validation evidence again.

## Known Gaps

Add focused coverage when work touches these areas:

- raw terminal line editing behavior that depends on platform TTY APIs,
- the real redirected-input fallback path in `read_shell_line`, beyond the
  injected scripted shell reader used by unit tests,
- built-executable negative/error-path smoke cases for unknown commands, missing
  arguments, bad config, and stderr routing, and
- platform-specific config permission or locked-file failures that need OS-specific setup.

## Adding Tests

For a small command or helper change, add a focused doctest case to
`tests/config_tests.cpp`. If a new test file is clearer, add it to the
`starter_tests` target in `CMakeLists.txt`. If a built-executable smoke case
needs to change, update `cmake/cli_smoke_test.cmake` in the same package.

Prefer tests that call the same application dispatch path as real CLI usage
when the behavior is user-facing. For pure helpers, test the helper directly.

## Focused Doctest Iteration

After building `starter_tests`, run the whole suite through CTest for reportable
validation. While iterating on one behavior, the doctest executable can list and
filter registered test cases directly:

```bash
./build/starter_tests --list-test-cases
./build/starter_tests --test-case="application reports unknown options through stderr"
```

Use the configuration-specific executable path on multi-config generators, such
as `.\build\Debug\starter_tests.exe`.

## CLI Smoke Checks

CTest covers reusable internals, command dispatch, and a short built-executable
smoke pass. A manual CLI pass is still useful after renaming the starter or
changing command registration:

```bash
./build/cli-starter --version
./build/cli-starter about
./build/cli-starter doctor
./build/cli-starter --config ./config/local.json config init
./build/cli-starter --config ./config/local.json config show
./build/cli-starter --config ./config/local.json hello --name Ada
./build/cli-starter echo --numbered one two
```

Use the configuration-specific executable path on multi-config generators.

## Expected Local Files

Keep generated build trees and local configs out of commits. Prefer ignored
build directories such as `build/`, `build-linux/`, `build-local-*`, `out/`, or
`cmake-build-*`; keep sandbox telemetry under `.sandbox-user/`; and keep local
config experiments in `config/local.json` or `config/*.local.json`.

If generated `build-local-*` or `.sandbox-user/` paths appear from an older
checkout, do not use them as validation evidence. Rebuild into a fresh ignored
directory before reporting test results, and leave actual artifact removal to a
separate repository cleanup change.

For that cleanup change, the success condition is mechanical and should be easy
to report:

```bash
git ls-files 'build-local-*' '.sandbox-user/*'
```

The command should print no paths after the cleanup. Then run the standard
unfiltered CTest flow from a fresh ignored `build/` tree so
`repository_hygiene` can confirm, in Git worktrees with `git` available, that
tracked generated artifacts no longer block validation.
