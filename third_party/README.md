# Third-Party Dependencies

This directory vendors small header-only dependencies so the starter does not depend on hidden sibling repositories.

## Included Dependencies

- `CLI11` `v2.6.2`
  - Source: <https://github.com/CLIUtils/CLI11>
  - License file: [licenses/CLI11-LICENSE.txt](/D:/git/CLI/third_party/licenses/CLI11-LICENSE.txt)
- `nlohmann/json` `v3.12.0`
  - Source: <https://github.com/nlohmann/json>
  - License file: [licenses/nlohmann-json-LICENSE.txt](/D:/git/CLI/third_party/licenses/nlohmann-json-LICENSE.txt)
- `doctest` `v2.5.2`
  - Source: <https://github.com/doctest/doctest>
  - License file: [licenses/doctest-LICENSE.txt](/D:/git/CLI/third_party/licenses/doctest-LICENSE.txt)

## Update Guidance

- Prefer official release tags over random commits.
- Record version changes in `docs/management/decisions.yaml` or `docs/management/handoff.yaml`.
- Re-run starter verification after dependency updates.
