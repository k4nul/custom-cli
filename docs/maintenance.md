# Maintenance

This guide collects the maintainer workflow for keeping the starter copyable,
portable, and aligned with the repository-local CMake, command, config, and test
evidence.

## Baseline Validation

Use the local CMake/CTest flow before reporting source changes. The tracked CI
workflow at `.github/workflows/ci.yml` runs the same validation on Linux and
Windows:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Keep both test flags explicit in maintenance reports. `CLI_STARTER_BUILD_TESTS`
controls whether the project-specific CTest entries are registered:
`starter_tests`, `cli_starter_smoke`, and `repository_hygiene`. `BUILD_TESTING`
keeps CTest registration enabled through the repository's `include(CTest)`
setup.

For multi-config generators, build and test the same configuration:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

CTest includes a short built-executable smoke pass and a repository hygiene
check for ignored local artifacts. After changes that affect the executable
name, command registration, config paths, or user-facing command behavior,
these commands are useful for manual inspection too:

```bash
./build/cli-starter --version
./build/cli-starter about
./build/cli-starter doctor
./build/cli-starter --config ./config/local.json config init
./build/cli-starter --config ./config/local.json config show
```

`doctor` is an advisory layout and config probe. A missing local config or
missing recommended layout path is reported in stdout, but CMake/CTest remains
the validation gate for reportable source changes.

If `git ls-files 'build-local-*' '.sandbox-user/*'` returns paths that still
exist in the checkout, the full unfiltered CTest run is expected to fail in
`repository_hygiene` until those tracked generated artifacts are removed. Record
that state as blocked validation; do not use old build output or a filtered
CTest run as a passing substitute.

Use the configuration-specific executable path on Visual Studio-style builds.

## Command Changes

Command availability is compile-time CLI wiring, not a runtime plugin system.
`Application` registers the root `shell` command directly, while sample commands
under `src/commands/` use central registration. When adding, renaming, or
removing a sample command:

1. Add or update the implementation under `src/commands/`.
2. Add new command source files to the `starter_core` source list in `CMakeLists.txt`.
3. Update the registrar declaration in `include/starter/commands/registrars.hpp`.
4. Update central registration in `src/commands/register_commands.cpp`.
5. Add or update doctest coverage under `tests/`.
6. Update `README.md`, `docs/architecture.md`, `docs/testing.md`, and
   `docs/troubleshooting.md` when user-facing behavior changes.

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
   writes a local file.

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
   `cli_starter_smoke`, and `repository_hygiene` cover the update.
5. Update user-facing docs only when dependency behavior changes build, test, or
   CLI usage.

## Local Artifact Hygiene

Use ignored build directories such as `build/`, `build-local-*`, or
`cmake-build-*` for new local work. `.sandbox-user/` is also ignored for local
sandbox and IDE telemetry. Avoid adding generated build output, IDE telemetry,
or machine-specific state to future changes.

If an older checkout contains tracked `build-local-*` or `.sandbox-user/` paths,
treat them as historical local artifacts rather than source evidence. Reconfigure
into a fresh ignored build directory before validating behavior.

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
passing, rerun the baseline validation flow, and mention the cleanup explicitly
in the change summary.

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
- `docs/command-reference.md`: global options, built-in commands, config
  fields, shell behavior, and exit statuses.
- `docs/onboarding.md`: first local build, smoke test, shell use, and first
  customization loop.
- `docs/architecture.md`: component layout, command flow, and extension points.
- `docs/testing.md`: validation commands, current coverage, and test gaps.
- `docs/troubleshooting.md`: known build, config, and runtime failures.
- `docs/maintenance.md`: maintainer validation, change checklists, artifact
  hygiene, and CI workflow expectations.
- `docs/migration-from-legacy.md`: historical migration context.

When a command, config field, CMake cache variable, or validation path changes,
update the nearest docs in the same change. When a known test gap is closed,
move it from `docs/testing.md`'s gap list into the current coverage summary.

## CI Workflow

The tracked GitHub Actions workflow lives at `.github/workflows/ci.yml` and
mirrors the baseline validation flow. It runs one Linux single-config CMake job
and one Windows Visual Studio-style multi-config job so both executable layouts
stay documented and tested.
