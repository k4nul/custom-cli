# CLI Starter

`CLI Starter` is a generic C++ command-line starter/template built to preserve a small reusable project shape without carrying over organization-specific product logic.

What it gives you:
- A small executable-focused layout built around `src/`, `include/`, `tests/`, and `docs/`
- One-shot command execution and an interactive shell mode
- A JSON configuration template
- Sample commands that show how to add options, subcommands, and validation
- Lightweight, vendored header-only dependencies for CLI parsing, JSON, and tests

## Quick start

1. Configure the project:

```bash
cmake -S . -B build
```

2. Build it:

```bash
cmake --build build
```

3. Generate a starter config:

```bash
build/cli-starter[.exe] config init
```

4. Run a sample command:

```bash
build/cli-starter[.exe] hello --name "template user"
```

5. Start the interactive shell:

```bash
build/cli-starter[.exe]
```

## Repository layout

- `src/`: application wiring, commands, and core helpers
- `include/`: public headers for the starter
- `config/`: checked-in configuration template
- `tests/`: starter-level tests plus management-doc validation
- `docs/`: explanatory prose docs for architecture and migration
- `docs/management/`: schema-backed operational state, policies, decisions, and handoff data
- `third_party/`: vendored header-only dependencies plus license files

## Customizing the starter

Start with these top-level CMake cache variables:

- `CLI_STARTER_BINARY_NAME`: executable file name
- `CLI_STARTER_DISPLAY_NAME`: human-friendly project name
- `CLI_STARTER_CONFIG_FILE`: default JSON config filename
- `CLI_STARTER_PROMPT_LABEL`: interactive prompt prefix

Example:

```bash
cmake -S . -B build -DCLI_STARTER_BINARY_NAME=my-cli -DCLI_STARTER_DISPLAY_NAME="My CLI"
```

## Adding a new command

1. Add a new `src/commands/<name>_command.cpp`.
2. Declare the registrar in [include/starter/commands/registrars.hpp](include/starter/commands/registrars.hpp).
3. Register it from [src/commands/register_commands.cpp](src/commands/register_commands.cpp).
4. Document user-facing behavior in [docs/architecture.md](docs/architecture.md).

The sample commands intentionally show different patterns:
- `hello`: consumes config defaults and command flags
- `echo`: demonstrates positional arguments and normalization
- `config`: demonstrates nested subcommands
- `doctor`: demonstrates starter self-checks

## Management docs

Operational rules and current repo state live under `docs/management/` as schema-backed YAML instances:

- `phases.yaml`: phase definitions and allowed statuses
- `plan.yaml`: current goal, active work, next actions, blockers, and risks
- `agent-policy.yaml`: naming, fact handling, change rules, and sub-agent delegation policy
- `verification.yaml`: global, phase-specific, and completion verification checks
- `decisions.yaml`: accepted design and process decisions
- `handoff.yaml`: current state for the next operator

The repository keeps these files in a JSON-compatible YAML subset so they can be validated with stock Python.

Validation command:

```bash
python tests/validate_management_docs.py
```

Starter test command sequence:

```bash
cmake -S . -B build -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

GitHub Actions mirrors those verification steps on `ubuntu-latest` and `windows-latest`, and also runs the management-doc validation script on every push and pull request.

## Dependency choices

The starter uses smaller, explicit pieces instead of a monolithic utility layer:

- `CLI11` for command-line parsing
- `nlohmann/json` for JSON configuration
- `doctest` for tests
- Standard C++17 library plus small local helpers for tokenization, file I/O, and error handling

Dependency source notes live in [third_party/README.md](third_party/README.md), and the migration rationale lives in [docs/migration-from-legacy.md](docs/migration-from-legacy.md).

## Documentation map

- Project direction: [docs/project-overview.md](docs/project-overview.md)
- Code structure: [docs/architecture.md](docs/architecture.md)
- Legacy migration details: [docs/migration-from-legacy.md](docs/migration-from-legacy.md)
- Management files: [docs/management/phases.yaml](docs/management/phases.yaml), [docs/management/plan.yaml](docs/management/plan.yaml), [docs/management/agent-policy.yaml](docs/management/agent-policy.yaml), [docs/management/verification.yaml](docs/management/verification.yaml), [docs/management/decisions.yaml](docs/management/decisions.yaml), [docs/management/handoff.yaml](docs/management/handoff.yaml)
- Legacy prose indexes: [docs/decisions.md](docs/decisions.md), [docs/handoff.md](docs/handoff.md), [docs/roadmap.md](docs/roadmap.md)
