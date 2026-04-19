# Repository Map

This repository is a generic C++ CLI starter. It is meant to be copied, renamed, and extended for new command-line tools without carrying over organization-specific product logic.

## Look Here First

- [README.md](README.md): quick start and starter usage
- [CMakeLists.txt](CMakeLists.txt): build entry point and starter naming knobs
- [src/](src): application flow, commands, and core helpers
- [include/](include): public headers and extension points
- [config/](config): checked-in configuration template
- [tests/](tests): starter verification examples

## Detailed Docs

- Purpose and scope: [docs/project-overview.md](docs/project-overview.md)
- Structure and extension points: [docs/architecture.md](docs/architecture.md)
- Legacy removal and replacement notes: [docs/migration-from-legacy.md](docs/migration-from-legacy.md)
- Design decision index: [docs/decisions.md](docs/decisions.md)
- Handoff index: [docs/handoff.md](docs/handoff.md)
- Roadmap index: [docs/roadmap.md](docs/roadmap.md)

## Management Files

- Phase model: [docs/management/phases.yaml](docs/management/phases.yaml)
- Current plan and next actions: [docs/management/plan.yaml](docs/management/plan.yaml)
- Editing and sub-agent policy: [docs/management/agent-policy.yaml](docs/management/agent-policy.yaml)
- Verification rules: [docs/management/verification.yaml](docs/management/verification.yaml)
- Decision log: [docs/management/decisions.yaml](docs/management/decisions.yaml)
- Handoff state: [docs/management/handoff.yaml](docs/management/handoff.yaml)
- JSON Schemas: [docs/management/schema/](docs/management/schema)

## Editing Policy

- Operational policy source of truth: [docs/management/agent-policy.yaml](docs/management/agent-policy.yaml)
- Schema definitions for management files: [docs/management/schema/](docs/management/schema)
- Current direction: keep removing remaining legacy organization-specific traces, treat `D:/git/CLI` as the maintained starter scope and standalone delivery repository inside the broader `D:/git` workspace, and run CLI starter git workflows from `D:/git/CLI` under the accepted direction recorded in [docs/management/decisions.yaml](docs/management/decisions.yaml)

## Verification

- Verification source of truth: [docs/management/verification.yaml](docs/management/verification.yaml)
- Validation entry point: [tests/validate_management_docs.py](tests/validate_management_docs.py)

## Additional AGENTS.md Files

Add a lower-level `AGENTS.md` only when a subdirectory needs local rules that would otherwise clutter this root map. Keep root guidance short and use `/docs` for detailed context.
