# Contributing to CLI Starter

This project is a reusable C++ CLI starter. Keep changes generic enough that a
new project can copy the repository without inheriting one team's workflow.

## Local Setup

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

On Windows, use the same CMake commands from a Visual Studio or Build Tools
developer shell and pass `-C Debug` to `ctest` when using a multi-config
generator.

## Pull Request Checklist

- Keep generated build outputs out of commits.
- Update README or `docs/` when command behavior changes.
- Add or update tests for CLI behavior, config parsing, and smoke coverage.
- Keep vendored dependency notices in `third_party/licenses/` current.

## Dependency Policy

This repository vendors small header-only dependencies for portability. When a
vendored dependency is updated, include the upstream version/source in
`third_party/README.md` and keep the upstream license text in
`third_party/licenses/`.
