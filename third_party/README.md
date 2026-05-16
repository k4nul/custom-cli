# Third-Party Dependencies

This directory vendors small header-only dependencies so the starter does not depend on hidden sibling repositories.

## Included Dependencies

- `CLI11` `v2.6.2`
  - Source: <https://github.com/CLIUtils/CLI11>
  - License file: [licenses/CLI11-LICENSE.txt](licenses/CLI11-LICENSE.txt)
- `nlohmann/json` `v3.12.0`
  - Source: <https://github.com/nlohmann/json>
  - License file: [licenses/nlohmann-json-LICENSE.txt](licenses/nlohmann-json-LICENSE.txt)
- `doctest` `v2.5.2`
  - Source: <https://github.com/doctest/doctest>
  - License file: [licenses/doctest-LICENSE.txt](licenses/doctest-LICENSE.txt)

## Update Guidance

- Prefer official release tags over random commits.
- Record version changes in this file and update the matching license file under `licenses/`.
- Update starter docs when a dependency change affects build, test, or user-facing CLI behavior.
- Re-run starter verification after dependency updates:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R starter_tests --output-on-failure
```
