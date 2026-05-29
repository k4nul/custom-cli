# Maintenance

This guide collects the maintainer workflow for keeping the starter copyable,
portable, and aligned with the repository-local CMake, command, config, and test
evidence.

## Baseline Validation

Use the local CMake/CTest flow before reporting source changes. Start by
checking the tracked legacy artifact patterns that `repository_hygiene` enforces:

```bash
git ls-files 'build-local-*' '.sandbox-user/*'
```

If that command returns paths that still exist in the checkout, the full
unfiltered CTest run is expected to fail in `repository_hygiene` until those
tracked generated artifacts are removed. Record that state as blocked
validation; do not use old build output or a filtered CTest run as a passing
substitute. A focused run of `starter_tests` and `cli_starter_smoke` can support
source-behavior investigation while the gate is dirty, but it must be labeled as
partial validation and paired with the artifact preflight output:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure -R '^(starter_tests|cli_starter_smoke)$'
```

If the artifact check prints no paths, run the baseline validation flow:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The tracked CI workflow at `.github/workflows/ci.yml` mirrors this validation on
Linux and Windows. CI builds with `--parallel`; the Windows job builds and tests
the `Debug` configuration. See [docs/ci.md](ci.md) for workflow triggers, exact
job commands, and failure triage.

Keep both test flags explicit in maintenance reports. `CLI_STARTER_BUILD_TESTS`
controls whether the project-specific CTest entries are registered:
`starter_tests`, `cli_starter_smoke`, and `repository_hygiene`. `BUILD_TESTING`
keeps CTest registration enabled through the repository's `include(CTest)`
setup.
Git is required for `repository_hygiene` to prove the tracked artifact gate. If
`git` is unavailable, that entry is skipped and should not be reported as a
passing artifact hygiene check. Use a verbose hygiene-only CTest run, or include
the `git` preflight commands, when normal CTest output does not make the skip
state clear:

```bash
command -v git
git rev-parse --is-inside-work-tree
ctest --test-dir build --output-on-failure -R '^repository_hygiene$' -V
```

For multi-config generators, build and test the same configuration:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

For partial validation while the artifact gate is dirty, keep the same
configuration and filter only the non-hygiene entries:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure -R "^(starter_tests|cli_starter_smoke)$"
```

CTest includes a short built-executable smoke pass and a repository hygiene
check for the tracked `build-local-*` and `.sandbox-user/*` legacy artifact
patterns when running inside a Git worktree with `git` available. The smoke pass
covers representative success commands, redirected default and explicit shell
sessions, and failure routing for parser/config errors. After changes that
affect the executable name, command registration, config paths, user-facing
command behavior, shell startup or redirected shell behavior, or parse/config
error text, these commands are useful for manual inspection too:

```bash
./build/cli-starter --version
./build/cli-starter about
./build/cli-starter doctor
./build/cli-starter --config ./config/local.json config init
./build/cli-starter --config ./config/local.json config show
./build/cli-starter --config ./config/local.json hello --name Ada
./build/cli-starter echo --numbered one two
printf 'help\nhello --name Ada\nexit\n' | ./build/cli-starter
printf 'hello\nquit\n' | ./build/cli-starter --config ./config/local.json shell
```

`doctor` is an advisory layout and config probe. A missing local config or
missing recommended layout path is reported in stdout, but CMake/CTest remains
the validation gate for reportable source changes.

Use the configuration-specific executable path on Visual Studio-style builds.

## Command Changes

Command availability is compile-time CLI wiring, not a runtime plugin system.
`configure_cli_app` registers the root `shell` command and delegates sample
commands under `src/commands/` to central registration. When adding, renaming, or
removing a sample command:

1. Add or update the implementation under `src/commands/`.
2. Add new command source files to the `starter_core` source list in `CMakeLists.txt`.
3. Update the registrar declaration in `include/starter/commands/registrars.hpp`
   using `CommandRegistrationContext`.
4. Update central registration in `src/commands/register_commands.cpp`.
5. Add or update doctest coverage under `tests/`, and update
   `cmake/cli_smoke_test.cmake` when the built executable's command path,
   output, error text, or shell dispatch changes.
6. Update `README.md`, `docs/command-reference.md`, `docs/architecture.md`,
   `docs/testing.md`, and `docs/troubleshooting.md` when user-facing behavior
   changes. Update `docs/ci.md` too when the command change modifies
   `cmake/cli_smoke_test.cmake` or CI reproduction guidance.

The `enabled_commands` field in config is serialized and shown by `config show`;
it is not a runtime allowlist. Do not document it as a way to disable commands
unless the implementation changes first.

## Config Changes

The config schema is defined by `AppConfig` in `include/starter/core/config.hpp`
and JSON parsing/serialization in `src/core/config.cpp`. The checked-in template
is `config/cli-starter.json`.

When changing config behavior:

1. Update the schema, parser, serializer, and config description together.
2. Keep `config/cli-starter.json` in sync with the defaults.
3. Add tests for default fallback, disk-loaded config, and error cases.
4. Document command examples with `--config ./config/local.json` when the example
   demonstrates the active runtime config path, or `--output ./config/local.json`
   when it demonstrates writing a template to an explicit local file.

`config init` writes a generated template that starts from `AppConfig` defaults,
then applies the configured prompt label and generated-template `notes` value.
It is not a byte-for-byte copy of `config/cli-starter.json`. Keep both paths
aligned when changing defaults, prompt naming, or generated notes.

`config/local.json` and `config/*.local.json` are ignored for local experiments.

## Dependency Updates

Header-only dependencies live under `third_party/`; this starter does not need a
package-manager bootstrap for normal builds. When updating a dependency:

1. Prefer an official release tag or release archive.
2. Update the vendored header files and the matching license file.
3. Update `third_party/README.md` with the exact version and source.
4. Re-run the unfiltered baseline validation flow so `starter_tests`,
   `cli_starter_smoke`, and, in Git worktrees with `git` available,
   `repository_hygiene` cover the update.
5. Update user-facing docs only when dependency behavior changes build, test, or
   CLI usage.

## Local Artifact Hygiene

Use ignored build directories such as `build/`, `build-linux/`,
`build-local-*`, `out/`, or `cmake-build-*` for new local work. `.sandbox-user/`
is also ignored for local sandbox and IDE telemetry. Avoid adding generated
build output, IDE telemetry, or machine-specific state to future changes.

If an older checkout contains tracked `build-local-*` or `.sandbox-user/` paths,
treat them as historical local artifacts rather than source evidence. Reconfigure
into a fresh ignored build directory before validating behavior.
Use [docs/artifact-hygiene.md](artifact-hygiene.md) as the canonical cleanup
runbook when the artifact hygiene package is intentionally selected.

Use this check when preparing maintenance reports or reviewing unexpected build
evidence:

```bash
git ls-files 'build-local-*' '.sandbox-user/*'
```

If the command returns paths that still exist in the checkout, keep
documentation and test reports anchored to fresh validation from `build/`, and
report full validation as blocked until the paths are removed. Removing those
tracked generated files is a repository cleanup package: delete the artifacts,
keep `.gitignore` coverage in place, keep the `repository_hygiene` CTest entry
passing for its configured legacy artifact patterns, rerun the baseline
validation flow, and mention the cleanup explicitly in the change summary.

Use this sequence for that cleanup package:

1. Confirm the worktree has no unrelated edits with `git status --short`.
2. List the exact tracked artifacts with
   `git ls-files 'build-local-*' '.sandbox-user/*'`.
3. Remove only those tracked generated paths:

   ```bash
   git rm -r --ignore-unmatch -- build-local-* .sandbox-user
   ```

4. Re-run `git ls-files 'build-local-*' '.sandbox-user/*'`; it should print
   nothing.
5. Run the baseline CMake/CTest flow from a fresh ignored `build/` tree.

Do not combine this cleanup with behavior changes. If validation still fails
after the tracked artifacts are gone, treat the remaining failure as a separate
build, test, or source issue.

## Documentation Changes

Keep the documentation set internally consistent:

- `README.md`: quick start, built-in commands, config, customization, and docs
  map.
- `docs/project-overview.md`: repository purpose, scope, intended users, and
  legacy boundary.
- `docs/command-reference.md`: global options, built-in commands, config
  fields, shell behavior, and exit statuses.
- `docs/onboarding.md`: first local build, smoke test, shell use, and first
  customization loop.
- `docs/architecture.md`: component layout, command flow, and extension points.
- `docs/testing.md`: validation commands, current coverage, and test gaps.
- `docs/artifact-hygiene.md`: tracked local artifact gate, cleanup sequence, and
  reporting states.
- `docs/ci.md`: GitHub Actions triggers, job commands, and failure triage.
- `docs/troubleshooting.md`: known build, config, and runtime failures.
- `docs/maintenance.md`: maintainer validation, change checklists, artifact
  hygiene, and CI workflow expectations.
- `docs/migration-from-legacy.md`: historical migration context.
- `third_party/README.md`: vendored dependency versions, source URLs, license
  links, and dependency update guidance.

When a command, config field, CMake cache variable, or validation path changes,
update the nearest docs in the same change. When a known test gap is closed,
move it from `docs/testing.md`'s gap list into the current coverage summary.

## CI Workflow

The tracked GitHub Actions workflow lives at `.github/workflows/ci.yml` and
mirrors the baseline validation flow. It runs one Linux single-config CMake job
and one Windows Visual Studio-style multi-config job so both executable layouts
stay documented and tested. Because CI runs unfiltered CTest, tracked
`build-local-*` or `.sandbox-user/*` paths make CI fail in `repository_hygiene`
until the artifact cleanup package removes them.

Use [docs/ci.md](ci.md) as the CI runbook for triggers, permissions, exact job
commands, local reproduction, and workflow update rules.
