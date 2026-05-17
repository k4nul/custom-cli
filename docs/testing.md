# Testing

The starter uses CTest for test discovery and doctest for assertions. Most
behavior tests link through the `starter_core` library, and a CTest smoke case
also runs the built executable to catch packaging and command-wiring regressions.

## Test Targets

`CMakeLists.txt` defines one test executable and two CTest entries:

- `starter_tests`: builds from `tests/config_tests.cpp`
- `cli_starter_smoke`: runs the built CLI through version, about, doctor,
  config, hello, and echo smoke checks

The test target and smoke entry are created only when `CLI_STARTER_BUILD_TESTS`
is enabled. That option defaults to CMake's `BUILD_TESTING` value because the
project includes `CTest`. Keep `BUILD_TESTING` enabled too; a cache or preset
that turns it off can prevent CTest from registering tests even if the test
target is compiled.

## Standard Validation

Use this flow for the normal local validation pass:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Leave the CTest command unfiltered for reportable validation so both
`starter_tests` and `cli_starter_smoke` run. Use focused doctest filters only
for local iteration.

The tracked GitHub Actions workflow at `.github/workflows/ci.yml` runs the same
CMake/CTest validation on Linux and Windows. Report local results from the flow
above before publishing source changes.

Report test results from a build tree created for the current validation pass.
Do not use existing `build-local-*` executables or cached CTest files as proof
that the current source still builds, even when those paths appear in the
checkout. The ignore rules prevent new local artifacts from being added, but
they do not make historical tracked artifacts authoritative.

Check for those paths when validation evidence looks suspicious:

```bash
git ls-files 'build-local-*' '.sandbox-user/*'
```

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

- tokenizing quoted shell input and reporting malformed shell input,
- preserving empty quoted shell arguments so interactive input matches normal argv behavior,
- JSON config serialization, parsing, strict top-level object validation, and typed read/write failures,
- custom config paths for config-backed commands,
- `config init` output-path behavior and write failure reporting,
- `config show` fallback output when no config file exists,
- `config show` defaults for omitted disk config fields,
- malformed, non-object, and wrong-type disk config errors for `config show` and config-backed `hello`,
- `hello` command dispatch through `Application`, including missing-config guidance,
- `echo` command dispatch for positional text, uppercase output, numbered output,
  and combined uppercase numbered output,
- top-level `--version`, `--help`, `--help-all`, and parse-error stream routing,
- CLI11 validation errors for missing `echo` text, unknown options, and missing `config` subcommands,
- `about` output and `doctor` behavior for healthy layouts, missing recommended layout paths,
  missing local config warnings, malformed config JSON, and wrong-type config fields,
- scripted interactive shell lifecycle coverage for no-argv shell entry, disk-backed prompts,
  shell-only `help`, `exit`, and `quit` handling, malformed input recovery, and command dispatch reuse,
- root command completion, including registered CLI commands plus shell-only `help`, `exit`, and `quit`,
- subcommand completion for `config init` and `config show`,
- scoped option completion for root options, `hello`, and `config init`, and
- completion fallback to root options when earlier shell context is malformed.

The `cli_starter_smoke` CTest entry covers the built executable path for
`--version`, `about`, `doctor`, config initialization and display, `hello`, and
numbered `echo`.

## Known Gaps

Add focused coverage when work touches these areas:

- raw terminal line editing behavior that depends on platform TTY APIs, and
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
build directories such as `build/`, `build-local-*`, or `cmake-build-*`, and
keep local config experiments in `config/local.json` or `config/*.local.json`.

If generated `build-local-*` or `.sandbox-user/` paths appear from an older
checkout, do not use them as validation evidence. Rebuild into a fresh ignored
directory before reporting test results, and leave actual artifact removal to a
separate non-documentation cleanup change.
