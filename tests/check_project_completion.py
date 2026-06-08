#!/usr/bin/env python3
"""Validate that the CLI starter completion gate is machine-checkable.

This script is intentionally dependency-free so the phase controller can run it
before selecting the phase-transition task. It checks the repository metadata,
test wiring, documentation anchors, and artifact hygiene preflight that make the
starter safe to treat as complete enough for maintenance-only rotation.
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
PASSING_GATE_STATUSES = {"passed", "machine-check", "waived"}
EXPECTED_COMMANDS = ("about", "hello", "echo", "config", "doctor", "shell")
EXPECTED_GLOBAL_OPTIONS = ("--version", "--help", "--help-all", "--config")


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as exc:
        raise AssertionError(f"{path.relative_to(REPO_ROOT)} is not readable: {exc}") from exc


def read_json(path: Path) -> dict:
    try:
        return json.loads(read_text(path))
    except json.JSONDecodeError as exc:
        raise AssertionError(f"{path.relative_to(REPO_ROOT)} is not valid JSON: {exc}") from exc


def relative(path: Path) -> str:
    return path.relative_to(REPO_ROOT).as_posix()


def require_path(path: Path, blockers: list[str]) -> None:
    if not path.exists():
        blockers.append(f"missing required path: {relative(path)}")


def require_contains(path: Path, snippets: tuple[str, ...], blockers: list[str]) -> None:
    if not path.exists():
        blockers.append(f"missing required path: {relative(path)}")
        return
    content = read_text(path)
    for snippet in snippets:
        if snippet not in content:
            blockers.append(f"{relative(path)} is missing expected text: {snippet!r}")


def run_git_ls_files(patterns: tuple[str, ...]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "ls-files", *patterns],
        cwd=REPO_ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def check_required_paths(blockers: list[str]) -> None:
    required_paths = (
        "CMakeLists.txt",
        "README.md",
        ".github/workflows/ci.yml",
        "cmake/cli_smoke_test.cmake",
        "cmake/repository_hygiene_test.cmake",
        "config/cli-starter.json",
        "docs/architecture.md",
        "docs/command-reference.md",
        "docs/instructions/phase-gates.json",
        "docs/maintenance.md",
        "docs/testing.md",
        "include/starter/app/application.hpp",
        "include/starter/app/cli_app.hpp",
        "include/starter/core/config.hpp",
        "include/starter/core/completion.hpp",
        "src/app/application.cpp",
        "src/app/cli_app.cpp",
        "src/commands/register_commands.cpp",
        "src/core/config.cpp",
        "src/core/completion.cpp",
        "tests/check_project_completion.py",
        "tests/config_tests.cpp",
        "third_party/README.md",
    )
    for path_text in required_paths:
        require_path(REPO_ROOT / path_text, blockers)


def check_phase_manifest(blockers: list[str]) -> None:
    manifest_path = REPO_ROOT / "docs/instructions/phase-gates.json"
    manifest = read_json(manifest_path)
    if manifest.get("project") != "custom-cli":
        blockers.append("phase manifest project must be custom-cli")
    if manifest.get("current_phase") != "completion-gate-repair":
        blockers.append("phase manifest current_phase must remain completion-gate-repair until phase-transition")
    if manifest.get("next_phase") != "starter-template-maintenance":
        blockers.append("phase manifest next_phase must be starter-template-maintenance")

    transition = manifest.get("transition")
    if not isinstance(transition, dict):
        blockers.append("phase manifest transition must be an object")
    elif transition.get("transition_validation_command") != "python3 tests/check_project_completion.py":
        blockers.append("phase manifest transition_validation_command must run this checker")

    gates = manifest.get("required_gates")
    if not isinstance(gates, list):
        blockers.append("phase manifest required_gates must be a list")
        return

    expected_gate_ids = {
        "completion-checker-exists",
        "cmake-build-passes",
        "starter-docs-match-behavior",
        "no-unrequested-publish",
    }
    seen_gate_ids: set[str] = set()
    for gate in gates:
        if not isinstance(gate, dict):
            blockers.append("phase manifest contains a non-object required gate")
            continue
        gate_id = str(gate.get("id") or "")
        seen_gate_ids.add(gate_id)
        if gate.get("required_for_transition") is False:
            continue
        status = str(gate.get("status") or "").strip().lower()
        if status not in PASSING_GATE_STATUSES:
            blockers.append(f"required gate {gate_id or '<missing-id>'} has non-passing status {status or '<missing>'}")

    missing_gate_ids = expected_gate_ids - seen_gate_ids
    for gate_id in sorted(missing_gate_ids):
        blockers.append(f"phase manifest is missing required gate: {gate_id}")


def check_cmake_and_ci_wiring(blockers: list[str]) -> None:
    require_contains(
        REPO_ROOT / "CMakeLists.txt",
        (
            "add_library(starter_core",
            "add_executable(${CLI_STARTER_BINARY_NAME} src/main.cpp)",
            "add_executable(starter_tests tests/config_tests.cpp)",
            "add_test(NAME starter_tests COMMAND starter_tests)",
            "NAME cli_starter_smoke",
            "NAME repository_hygiene",
            "CLI_STARTER_BUILD_TESTS",
        ),
        blockers,
    )
    require_contains(
        REPO_ROOT / ".github/workflows/ci.yml",
        (
            "runs-on: ubuntu-latest",
            "runs-on: windows-latest",
            "cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON",
            "ctest --test-dir build --output-on-failure",
        ),
        blockers,
    )
    require_contains(
        REPO_ROOT / "cmake/repository_hygiene_test.cmake",
        ("ls-files", '"build-local-*"', '".sandbox-user/*"'),
        blockers,
    )


def check_command_registration(blockers: list[str]) -> None:
    cli_app = read_text(REPO_ROOT / "src/app/cli_app.cpp")
    registrations = read_text(REPO_ROOT / "src/commands/register_commands.cpp")

    for option in ("--help-all", "--version", "-c,--config"):
        if option not in cli_app:
            blockers.append(f"src/app/cli_app.cpp does not register {option}")

    if 'app.add_subcommand("shell"' not in cli_app:
        blockers.append("src/app/cli_app.cpp does not register the shell subcommand")

    for command in ("about", "hello", "echo", "config", "doctor"):
        registrar = f"register_{command}_command"
        if registrar not in registrations:
            blockers.append(f"src/commands/register_commands.cpp does not call {registrar}")


def check_tests_and_smoke_coverage(blockers: list[str]) -> None:
    tests = read_text(REPO_ROOT / "tests/config_tests.cpp")
    test_case_count = len(re.findall(r"\bTEST_CASE\(", tests))
    if test_case_count < 80:
        blockers.append(f"tests/config_tests.cpp has too few doctest cases for current coverage: {test_case_count}")

    for expected in (
        "application accepts hello subcommand options from argv order",
        "application echoes numbered positional text",
        "interactive shell runs no-argv sessions through the normal dispatch path",
        "interactive shell scopes completion after global config options",
        "config show reports malformed disk config through stderr",
        "doctor reports healthy starter layout with missing config warning",
        "tab completion reflects starter commands subcommands and options",
    ):
        if expected not in tests:
            blockers.append(f"tests/config_tests.cpp is missing expected coverage: {expected}")

    smoke = read_text(REPO_ROOT / "cmake/cli_smoke_test.cmake")
    for command in ("version", "about", "doctor", "config init", "config show", "hello", "echo numbered"):
        if f'run_cli_smoke_case("{command}"' not in smoke:
            blockers.append(f"cmake/cli_smoke_test.cmake is missing smoke case: {command}")
    for failure in ("unknown command", "missing echo text", "unknown hello option", "missing config subcommand"):
        if f'run_cli_failure_smoke_case("{failure}"' not in smoke:
            blockers.append(f"cmake/cli_smoke_test.cmake is missing failure smoke case: {failure}")
    for shell_case in ("redirected default shell", "redirected explicit shell"):
        if f'run_cli_stdin_smoke_case("{shell_case}"' not in smoke:
            blockers.append(f"cmake/cli_smoke_test.cmake is missing shell smoke case: {shell_case}")


def check_documentation_alignment(blockers: list[str]) -> None:
    command_reference = read_text(REPO_ROOT / "docs/command-reference.md")
    readme = read_text(REPO_ROOT / "README.md")
    testing = read_text(REPO_ROOT / "docs/testing.md")
    maintenance = read_text(REPO_ROOT / "docs/maintenance.md")

    for command in EXPECTED_COMMANDS:
        if f"`{command}`" not in command_reference:
            blockers.append(f"docs/command-reference.md does not document `{command}`")
        if command not in readme:
            blockers.append(f"README.md does not mention command text: {command}")

    for option in EXPECTED_GLOBAL_OPTIONS:
        if f"`{option}`" not in command_reference:
            blockers.append(f"docs/command-reference.md does not document `{option}`")

    for path in ("docs/testing.md", "docs/maintenance.md"):
        content = testing if path == "docs/testing.md" else maintenance
        if "python3 tests/check_project_completion.py" not in content:
            blockers.append(f"{path} does not document the completion checker command")

    for validation_text in (
        "git ls-files 'build-local-*' '.sandbox-user/*'",
        "cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON",
        "ctest --test-dir build --output-on-failure",
    ):
        if validation_text not in testing:
            blockers.append(f"docs/testing.md is missing validation command: {validation_text}")
        if validation_text not in maintenance:
            blockers.append(f"docs/maintenance.md is missing validation command: {validation_text}")


def check_config_template(blockers: list[str]) -> None:
    config = read_json(REPO_ROOT / "config/cli-starter.json")
    expected_fields = {
        "prompt": str,
        "default_name": str,
        "enabled_commands": list,
        "notes": str,
    }
    for field, expected_type in expected_fields.items():
        if field not in config:
            blockers.append(f"config/cli-starter.json is missing field: {field}")
        elif not isinstance(config[field], expected_type):
            blockers.append(f"config/cli-starter.json field {field} has the wrong type")
    enabled_commands = config.get("enabled_commands", [])
    for command in ("about", "hello", "echo", "config", "doctor"):
        if command not in enabled_commands:
            blockers.append(f"config/cli-starter.json enabled_commands omits {command}")


def check_artifact_hygiene(blockers: list[str]) -> None:
    result = run_git_ls_files(("build-local-*", ".sandbox-user/*"))
    if result.returncode != 0:
        blockers.append(f"git artifact preflight failed: {result.stderr.strip() or 'unknown error'}")
        return
    tracked_artifacts = [line for line in result.stdout.splitlines() if line.strip()]
    if tracked_artifacts:
        sample = ", ".join(tracked_artifacts[:5])
        suffix = "" if len(tracked_artifacts) <= 5 else f", and {len(tracked_artifacts) - 5} more"
        blockers.append(f"tracked generated artifact paths are still present: {sample}{suffix}")


def collect_blockers() -> list[str]:
    blockers: list[str] = []
    checks = (
        check_required_paths,
        check_phase_manifest,
        check_cmake_and_ci_wiring,
        check_command_registration,
        check_tests_and_smoke_coverage,
        check_documentation_alignment,
        check_config_template,
        check_artifact_hygiene,
    )
    for check in checks:
        try:
            check(blockers)
        except AssertionError as exc:
            blockers.append(str(exc))
    return blockers


def main() -> int:
    blockers = collect_blockers()
    if blockers:
        print("phase transition blocked:")
        for blocker in blockers:
            print(f"- {blocker}")
        return 1

    print("completion check passed:")
    print("- phase manifest gates are machine-checkable")
    print("- CMake, CTest, smoke, and CI wiring are present")
    print("- command documentation covers the registered starter surface")
    print("- repository artifact hygiene preflight is clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
