# Artifact Hygiene

This runbook covers the repository hygiene gate for historical local build and
sandbox artifacts. The starter ignores these paths for new local work, but older
checkouts can still contain tracked generated files that must be removed before
full CTest validation can be reported as passing.

## What The Gate Checks

The `repository_hygiene` CTest entry is registered from `CMakeLists.txt` and
implemented by `cmake/repository_hygiene_test.cmake`. When it runs inside a Git
worktree with `git` available, it inspects tracked paths that match:

- `build-local-*`
- `.sandbox-user/*`

The check fails when matching tracked paths still exist in the checkout. If
`git` is not available, or the test is run outside a Git worktree, the hygiene
script reports a skip instead of proving that the checkout is clean.
CTest may still summarize that skip path as a successful test process because
the script returns early. Treat it as a skipped gate, not a passed hygiene proof,
unless the Git preflight succeeds and the tracked artifact query is clean.

## Inspect Current State

Start every validation or cleanup report with the artifact gate:

```bash
git status --short
git ls-files 'build-local-*' '.sandbox-user/*'
```

The `git status --short` output should be empty before a cleanup package starts.
The `git ls-files` command should print no paths after cleanup. If it prints
paths, do not use historical binaries, CTest files, Visual Studio telemetry, or
sandbox files from those locations as evidence for the current source tree.

For planning only, this breakdown helps identify which top-level generated
families are still tracked:

```bash
git ls-files 'build-local-*' '.sandbox-user/*' | cut -d/ -f1 | sort | uniq -c
```

## Cleanup Package

Keep the cleanup mechanical and isolated:

1. Confirm there are no unrelated edits:

   ```bash
   git status --short
   ```

2. List the tracked generated paths:

   ```bash
   git ls-files 'build-local-*' '.sandbox-user/*'
   ```

3. Remove only those generated artifact families:

   ```bash
   git rm -r --ignore-unmatch -- build-local-* .sandbox-user
   ```

4. Confirm the gate is clean:

   ```bash
   git ls-files 'build-local-*' '.sandbox-user/*'
   ```

5. Rebuild and run unfiltered validation from a fresh ignored `build/` tree:

   ```bash
   cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

Do not combine artifact removal with command, config, dependency, or test
behavior changes. Directly related documentation updates are acceptable when
they keep the cleanup instructions accurate.

## Reporting Results

- Report validation as blocked when `git ls-files 'build-local-*' '.sandbox-user/*'`
  prints paths that still exist in the checkout.
- Report `repository_hygiene` as skipped, not passed, when `git` is unavailable
  or validation runs outside a Git worktree.
- Report full validation as passing only after the artifact gate prints no paths
  and the unfiltered CMake/CTest flow passes from a fresh ignored build tree.

When the skip state is ambiguous in normal CTest output, run the hygiene entry
verbosely or include the Git preflight in the report:

```bash
command -v git
git rev-parse --is-inside-work-tree
ctest --test-dir build --output-on-failure -R '^repository_hygiene$' -V
```
