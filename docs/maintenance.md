# Maintenance

This guide collects the maintainer workflow for keeping the starter copyable,
portable, and aligned with the repository-local CMake, command, config, and test
evidence.

## Baseline Validation

Use the local CMake/CTest flow as the authoritative validation path until a CI
workflow is added:

```bash
cmake -S . -B build -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R starter_tests --output-on-failure
```

For multi-config generators, build and test the same configuration:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug -R starter_tests --output-on-failure
```

After changes that affect the executable name, command registration, config
paths, or user-facing command behavior, also run a short CLI smoke pass:

```bash
./build/cli-starter --version
./build/cli-starter about
./build/cli-starter doctor
./build/cli-starter --config ./config/local.json config init
./build/cli-starter --config ./config/local.json config show
```

Use the configuration-specific executable path on Visual Studio-style builds.

## Command Changes

Command availability is compile-time registration, not a runtime plugin system.
When adding, renaming, or removing a command:

1. Add or update the implementation under `src/commands/`.
2. Update the registrar declaration in `include/starter/commands/registrars.hpp`.
3. Update central registration in `src/commands/register_commands.cpp`.
4. Add or update doctest coverage under `tests/`.
5. Update `README.md`, `docs/architecture.md`, `docs/testing.md`, and
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

`config/local.json` and `config/*.local.json` are ignored for local experiments.

## Dependency Updates

Header-only dependencies live under `third_party/`; this starter does not need a
package-manager bootstrap for normal builds. When updating a dependency:

1. Prefer an official release tag or release archive.
2. Update the vendored header files and the matching license file.
3. Update `third_party/README.md` with the exact version and source.
4. Re-run the baseline validation flow.
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

If the command returns paths, keep documentation and test reports anchored to
fresh validation from `build/`. Removing those tracked generated files is a
repository cleanup package: delete the artifacts, keep `.gitignore` coverage in
place, rerun the baseline validation flow, and mention the cleanup explicitly in
the change summary.

## Documentation Changes

Keep the documentation set internally consistent:

- `README.md`: quick start, built-in commands, config, customization, and docs
  map.
- `docs/onboarding.md`: first local build, smoke test, shell use, and first
  customization loop.
- `docs/architecture.md`: component layout, command flow, and extension points.
- `docs/testing.md`: validation commands, current coverage, and test gaps.
- `docs/troubleshooting.md`: known build, config, and runtime failures.
- `docs/migration-from-legacy.md`: historical migration context.

When a command, config field, CMake cache variable, or validation path changes,
update the nearest docs in the same change.

## CI Bootstrap Expectations

No workflow file is tracked yet, so local CMake/CTest remains authoritative. A
future CI package should start with the same configure, build, and CTest commands
used locally. A practical first matrix is one Linux compiler job plus one
Windows Visual Studio-style multi-config job so both executable layouts stay
documented and tested.
