# Troubleshooting

These notes cover local setup issues that can be diagnosed from the starter's
checked-in CMake, config, and command behavior.

## CTest Cannot Find `starter_tests`

Make sure tests were enabled at configure time and the build completed:

```bash
cmake -S . -B build -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R starter_tests --output-on-failure
```

`CLI_STARTER_BUILD_TESTS` defaults to `BUILD_TESTING`, so a toolchain or preset
that disables `BUILD_TESTING` also disables the `starter_tests` target unless
you pass `-DCLI_STARTER_BUILD_TESTS=ON`.

## Multi-Config Tests Fail To Start

Visual Studio and other multi-config generators put binaries under a
configuration directory. Build and test the same configuration:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug -R starter_tests --output-on-failure
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

## Interactive Input Reports An Input Error

The interactive shell tokenizes input before dispatching commands. Unterminated
or malformed quotes are reported as input errors and do not exit the shell.
Re-enter the command with matching quotes:

```text
starter> hello --name "Ada Lovelace"
```
