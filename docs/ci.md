# CI Workflow

The tracked GitHub Actions workflow is `.github/workflows/ci.yml`. It is a
portable mirror of the local CMake/CTest validation flow, not a separate build
system. Keep this document, `docs/testing.md`, and `docs/maintenance.md` in sync
when the workflow changes.

## Triggers And Permissions

The workflow runs on:

- pushes,
- pull requests, and
- manual `workflow_dispatch` runs.

It grants read-only repository contents permission. The jobs do not publish
artifacts, mutate repository contents, or require project secrets.

## Jobs

The workflow has two jobs:

| Job | Runner | Timeout | Build layout |
| --- | --- | --- | --- |
| `linux` | `ubuntu-latest` | 15 minutes | single-config CMake build under `build/` |
| `windows` | `windows-latest` | 20 minutes | Visual Studio-style `Debug` configuration under `build/` |

Both jobs check out the repository, configure with tests enabled, build, and run
unfiltered CTest.

Linux commands:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Windows commands:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
```

## Relationship To Local Validation

Use the same configure flags locally before reporting source changes. Collect
reportable results from a build tree created for that validation pass; if
`build/` already exists from an earlier run, remove it or choose another ignored
build directory first. The unfiltered CTest run covers the registered entries
from `CMakeLists.txt`: `starter_tests`, `template_instantiation_workflow`,
`shell_completion_generation`, `command_manpage_generation`,
`command_metadata_json_generation`, `command_markdown_reference_generation`,
`cli_starter_smoke`, and
`repository_hygiene`. The hygiene entry requires
`.editorconfig` and `.gitattributes`, then uses Git to inspect tracked local
artifact paths.

Start local reports with the policy-file and artifact preflight:

```bash
test -f .editorconfig
test -f .gitattributes
git ls-files 'build-local-*' '.sandbox-user/*'
```

When either policy file is missing, or when the artifact command prints tracked
paths that still exist in the checkout, CI is expected to fail in
`repository_hygiene` until the dedicated cleanup package fixes the gate. Local
source-behavior investigation can still run the non-hygiene entries, but report
that result as partial validation and include the preflight output. When the
artifact listing is long, include the path count and top-level artifact families
too:

```bash
git ls-files 'build-local-*' '.sandbox-user/*' | cut -d/ -f1 | sort | uniq -c
```

Then run the non-hygiene entries from a fresh ignored build tree:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure -R '^(starter_tests|project_completion_state|project_completion_state_regression|template_instantiation_workflow|shell_completion_generation|command_manpage_generation|command_metadata_json_generation|command_markdown_reference_generation|cli_starter_smoke)$'
```

Do not filter CI's CTest command to hide the hygiene failure. The workflow is
the reportable full-validation gate, so tracked local artifacts should be fixed
by the cleanup runbook in `docs/artifact-hygiene.md`.

## Debugging Failures

- If configure fails, reproduce the same `cmake -S . -B build` command and
  confirm the local CMake version is at least 3.18 and that CMake can find a
  Python 3 interpreter for `template_instantiation_workflow`.
- If build fails, reproduce the same build layout as the failing job. Use
  `--config Debug` for Windows multi-config failures.
- If `starter_tests` fails, run `ctest --test-dir build --output-on-failure -R
  '^starter_tests$'` and then use focused doctest filters from
  `docs/testing.md`. For the Windows workflow layout, include the matching
  configuration:

  ```powershell
  ctest --test-dir build -C Debug --output-on-failure -R '^starter_tests$'
  ```

- If `cli_starter_smoke` fails, inspect the command path, stdout/stderr
  expectations, redirected shell behavior, and config examples in
  `cmake/cli_smoke_test.cmake`.
- If `template_instantiation_workflow` fails, rerun that CTest entry and then
  run the helper tests directly when investigating outside CTest:

  ```bash
  ctest --test-dir build --output-on-failure -R '^template_instantiation_workflow$'
  python3 tests/instantiate_template_tests.py
  ```

- If `repository_hygiene` fails, first confirm `.editorconfig` and
  `.gitattributes` are present, then run `git ls-files 'build-local-*'
  '.sandbox-user/*'` and follow `docs/artifact-hygiene.md` for tracked generated
  artifact cleanup.

## When To Update CI

Update `.github/workflows/ci.yml` and the related docs in the same change when:

- the required CMake version, configure flags, build directory, or executable
  layout changes,
- CTest entries are added, removed, renamed, or intentionally filtered,
- command registration, config behavior, shell behavior, or stderr/stdout
  routing changes in a way that updates `cmake/cli_smoke_test.cmake`,
- artifact hygiene patterns change in `cmake/repository_hygiene_test.cmake`, or
- supported platforms change.

The current workflow intentionally covers Linux and Windows only. It does not
run macOS, sanitizer, coverage, static-analysis, or formatting/lint jobs.
