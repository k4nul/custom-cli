#!/usr/bin/env python3
"""Regression tests for management and phase-state consistency."""

import json
import tempfile
from pathlib import Path

from check_project_completion import (
    BASELINE_VALIDATION_COMMAND,
    MAINTENANCE_COMPLETE_PHASE,
    check_management_files,
    check_management_phase_alignment,
)


def check(current_phase: str, goal_status: str, lifecycle_status: str) -> list[str]:
    blockers: list[str] = []
    check_management_phase_alignment(
        current_phase,
        {"nextGoals": [{"id": "next-feature", "status": goal_status}]},
        {"lifecycle": {"status": lifecycle_status}},
        {"project": {"managementStatus": lifecycle_status}},
        blockers,
    )
    return blockers


assert check(MAINTENANCE_COMPLETE_PHASE, "complete", "paused-complete") == []
assert "cmake --build build --parallel 4" in BASELINE_VALIDATION_COMMAND

contradictions = check(MAINTENANCE_COMPLETE_PHASE, "planned", "active")
assert any("unresolved management goals" in blocker for blocker in contradictions)
assert any("AUTOMATION lifecycle" in blocker for blocker in contradictions)
assert any("PROJECT managementStatus" in blocker for blocker in contradictions)

assert any(
    "active phase cannot use paused-complete" in blocker
    for blocker in check("command-markdown-reference-generation", "active", "paused-complete")
)

with tempfile.TemporaryDirectory() as tmp:
    repo_root = Path(tmp)
    absent_blockers: list[str] = []
    check_management_files(repo_root, MAINTENANCE_COMPLETE_PHASE, absent_blockers)
    assert absent_blockers == []

    management = repo_root / "docs" / "management"
    management.mkdir(parents=True)
    (management / "PLAN.json").write_text(
        json.dumps({"nextGoals": []}), encoding="utf-8"
    )
    partial_blockers: list[str] = []
    check_management_files(repo_root, MAINTENANCE_COMPLETE_PHASE, partial_blockers)
    assert partial_blockers == [
        "management state is incomplete; missing files: "
        "docs/management/AUTOMATION.json, docs/management/PROJECT.json"
    ]

print("check_project_completion_state_tests passed")
