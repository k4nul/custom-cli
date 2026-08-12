#!/usr/bin/env python3
"""Validate that the CLI starter completion evidence is machine-checkable.

This script is intentionally dependency-free so automation can run it before a
phase transition and after the project has entered maintenance. It checks the
repository metadata, test wiring, documentation anchors, and artifact hygiene
preflight that make the starter safe to keep in maintenance-only rotation.
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
COMPLETION_REPAIR_PHASE = "completion-gate-repair"
MAINTENANCE_PHASE = "starter-template-maintenance"
MAINTENANCE_COMPLETE_PHASE = "starter-template-maintenance-complete"
SHELL_COMPLETION_GENERATION_PHASE = "shell-completion-generation"
COMMAND_MANPAGE_GENERATION_PHASE = "command-manpage-generation"
COMMAND_METADATA_JSON_GENERATION_PHASE = "command-metadata-json-generation"
COMPLETION_CHECK_COMMAND = "python3 tests/check_project_completion.py"
BASELINE_VALIDATION_COMMAND = (
    "cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON "
    "&& cmake --build build && ctest --test-dir build --output-on-failure"
)
MAINTENANCE_NO_PENDING_MODE = "maintenance-no-pending-transition"
MAINTENANCE_COMPLETION_PENDING_MODE = "maintenance-completion-pending"
MAINTENANCE_COMPLETE_MODE = "maintenance-complete-no-pending-transition"
SHELL_COMPLETION_GENERATION_MODE = "strict-gate"
SHELL_COMPLETION_TRANSITION_COMMAND = (
    "bash scripts/test-generate-completions.sh && " + BASELINE_VALIDATION_COMMAND
)
COMMAND_MANPAGE_TRANSITION_COMMAND = (
    "bash scripts/test-generate-manpage.sh && " + BASELINE_VALIDATION_COMMAND
)
COMMAND_METADATA_JSON_TRANSITION_COMMAND = (
    "bash scripts/test-generate-command-metadata.sh && " + BASELINE_VALIDATION_COMMAND
)
NON_HYGIENE_CTEST_FILTER = "^(starter_tests|template_instantiation_workflow|shell_completion_generation|command_manpage_generation|cli_starter_smoke)$"
LEGACY_NON_HYGIENE_CTEST_FILTER = "^(starter_tests|cli_starter_smoke)$"


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
        ".editorconfig",
        ".gitattributes",
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
        "docs/template-instantiation.md",
        "docs/testing.md",
        "include/starter/app/application.hpp",
        "include/starter/app/cli_app.hpp",
        "scripts/instantiate_template.py",
        "scripts/generate-completions.py",
        "scripts/test-generate-completions.sh",
        "scripts/generate-manpage.py",
        "scripts/test-generate-manpage.sh",
        "include/starter/core/config.hpp",
        "include/starter/core/completion.hpp",
        "src/app/application.cpp",
        "src/app/cli_app.cpp",
        "src/commands/register_commands.cpp",
        "src/core/config.cpp",
        "src/core/completion.cpp",
        "tests/check_project_completion.py",
        "tests/config_tests.cpp",
        "tests/instantiate_template_tests.py",
        "tests/generate_completion_tests.py",
        "tests/generate_manpage_tests.py",
        "docs/man-pages.md",
        "third_party/README.md",
    )
    for path_text in required_paths:
        require_path(REPO_ROOT / path_text, blockers)


def check_phase_manifest(blockers: list[str]) -> None:
    manifest_path = REPO_ROOT / "docs/instructions/phase-gates.json"
    manifest = read_json(manifest_path)
    if manifest.get("project") != "custom-cli":
        blockers.append("phase manifest project must be custom-cli")

    transition = manifest.get("transition")
    if not isinstance(transition, dict):
        blockers.append("phase manifest transition must be an object")
        transition = {}

    current_phase = str(manifest.get("current_phase") or "")
    next_phase = str(manifest.get("next_phase") or "")
    transition_mode = str(transition.get("mode") or "")
    transition_validation_command = str(transition.get("transition_validation_command") or "")
    if current_phase == COMPLETION_REPAIR_PHASE:
        if next_phase != MAINTENANCE_PHASE:
            blockers.append(
                "phase manifest next_phase must be starter-template-maintenance "
                "before phase-transition"
            )
        if transition_validation_command != COMPLETION_CHECK_COMMAND:
            blockers.append(
                "phase manifest transition_validation_command must run this "
                "checker before phase-transition"
            )
    elif current_phase == SHELL_COMPLETION_GENERATION_PHASE:
        if next_phase != MAINTENANCE_COMPLETE_PHASE:
            blockers.append(
                "phase manifest next_phase must be starter-template-maintenance-complete "
                "after shell completion generation"
            )
        if transition_mode != SHELL_COMPLETION_GENERATION_MODE:
            blockers.append("phase manifest transition mode must be strict-gate for shell completion generation")
        if transition_validation_command != SHELL_COMPLETION_TRANSITION_COMMAND:
            blockers.append(
                "phase manifest transition_validation_command must run shell completion "
                "tests and the baseline CMake/CTest flow"
            )
    elif current_phase == COMMAND_MANPAGE_GENERATION_PHASE:
        if next_phase != MAINTENANCE_COMPLETE_PHASE:
            blockers.append(
                "phase manifest next_phase must be starter-template-maintenance-complete "
                "after command man page generation"
            )
        if transition_mode != SHELL_COMPLETION_GENERATION_MODE:
            blockers.append("phase manifest transition mode must be strict-gate for command man page generation")
        if transition_validation_command != COMMAND_MANPAGE_TRANSITION_COMMAND:
            blockers.append(
                "phase manifest transition_validation_command must run command man page "
                "tests and the baseline CMake/CTest flow"
            )
    elif current_phase == COMMAND_METADATA_JSON_GENERATION_PHASE:
        if next_phase != MAINTENANCE_COMPLETE_PHASE:
            blockers.append(
                "phase manifest next_phase must be starter-template-maintenance-complete "
                "after JSON command metadata generation"
            )
        if transition_mode != SHELL_COMPLETION_GENERATION_MODE:
            blockers.append("phase manifest transition mode must be strict-gate for JSON command metadata generation")
        if transition_validation_command != COMMAND_METADATA_JSON_TRANSITION_COMMAND:
            blockers.append(
                "phase manifest transition_validation_command must run JSON command metadata "
                "tests and the baseline CMake/CTest flow"
            )
    elif current_phase == MAINTENANCE_PHASE:
        if next_phase:
            if next_phase != MAINTENANCE_COMPLETE_PHASE:
                blockers.append(
                    "phase manifest next_phase must be "
                    "starter-template-maintenance-complete when closing "
                    "maintenance"
                )
            if transition_mode != MAINTENANCE_COMPLETION_PENDING_MODE:
                blockers.append(
                    "phase manifest transition mode must be "
                    "maintenance-completion-pending when closing maintenance"
                )
            if transition_validation_command != COMPLETION_CHECK_COMMAND:
                blockers.append(
                    "phase manifest transition_validation_command must be set "
                    "to the completion checker when closing maintenance"
                )
        else:
            if transition_mode != MAINTENANCE_NO_PENDING_MODE:
                blockers.append(
                    "phase manifest transition mode must be "
                    "maintenance-no-pending-transition"
                )
            if transition.get("validation_command") != BASELINE_VALIDATION_COMMAND:
                blockers.append(
                    "phase manifest validation_command must run the baseline "
                    "CMake/CTest flow"
                )
            if transition_validation_command:
                blockers.append(
                    "phase manifest transition_validation_command must be empty "
                    "during maintenance"
                )
    elif current_phase == MAINTENANCE_COMPLETE_PHASE:
        if next_phase:
            blockers.append(
                "phase manifest next_phase must be empty after maintenance is complete"
            )
        if transition_mode != MAINTENANCE_COMPLETE_MODE:
            blockers.append(
                "phase manifest transition mode must be "
                "maintenance-complete-no-pending-transition after maintenance is complete"
            )
        if transition.get("validation_command") != BASELINE_VALIDATION_COMMAND:
            blockers.append(
                "phase manifest validation_command must run the baseline "
                "CMake/CTest flow after maintenance is complete"
            )
        if transition_validation_command:
            blockers.append(
                "phase manifest transition_validation_command must be empty "
                "after maintenance is complete"
            )
    else:
        blockers.append(
            "phase manifest current_phase must be completion-gate-repair, "
            "shell-completion-generation, command-manpage-generation, command-metadata-json-generation, "
            "starter-template-maintenance, or "
            "starter-template-maintenance-complete"
        )

    phase_model = manifest.get("phase_model")
    if not isinstance(phase_model, list):
        blockers.append("phase manifest phase_model must be a list")
    else:
        phase_ids = {
            str(phase.get("id") or "")
            for phase in phase_model
            if isinstance(phase, dict)
        }
        for phase_id in (
            COMPLETION_REPAIR_PHASE,
            SHELL_COMPLETION_GENERATION_PHASE,
            COMMAND_MANPAGE_GENERATION_PHASE,
            COMMAND_METADATA_JSON_GENERATION_PHASE,
            MAINTENANCE_PHASE,
            MAINTENANCE_COMPLETE_PHASE,
        ):
            if phase_id not in phase_ids:
                blockers.append(f"phase manifest phase_model is missing phase: {phase_id}")

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
            blockers.append(
                f"required gate {gate_id or '<missing-id>'} has non-passing "
                f"status {status or '<missing>'}"
            )

    missing_gate_ids = expected_gate_ids - seen_gate_ids
    for gate_id in sorted(missing_gate_ids):
        blockers.append(f"phase manifest is missing required gate: {gate_id}")


def check_cmake_and_ci_wiring(blockers: list[str]) -> None:
    require_contains(
        REPO_ROOT / ".editorconfig",
        (
            "root = true",
            "end_of_line = lf",
            "insert_final_newline = true",
            "trim_trailing_whitespace = true",
        ),
        blockers,
    )
    require_contains(
        REPO_ROOT / ".gitattributes",
        (
            "* text=auto eol=lf",
            "*.bat text eol=crlf",
            "*.cmd text eol=crlf",
        ),
        blockers,
    )
    require_contains(
        REPO_ROOT / "CMakeLists.txt",
        (
            "add_library(starter_core",
            "add_executable(${CLI_STARTER_BINARY_NAME} src/main.cpp)",
            "find_package(Python3 COMPONENTS Interpreter REQUIRED)",
            "add_executable(starter_tests tests/config_tests.cpp)",
            "add_test(NAME starter_tests COMMAND starter_tests)",
            "NAME template_instantiation_workflow",
            "NAME shell_completion_generation",
            "NAME command_manpage_generation",
            "PYTHONDONTWRITEBYTECODE=1",
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
        (".editorconfig", ".gitattributes", "ls-files", '"build-local-*"', '".sandbox-user/*"'),
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
        blockers.append(
            "tests/config_tests.cpp has too few doctest cases for current "
            f"coverage: {test_case_count}"
        )

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
    expected_smoke_cases = {
        "version": ("run_cli_exact_smoke_case", '"version"'),
        "about display": ('"about display"',),
        "about binary": ('"about binary"',),
        "about config": ('"about config"',),
        "doctor": ('run_cli_smoke_case("doctor"',),
        "config init": ('run_cli_smoke_case("config init"',),
        "config show": ('run_cli_smoke_case("config show"',),
        "hello": ('run_cli_smoke_case("hello"',),
        "echo numbered": ('run_cli_smoke_case("echo numbered"',),
    }
    for command, markers in expected_smoke_cases.items():
        if not all(marker in smoke for marker in markers):
            blockers.append(f"cmake/cli_smoke_test.cmake is missing smoke case: {command}")
    for dynamic_marker in (
        "CLI_STARTER_DISPLAY_NAME",
        "CLI_STARTER_BINARY_NAME",
        "CLI_STARTER_CONFIG_FILE",
        "CLI_STARTER_PROMPT_LABEL",
    ):
        if dynamic_marker not in smoke:
            blockers.append(
                "cmake/cli_smoke_test.cmake is missing dynamic metadata "
                f"marker: {dynamic_marker}"
            )
    for failure in (
        "unknown command",
        "missing echo text",
        "unknown hello option",
        "missing config subcommand",
    ):
        if f'run_cli_failure_smoke_case("{failure}"' not in smoke:
            blockers.append(f"cmake/cli_smoke_test.cmake is missing failure smoke case: {failure}")
    for shell_case in (
        "redirected default shell",
        "redirected explicit shell",
        "redirected empty-prompt shell",
    ):
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

    documented_filter_paths = (
        "README.md",
        "docs/ci.md",
        "docs/maintenance.md",
        "docs/onboarding.md",
        "docs/testing.md",
        "docs/troubleshooting.md",
    )
    for path_text in documented_filter_paths:
        content = read_text(REPO_ROOT / path_text)
        if NON_HYGIENE_CTEST_FILTER not in content:
            blockers.append(f"{path_text} does not document the current non-hygiene CTest filter")

    markdown_paths = [REPO_ROOT / "README.md", *sorted((REPO_ROOT / "docs").glob("*.md"))]
    for path in markdown_paths:
        content = read_text(path)
        if LEGACY_NON_HYGIENE_CTEST_FILTER in content:
            blockers.append(f"{relative(path)} still documents the legacy non-hygiene CTest filter")


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


def check_template_instantiation_workflow(blockers: list[str]) -> None:
    require_contains(
        REPO_ROOT / "scripts/instantiate_template.py",
        (
            "CLI_STARTER_BINARY_NAME",
            "CLI_STARTER_DISPLAY_NAME",
            "CLI_STARTER_CONFIG_FILE",
            "CLI_STARTER_PROMPT_LABEL",
            "write_config_template",
            "run_repository_preflight",
            "run_validation",
            "preflight_commands",
            "--run-validation",
        ),
        blockers,
    )


def check_shell_completion_generation(blockers: list[str]) -> None:
    require_contains(
        REPO_ROOT / "scripts/generate-completions.py",
        (
            "extract_public_commands",
            "src/app/cli_app.cpp",
            "src/commands/register_commands.cpp",
            "render_bash",
            "render_zsh",
            "render_powershell",
            "--output-dir",
            "--force",
        ),
        blockers,
    )
    require_contains(
        REPO_ROOT / "tests/generate_completion_tests.py",
        (
            "test_extracts_every_public_command_from_the_cpp_registry",
            "test_every_renderer_covers_every_public_command_deterministically",
            "test_output_directory_writes_all_shell_scripts_for_the_selected_binary",
            "test_refuses_a_symlinked_generated_output",
        ),
        blockers,
    )
    require_contains(
        REPO_ROOT / "scripts/test-generate-completions.sh",
        ("generate_completion_tests.py", "PYTHONDONTWRITEBYTECODE=1"),
        blockers,
    )
    require_contains(
        REPO_ROOT / "docs/shell-completions.md",
        (
            "Bash, Zsh, or\nPowerShell",
            "--shell bash",
            "--output-dir",
            "bash scripts/test-generate-completions.sh",
        ),
        blockers,
    )
    require_contains(
        REPO_ROOT / "tests/instantiate_template_tests.py",
        (
            "test_plan_derives_safe_defaults_from_binary_name",
            "test_plan_preserves_explicit_display_config_and_prompt_values",
            "test_plan_rejects_display_names_that_cannot_be_written_to_project_header",
            "test_write_config_template_creates_config_file_and_refuses_unforced_overwrite",
            "test_write_config_template_refuses_missing_repo_root",
            "test_write_config_template_refuses_symlink_repo_root",
            "test_write_config_template_refuses_non_directory_config_path",
            "test_write_config_template_refuses_symlink_config_directory",
            "test_run_validation_executes_generated_commands_from_repo_root",
            "test_run_validation_stops_before_cmake_when_policy_file_is_missing",
            "test_run_validation_rejects_tracked_local_artifacts_before_cmake",
            "test_main_json_run_validation_reports_result",
            "test_json_output_includes_commands_and_written_config_path",
        ),
        blockers,
    )
    require_contains(
        REPO_ROOT / "docs/template-instantiation.md",
        (
            "python3 scripts/instantiate_template.py",
            "python3 tests/instantiate_template_tests.py",
            "--write-config",
            "--run-validation",
            "repository policy-file and artifact preflight",
        ),
        blockers,
    )


def check_command_manpage_generation(blockers: list[str]) -> None:
    require_contains(
        REPO_ROOT / "scripts/generate-manpage.py",
        (
            "extract_public_commands",
            "extract_global_options",
            "src/app/cli_app.cpp",
            "src/commands/register_commands.cpp",
            "render_manpage",
            "--output",
            "--force",
        ),
        blockers,
    )
    require_contains(
        REPO_ROOT / "tests/generate_manpage_tests.py",
        (
            "test_extracts_every_public_command_from_the_cpp_registry",
            "test_extracts_every_global_option_from_the_root_cli_setup",
            "test_rendering_is_deterministic_and_covers_commands_and_options",
            "test_refuses_unsafe_command_names_and_symlinked_outputs",
        ),
        blockers,
    )
    require_contains(
        REPO_ROOT / "scripts/test-generate-manpage.sh",
        ("generate_manpage_tests.py", "PYTHONDONTWRITEBYTECODE=1"),
        blockers,
    )
    require_contains(
        REPO_ROOT / "docs/man-pages.md",
        (
            "deterministic roff man page",
            "--command-name my-cli",
            "--output ./man/my-cli.1",
            "bash scripts/test-generate-manpage.sh",
        ),
        blockers,
    )


def check_artifact_hygiene(blockers: list[str]) -> None:
    result = run_git_ls_files(("build-local-*", ".sandbox-user/*"))
    if result.returncode != 0:
        blockers.append(
            f"git artifact preflight failed: {result.stderr.strip() or 'unknown error'}"
        )
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
        check_template_instantiation_workflow,
        check_shell_completion_generation,
        check_command_manpage_generation,
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
        print("completion check blocked:")
        for blocker in blockers:
            print(f"- {blocker}")
        return 1

    print("completion check passed:")
    print("- phase manifest gates are machine-checkable")
    print("- CMake, CTest, template instantiation, generated docs, smoke, and CI wiring are present")
    print("- command documentation covers the registered starter surface")
    print("- repository artifact hygiene preflight is clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
