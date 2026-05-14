# Testing

The starter uses CTest for test discovery and doctest for assertions. Both the
application and tests link through the `starter_core` library, so tests can
exercise command behavior without spawning the built executable.

## Test Targets

`CMakeLists.txt` defines one test executable:

- `starter_tests`: builds from `tests/config_tests.cpp`

The target is created only when `CLI_STARTER_BUILD_TESTS` is enabled. That
option defaults to CMake's `BUILD_TESTING` value because the project includes
`CTest`.

## Standard Validation

Use this flow for the normal local validation pass:

```bash
cmake -S . -B build -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R starter_tests --output-on-failure
```

No CI workflow is tracked in this repository yet, so this local CMake/CTest flow
is the authoritative validation path until CI is added. See
`docs/maintenance.md` for the expected first CI shape.

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

For Visual Studio-style layouts:

```powershell
.\build\Debug\starter_tests.exe
```

## Current Coverage

`tests/config_tests.cpp` covers the starter's reusable behavior:

- tokenizing quoted shell input and reporting malformed shell input,
- JSON config serialization and parsing,
- custom config paths for config-backed commands,
- `config init` output-path behavior,
- `config show` fallback output when no config file exists,
- `hello` command dispatch through `Application`, including missing-config guidance,
- `echo` command dispatch for positional text, uppercase output, numbered output,
  and combined uppercase numbered output,
- top-level `--version`, `--help`, and parse-error stream routing,
- `about` and `doctor` smoke behavior,
- root command completion,
- subcommand completion for `config init` and `config show`, and
- option completion for `hello --name`, `hello -e`, and `hello --enthusiastic`.

## Known Gaps

Add focused coverage when work touches these areas:

- full interactive shell lifecycle with real input/output,
- `--help-all` output content,
- malformed JSON config files,
- config read/write failures,
- missing required `echo` text and other CLI11 validation errors, and
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
build directories such as `build/` or `cmake-build-*`, and keep local config
experiments in `config/local.json` or `config/*.local.json`.

If generated `build-local-*` or `.sandbox-user/` paths appear in a checkout,
do not use them as validation evidence. Rebuild into a fresh ignored directory
before reporting test results.
