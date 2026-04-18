# Migration From Legacy

## Confirmed Legacy Shape

Before refactoring, the repository contained:

- a legacy executable host
- a shared command framework
- a product-specific command plugin

The old build also linked to sibling repositories that were not present inside this repository.

## Legacy Trace Removal

Removed or replaced categories:

- legacy organization-specific directory names, target names, and class names
- product names in help text, banners, RC metadata, and error messages
- organization contact information
- hidden backdoor-like commands
- hard-coded organization-specific deployment paths
- product codes and update logic tied to a specific environment

## Shared Utility Layer Breakdown

Observed responsibilities in the removed shared utility layer:

- command/runtime utilities: error codes, exceptions, logging
- string utilities and text conversions
- file/path helpers
- JSON serialization and formatter objects
- hashing helpers
- process/thread/dynamic-library wrappers
- network inspection helpers

## Replacement Strategy

- Replaced CLI parsing with `CLI11`
- Replaced JSON formatter usage with `nlohmann/json`
- Replaced test gap with `doctest`
- Replaced retained utility needs with local, focused helpers in `src/core/`
- Removed product-specific features instead of recreating them when they had no place in a generic starter

## Role-To-Replacement Map

- CLI/subcommand parsing -> `CLI11`
- Formatter-based JSON config -> `nlohmann/json`
- Starter tests -> `doctest`
- Token splitting and shell input reuse -> local `tokenize_command_line`
- Project metadata and default config naming -> generated config header plus local metadata helper
- File writes for starter config generation -> standard library filesystem and streams

## Legacy To New Mapping

- legacy executable host -> `Application` in `src/app/`
- shared command framework -> `src/core/` plus command registrars
- product operations plugin -> removed, replaced by sample starter commands

## Important Design Shift

The new repository is intentionally not a drop-in clone of the old runtime behavior. It is a reusable starter that keeps reusable CLI patterns and discards environment-specific operations.
