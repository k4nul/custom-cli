# Template Instantiation

Use `scripts/instantiate_template.py` after copying this starter into a new
repository and before replacing the sample commands. The script keeps the first
rename workflow in one executable place: it validates safe project names, prints
the CMake cache variables needed for the renamed build, and can write the
matching `config/<name>.json` template.

This is the supported first step of instantiating a copied starter, not a
whole-repository rename. It changes generated build metadata and can create the
renamed config template. It does not rename `project(CLIStarter)`,
`starter_core`, `include/starter/`, the `starter` namespace, documentation
examples, or sample command behavior. Replace those intentionally when the
copied project starts to become a real application.

## Workflow Summary

Use this sequence in a copied checkout:

1. Pick the public runtime names: executable name, display name, default config
   file name, and shell prompt label.
2. Run the helper without `--write-config` to inspect the generated CMake and
   validation commands.
3. Run the printed validation command from a fresh ignored build directory.
4. Re-run the helper with `--write-config` when the copied project is ready to
   use the renamed default config file.
5. Inspect and commit the generated `config/<name>.json` template with any docs
   and command examples that should now point at the renamed config.
6. Replace internal source identifiers, namespaces, sample commands, and
   documentation examples in separate intentional changes.

Keep the plan step and the write step separate unless the copied project already
has a clear rename decision. The dry plan is useful for checking derived
defaults and command quoting without changing the checkout.

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

Example JSON plan:

```bash
python3 scripts/instantiate_template.py \
  --binary-name my-cli \
  --display-name "My CLI" \
  --json
```

The JSON output is for local automation and review. It does not execute CMake,
build the project, run CTest, or write files unless `--write-config` is also
passed.

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

`--write-config` only creates or replaces the runtime config template. It does
not update source files, CMake project names, command registration, README
examples, command reference examples, CI commands, or test expectations. When
the copied project switches to the renamed config by default, update those
reader-facing references in the same documentation package that commits the new
config template.

After writing, inspect the local diff before committing:

```bash
git status --short
cat config/my-cli.json
```

Then run the printed validation command. It configures the renamed executable,
builds it, and runs the CTest suite with output-on-failure enabled.

If the copied checkout still has examples that refer to `cli-starter` or
`config/cli-starter.json`, decide whether they are still intentional starter
references or should be replaced by the copied project's names. Do not treat the
helper as evidence that every repository reference has been renamed.

## Safety Rules

The script intentionally refuses path-like names, control characters, unsafe
prompt labels, display names that cannot be written safely to the generated C++
project header, and non-JSON config file names. It also refuses to create files
under a missing, non-directory, or symlinked repository root, refuses to replace
an existing config file unless `--force` is passed, refuses symlink or
non-regular output files, and refuses a symlinked or non-directory `config/`
path before writing. That keeps `--write-config` from following a copied
checkout's unexpected local filesystem redirection.

Safe token values must start with a letter or digit and then use only letters,
digits, `.`, `_`, or `-`. `--display-name` may contain spaces, but it must not
be blank, contain control characters, or contain double quotes or backslashes.
`--config-file` must end in `.json`.

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

If the artifact preflight prints tracked paths, report the rename validation as
blocked until those legacy generated artifacts are removed. The CTest
`repository_hygiene` entry fails for matching tracked paths that still exist in
the checkout; the completion checker treats any matching `git ls-files` output
as a preflight blocker. Record the preflight output with the rename plan and
treat artifact cleanup as a separate package. When source-behavior evidence is
still needed before cleanup, run a fresh ignored build tree and filter only the
non-hygiene entries:

```bash
cmake -S . -B build \
  -DCLI_STARTER_BINARY_NAME=my-cli \
  -DCLI_STARTER_DISPLAY_NAME="My CLI" \
  -DCLI_STARTER_CONFIG_FILE=my-cli.json \
  -DCLI_STARTER_PROMPT_LABEL=mycli \
  -DBUILD_TESTING=ON \
  -DCLI_STARTER_BUILD_TESTS=ON && \
cmake --build build && \
ctest --test-dir build --output-on-failure -R '^(starter_tests|template_instantiation_workflow|cli_starter_smoke)$'
```

Label that result as partial validation and include the artifact family counts
when the preflight listing is long:

```bash
git ls-files 'build-local-*' '.sandbox-user/*' | cut -d/ -f1 | sort | uniq -c
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

When the helper behavior changes, keep these files aligned in the same change:

- `scripts/instantiate_template.py`
- `tests/instantiate_template_tests.py`
- `docs/template-instantiation.md`
- `docs/onboarding.md`, when the first customization loop changes
- `docs/testing.md` and `docs/maintenance.md`, when validation or maintenance
  expectations change
