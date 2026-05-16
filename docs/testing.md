# Testing

The starter uses CTest for test discovery and doctest for assertions. Both the
application and tests link through the `starter_core` library, so tests can
exercise command behavior without spawning the built executable.

## Test Targets

`CMakeLists.txt` defines one test executable:

- `starter_tests`: builds from `tests/config_tests.cpp`

The target is created only when `CLI_STARTER_BUILD_TESTS` is enabled. That
option defaults to CMake's `BUILD_TESTING` value because the project includes
`CTest`. Keep `BUILD_TESTING` enabled too; a cache or preset that turns it off
can prevent CTest from registering the executable even if the test target is
compiled.

## Standard Validation

Use this flow for the normal local validation pass:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R starter_tests --output-on-failure
```

No CI workflow is tracked in this repository yet, so this local CMake/CTest flow
is the authoritative validation path until CI is added. See
`docs/maintenance.md` for the expected first CI shape.

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
ctest --test-dir build -C Debug -R starter_tests --output-on-failure
```

Running through CTest is preferred because it matches the CMake-registered test
name, but the generated test executable can also be useful while iterating:

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
- `about` and `doctor` smoke behavior,
- scripted interactive shell lifecycle coverage for no-argv shell entry, disk-backed prompts,
  shell-only help/exit handling, malformed input recovery, and command dispatch reuse,
- root command completion,
- subcommand completion for `config init` and `config show`, and
- option completion for `hello --name`, `hello -e`, and `hello --enthusiastic`.

## Known Gaps

Add focused coverage when work touches these areas:

- raw terminal line editing behavior that depends on platform TTY APIs,
- platform-specific config permission or locked-file failures that need OS-specific setup, and
- future CI behavior once workflow files are added.

## Adding Tests

For a small command or helper change, add a focused doctest case to
`tests/config_tests.cpp`. If a new test file is clearer, add it to the
`starter_tests` target in `CMakeLists.txt` so CTest continues to expose a single
starter validation target.

Prefer tests that call the same application dispatch path as real CLI usage
when the behavior is user-facing. For pure helpers, test the helper directly.

## CLI Smoke Checks

CTest covers reusable internals and command dispatch. A manual CLI smoke pass
is still useful after renaming the starter or changing command registration:

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
