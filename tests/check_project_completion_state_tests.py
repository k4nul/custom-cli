#!/usr/bin/env python3
"""Regression tests for management and phase-state consistency."""

from check_project_completion import (
    MAINTENANCE_COMPLETE_PHASE,
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

contradictions = check(MAINTENANCE_COMPLETE_PHASE, "planned", "active")
assert any("unresolved management goals" in blocker for blocker in contradictions)
assert any("AUTOMATION lifecycle" in blocker for blocker in contradictions)
assert any("PROJECT managementStatus" in blocker for blocker in contradictions)

assert any(
    "active phase cannot use paused-complete" in blocker
    for blocker in check("command-markdown-reference-generation", "active", "paused-complete")
)

print("check_project_completion_state_tests passed")
