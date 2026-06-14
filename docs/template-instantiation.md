# Template Instantiation

Use `scripts/instantiate_template.py` after copying this starter into a new
repository and before replacing the sample commands. The script keeps the rename
workflow in one executable place: it validates safe project names, prints the
CMake cache variables needed for the renamed build, and can write the matching
`config/<name>.json` template.

This is the first step of instantiating a copied starter, not a whole-repository
rename. It changes generated build metadata and can create the renamed config
template. It does not rename `project(CLIStarter)`, `starter_core`,
`include/starter/`, the `starter` namespace, documentation examples, or sample
command behavior. Replace those intentionally when the copied project starts to
become a real application.

## Plan The Rename

Plan a rename without changing files:

```bash
python3 scripts/instantiate_template.py \
  --binary-name my-cli \
  --display-name "My CLI" \
  --config-file my-cli.json \
  --prompt-label mycli
```

When optional values are omitted, the script derives them from
`--binary-name`: `my-cli` becomes display name `My Cli`, config file
`my-cli.json`, and prompt label `mycli`.

Use `--build-dir <name>` when the copied project should validate in a build
directory other than `build`. The build directory is validated as a single safe
directory name, not a nested path.

Use `--json` when another local script needs the generated plan. The JSON
output includes the CMake configure command, build command, CTest command, full
validation command, config path, and written config path when `--write-config`
is used.

## Write The Config Template

Create the matching config template in a copied checkout:

```bash
python3 scripts/instantiate_template.py \
  --binary-name my-cli \
  --display-name "My CLI" \
  --config-file my-cli.json \
  --prompt-label mycli \
  --write-config
```

By default, `--write-config` writes under the current directory. Use
`--repo-root /path/to/copied/starter` when running the helper from another
working directory:

```bash
python3 scripts/instantiate_template.py \
  --binary-name my-cli \
  --repo-root /path/to/copied/starter \
  --write-config
```

The generated file contains the renamed prompt label, the starter default name,
the default enabled-command list, and generated-template notes. Commit
`config/<name>.json` when it is the copied project's intended default runtime
config. Then update docs and examples that still point at
`config/cli-starter.json`, or keep the old template only while migration is
intentional.

Then run the printed validation command. It configures the renamed executable,
builds it, and runs the CTest suite with output-on-failure enabled.

## Safety Rules

The script intentionally refuses path-like names, control characters, unsafe
prompt labels, and non-JSON config file names. It also refuses to replace an
existing config file unless `--force` is passed, refuses symlink or non-regular
output files, and refuses a symlinked or non-directory `config/` path before
writing. That keeps `--write-config` from following a copied checkout's
unexpected local filesystem redirection.

Safe token values must start with a letter or digit and then use only letters,
digits, `.`, `_`, or `-`. `--display-name` may contain spaces, but it must not
be blank or contain control characters. `--config-file` must end in `.json`.

Only use `--force` after checking the existing generated config. It replaces a
regular file at `config/<name>.json`; it still refuses symlinks, directories,
and other non-regular paths.

## Validation

Start with the repository artifact preflight used by the normal validation
flow:

```bash
git ls-files 'build-local-*' '.sandbox-user/*'
```

If that command prints no paths, run the validation command printed by the
helper. It has this shape:

```bash
cmake -S . -B build \
  -DCLI_STARTER_BINARY_NAME=my-cli \
  -DCLI_STARTER_DISPLAY_NAME="My CLI" \
  -DCLI_STARTER_CONFIG_FILE=my-cli.json \
  -DCLI_STARTER_PROMPT_LABEL=mycli \
  -DBUILD_TESTING=ON \
  -DCLI_STARTER_BUILD_TESTS=ON && \
cmake --build build && \
ctest --test-dir build --output-on-failure
```

For multi-config generators, build and test the same configuration:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Validate this workflow with:

```bash
python3 tests/instantiate_template_tests.py
```

The normal CMake/CTest validation flow also runs these tests through the
`template_instantiation_workflow` CTest entry:

```bash
ctest --test-dir build --output-on-failure -R '^template_instantiation_workflow$'
```
