#!/usr/bin/env python3
"""Validate docs/management/*.yaml against repository-local JSON Schemas.

The management instance files intentionally stay inside a JSON-compatible YAML
subset. JSON is valid YAML 1.2, which lets this repository validate the files
using only the Python standard library.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]

VALIDATION_MAP = [
    ("docs/management/phases.yaml", "docs/management/schema/phase.schema.json"),
    ("docs/management/plan.yaml", "docs/management/schema/plan.schema.json"),
    ("docs/management/agent-policy.yaml", "docs/management/schema/agent-policy.schema.json"),
    ("docs/management/verification.yaml", "docs/management/schema/verification.schema.json"),
    ("docs/management/decisions.yaml", "docs/management/schema/decision.schema.json"),
    ("docs/management/handoff.yaml", "docs/management/schema/handoff.schema.json"),
]


def load_json_document(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def is_type(instance: Any, expected_type: str) -> bool:
    if expected_type == "object":
        return isinstance(instance, dict)
    if expected_type == "array":
        return isinstance(instance, list)
    if expected_type == "string":
        return isinstance(instance, str)
    if expected_type == "boolean":
        return isinstance(instance, bool)
    if expected_type == "integer":
        return type(instance) is int
    if expected_type == "number":
        return type(instance) is int or type(instance) is float
    return True


def validate(instance: Any, schema: dict[str, Any], path: str = "$") -> list[str]:
    errors: list[str] = []

    expected_type = schema.get("type")
    if expected_type is not None and not is_type(instance, expected_type):
        return [f"{path}: expected type {expected_type}, got {type(instance).__name__}"]

    if "const" in schema and instance != schema["const"]:
        errors.append(f"{path}: expected constant value {schema['const']!r}, got {instance!r}")

    if "enum" in schema and instance not in schema["enum"]:
        errors.append(f"{path}: expected one of {schema['enum']!r}, got {instance!r}")

    if "minimum" in schema and isinstance(instance, (int, float)) and not isinstance(instance, bool):
        if instance < schema["minimum"]:
            errors.append(f"{path}: expected value >= {schema['minimum']}, got {instance}")

    if expected_type == "object":
        required = schema.get("required", [])
        for key in required:
            if key not in instance:
                errors.append(f"{path}: missing required key {key!r}")

        properties = schema.get("properties", {})
        additional_allowed = schema.get("additionalProperties", True)
        if additional_allowed is False:
            for key in instance:
                if key not in properties:
                    errors.append(f"{path}: unexpected key {key!r}")

        for key, subschema in properties.items():
            if key in instance:
                errors.extend(validate(instance[key], subschema, f"{path}.{key}"))

    if expected_type == "array":
        min_items = schema.get("minItems")
        if min_items is not None and len(instance) < min_items:
            errors.append(f"{path}: expected at least {min_items} items, got {len(instance)}")

        item_schema = schema.get("items")
        if item_schema is not None:
            for index, item in enumerate(instance):
                errors.extend(validate(item, item_schema, f"{path}[{index}]"))

    return errors


def main() -> int:
    failures: list[str] = []

    for instance_rel, schema_rel in VALIDATION_MAP:
        instance_path = ROOT / instance_rel
        schema_path = ROOT / schema_rel

        try:
            instance = load_json_document(instance_path)
        except json.JSONDecodeError as error:
            failures.append(f"{instance_rel}: parse error at line {error.lineno}, column {error.colno}: {error.msg}")
            continue

        schema = load_json_document(schema_path)
        errors = validate(instance, schema)
        if errors:
            failures.extend(f"{instance_rel}: {message}" for message in errors)
        else:
            print(f"OK {instance_rel}")

    if failures:
        print("Management-doc validation failed:", file=sys.stderr)
        for failure in failures:
            print(f" - {failure}", file=sys.stderr)
        return 1

    print("All management docs passed schema validation.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
