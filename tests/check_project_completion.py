#!/usr/bin/env python3
"""Validate that the repository satisfies the documented project completion gate."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
MANAGEMENT_VALIDATOR = ROOT / "tests" / "validate_management_docs.py"

EXPECTED_ABSENT_PATHS = [
    ROOT / ".github" / "workflows" / "ci.yml",
    ROOT / "scripts" / "github_ci_status.py",
]

STARTER_FACING_FILES = [
    ROOT / "README.md",
    ROOT / "CMakeLists.txt",
    ROOT / "config" / "cli-starter.json",
    ROOT / "cmake" / "project_config.hpp.in",
    ROOT / "cmake" / "validate_management_docs.cmake",
    ROOT / "docs" / "project-overview.md",
    ROOT / "docs" / "architecture.md",
    ROOT / "docs" / "migration-from-legacy.md",
    ROOT / "tests" / "config_tests.cpp",
    ROOT / "tests" / "validate_management_docs.py",
]

STARTER_FACING_GLOBS = [
    "include/**/*.hpp",
    "src/**/*.cpp",
]

BANNED_TRACES = [
    "custom-cli",
    "k4nul",
    "cppcore",
    "D:/git",
    r"D:\\git",
]

REQUIRED_GLOBAL_CHECK_IDS = {
    "automation-stop-rule-documented",
}

REQUIRED_COMPLETION_CHECK_IDS = {
    "project-exit-criteria-aligned",
    "project-completion-state-script",
}


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def add_error(errors: list[str], message: str) -> None:
    errors.append(message)


def ensure(errors: list[str], condition: bool, message: str) -> None:
    if not condition:
        add_error(errors, message)


def run_management_doc_validation(errors: list[str]) -> None:
    result = subprocess.run(
        [sys.executable, str(MANAGEMENT_VALIDATOR)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        combined_output = (result.stdout + result.stderr).strip()
        add_error(
            errors,
            "management-doc validation failed before completion checks:\n"
            f"{combined_output}",
        )


def collect_starter_facing_paths() -> list[Path]:
    collected = set(STARTER_FACING_FILES)
    for pattern in STARTER_FACING_GLOBS:
        collected.update(ROOT.glob(pattern))
    return sorted(collected)


def validate_absent_paths(errors: list[str]) -> None:
    workflow_dir = ROOT / ".github" / "workflows"
    if workflow_dir.exists():
        workflow_files = sorted(workflow_dir.glob("*.yml")) + sorted(workflow_dir.glob("*.yaml"))
        ensure(
            errors,
            not workflow_files,
            ".github/workflows still contains workflow files even though D14 requires repo-local GitHub Actions to stay absent.",
        )

    for path in EXPECTED_ABSENT_PATHS:
        ensure(errors, not path.exists(), f"{path.relative_to(ROOT)} should remain absent under the completion gate.")


def validate_starter_facing_traces(errors: list[str]) -> None:
    for path in collect_starter_facing_paths():
        ensure(errors, path.exists(), f"starter-facing file is missing: {path.relative_to(ROOT)}")
        if not path.exists():
            continue
        contents = path.read_text(encoding="utf-8")
        for trace in BANNED_TRACES:
            if trace in contents:
                add_error(errors, f"{path.relative_to(ROOT)} still contains banned legacy trace {trace!r}.")


def validate_plan(plan: dict[str, Any], errors: list[str]) -> None:
    ensure(errors, plan.get("current_phase") == "handoff", "plan.yaml current_phase must be `handoff` once all work is complete.")
    ensure(errors, plan.get("blockers") == [], "plan.yaml blockers must be empty when the project is complete.")

    active_items = plan.get("active_items", [])
    ensure(errors, bool(active_items), "plan.yaml must keep at least one active_items entry documenting the completion state.")
    incomplete_items = [item.get("id", "<unknown>") for item in active_items if item.get("status") != "completed"]
    ensure(errors, not incomplete_items, f"plan.yaml still has non-completed active_items: {', '.join(incomplete_items)}")

    success_criteria = plan.get("goal", {}).get("success_criteria", [])
    ensure(errors, bool(success_criteria), "plan.yaml goal.success_criteria must be populated.")

    automation_actions = [
        item for item in plan.get("next_actions", []) if "scheduled or recurring automation pass" in item.get("action", "")
    ]
    ensure(errors, bool(automation_actions), "plan.yaml next_actions must include the recurring-automation completion check.")
    if automation_actions:
        action = automation_actions[0]
        ensure(
            errors,
            "tests/check_project_completion.py" in action.get("action", ""),
            "plan.yaml should direct automation to run tests/check_project_completion.py first.",
        )
        ensure(
            errors,
            "exit immediately" in action.get("exit_signal", ""),
            "plan.yaml recurring-automation exit signal must require an immediate no-op exit on satisfied completion criteria.",
        )


def validate_agent_policy(plan: dict[str, Any], policy: dict[str, Any], errors: list[str]) -> None:
    completion_policy = policy.get("completion_policy", {})
    ensure(
        errors,
        completion_policy.get("source_of_truth") == "docs/management/plan.yaml goal.success_criteria",
        "agent-policy.yaml completion_policy.source_of_truth must point to plan.yaml goal.success_criteria.",
    )
    ensure(
        errors,
        len(completion_policy.get("project_exit_criteria", [])) == len(plan.get("goal", {}).get("success_criteria", [])),
        "agent-policy.yaml completion_policy.project_exit_criteria must stay aligned in count with plan.yaml goal.success_criteria.",
    )
    ensure(
        errors,
        "tests/check_project_completion.py" in completion_policy.get("automation_stop_rule", ""),
        "agent-policy.yaml automation_stop_rule should point recurring automation at tests/check_project_completion.py.",
    )
    ensure(
        errors,
        "completion email" in completion_policy.get("completion_notification_rule", "")
        or "completion notice" in completion_policy.get("completion_notification_rule", ""),
        "agent-policy.yaml completion_notification_rule should describe the requested completion notification behavior.",
    )


def validate_handoff(handoff: dict[str, Any], errors: list[str]) -> None:
    ensure(errors, handoff.get("in_progress") == [], "handoff.yaml in_progress must be empty once all work is complete.")
    ensure(errors, handoff.get("verification_pending") == [], "handoff.yaml verification_pending must be empty once all work is complete.")
    ensure(
        errors,
        "satisfies the documented project completion gate" in handoff.get("current_state", {}).get("summary", ""),
        "handoff.yaml current_state.summary must state that the current tree satisfies the documented completion gate.",
    )

    automation_actions = [
        item for item in handoff.get("next_recommended", []) if "tests/check_project_completion.py" in item.get("action", "")
    ]
    ensure(errors, bool(automation_actions), "handoff.yaml next_recommended must include the recurring-automation no-op path.")
    if automation_actions:
        ensure(
            errors,
            "exit immediately" in automation_actions[0].get("action", ""),
            "handoff.yaml recurring-automation guidance must require an immediate exit on satisfied completion criteria.",
        )


def validate_verification(verification: dict[str, Any], errors: list[str]) -> None:
    global_ids = {item.get("id") for item in verification.get("global_checks", [])}
    completion_ids = {item.get("id") for item in verification.get("completion_checks", [])}

    ensure(
        errors,
        REQUIRED_GLOBAL_CHECK_IDS.issubset(global_ids),
        "verification.yaml is missing required global checks for the automation stop rule.",
    )
    ensure(
        errors,
        REQUIRED_COMPLETION_CHECK_IDS.issubset(completion_ids),
        "verification.yaml is missing required completion checks for the project completion state.",
    )


def validate_agents(errors: list[str]) -> None:
    contents = (ROOT / "AGENTS.md").read_text(encoding="utf-8")
    required_snippets = [
        "## Completion Gate",
        "python3 tests/check_project_completion.py",
        "exit immediately",
    ]
    for snippet in required_snippets:
        ensure(errors, snippet in contents, f"AGENTS.md is missing required completion-gate snippet: {snippet!r}")


def validate_cron_prompt(errors: list[str]) -> None:
    prompt_path = ROOT / ".codex" / "cron-prompt.txt"
    ensure(errors, prompt_path.exists(), ".codex/cron-prompt.txt must exist so recurring automation has a completion-gated entrypoint.")
    if not prompt_path.exists():
        return

    contents = prompt_path.read_text(encoding="utf-8")
    required_snippets = [
        "python3 tests/check_project_completion.py",
        "즉시 종료",
        "실패할 때만",
    ]
    for snippet in required_snippets:
        ensure(errors, snippet in contents, f".codex/cron-prompt.txt is missing required completion-gate snippet: {snippet!r}")


def main() -> int:
    errors: list[str] = []
    run_management_doc_validation(errors)

    plan = load_json(ROOT / "docs" / "management" / "plan.yaml")
    policy = load_json(ROOT / "docs" / "management" / "agent-policy.yaml")
    handoff = load_json(ROOT / "docs" / "management" / "handoff.yaml")
    verification = load_json(ROOT / "docs" / "management" / "verification.yaml")

    validate_absent_paths(errors)
    validate_starter_facing_traces(errors)
    validate_plan(plan, errors)
    validate_agent_policy(plan, policy, errors)
    validate_handoff(handoff, errors)
    validate_verification(verification, errors)
    validate_agents(errors)
    validate_cron_prompt(errors)

    if errors:
        print("Project completion validation failed:", file=sys.stderr)
        for error in errors:
            print(f" - {error}", file=sys.stderr)
        return 1

    print("Project completion state confirmed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
