#!/usr/bin/env python3
"""Tests for the starter instantiation workflow."""

from __future__ import annotations

import argparse
import contextlib
import io
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


sys.dont_write_bytecode = True

REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_PATH = REPO_ROOT / "scripts" / "instantiate_template.py"
EXPECTED_ENABLED_COMMANDS = ["about", "hello", "echo", "config", "doctor"]
SPEC = importlib.util.spec_from_file_location("instantiate_template", SCRIPT_PATH)
assert SPEC is not None
instantiate_template = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = instantiate_template
SPEC.loader.exec_module(instantiate_template)


def make_args(**overrides: object) -> argparse.Namespace:
    values = {
        "binary_name": "my-cli",
        "display_name": None,
        "config_file": None,
        "prompt_label": None,
        "build_dir": "build-renamed",
    }
    values.update(overrides)
    return argparse.Namespace(**values)


def run_main(argv: list[str]) -> tuple[int, str, str]:
    stdout = io.StringIO()
    stderr = io.StringIO()
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        exit_code = instantiate_template.main(argv)
    return exit_code, stdout.getvalue(), stderr.getvalue()


def run_script(argv: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT_PATH), *argv],
        cwd=cwd,
        check=False,
        capture_output=True,
        text=True,
    )


class InstantiateTemplateTests(unittest.TestCase):
    def test_plan_derives_safe_defaults_from_binary_name(self) -> None:
        plan = instantiate_template.build_plan(make_args())

        self.assertEqual(plan.binary_name, "my-cli")
        self.assertEqual(plan.display_name, "My Cli")
        self.assertEqual(plan.config_file, "my-cli.json")
        self.assertEqual(plan.prompt_label, "mycli")
        self.assertIn("-DCLI_STARTER_BINARY_NAME=my-cli", plan.cmake_command)
        self.assertIn("-DCLI_STARTER_PROMPT_LABEL=mycli", plan.cmake_command)
        self.assertIn(" && cmake --build build-renamed && ", plan.validation_command)
        self.assertEqual(plan.config_path.as_posix(), "config/my-cli.json")

    def test_plan_derives_readable_defaults_from_mixed_separator_binary_names(self) -> None:
        plan = instantiate_template.build_plan(make_args(binary_name="ops.team_cli"))

        self.assertEqual(plan.display_name, "Ops Team Cli")
        self.assertEqual(plan.config_file, "ops.team_cli.json")
        self.assertEqual(plan.prompt_label, "opsteamcli")
        self.assertEqual(plan.config_path.as_posix(), "config/ops.team_cli.json")

    def test_plan_preserves_explicit_display_config_and_prompt_values(self) -> None:
        plan = instantiate_template.build_plan(
            make_args(
                binary_name="opsctl",
                display_name="Ops Control",
                config_file="ops.json",
                prompt_label="ops",
            )
        )

        self.assertIn("-DCLI_STARTER_DISPLAY_NAME=Ops Control", plan.cmake_command)
        self.assertIn("-DCLI_STARTER_CONFIG_FILE=ops.json", plan.cmake_command)
        self.assertEqual(plan.config_template["prompt"], "ops")
        self.assertEqual(plan.config_template["enabled_commands"], EXPECTED_ENABLED_COMMANDS)

    def test_plan_accepts_safe_punctuation_in_explicit_config_file(self) -> None:
        plan = instantiate_template.build_plan(
            make_args(
                binary_name="opsctl",
                config_file="ops.team_cli-v2.json",
            )
        )

        self.assertEqual(plan.config_file, "ops.team_cli-v2.json")
        self.assertEqual(plan.config_path.as_posix(), "config/ops.team_cli-v2.json")
        self.assertIn("-DCLI_STARTER_CONFIG_FILE=ops.team_cli-v2.json", plan.cmake_command)

    def test_validation_command_shell_quotes_values_with_spaces(self) -> None:
        plan = instantiate_template.build_plan(
            make_args(binary_name="opsctl", display_name="Ops Control")
        )

        self.assertIn("'-DCLI_STARTER_DISPLAY_NAME=Ops Control'", plan.validation_command)
        self.assertIn("-DCLI_STARTER_BINARY_NAME=opsctl", plan.validation_command)
        self.assertIn(" && cmake --build build-renamed && ", plan.validation_command)
        self.assertNotIn("'&&'", plan.validation_command)

    def test_plan_rejects_paths_control_characters_and_non_json_config_files(self) -> None:
        invalid_args = [
            make_args(binary_name="../my-cli"),
            make_args(binary_name="bad\nname"),
            make_args(config_file="nested/my-cli.json"),
            make_args(config_file="my-cli.conf"),
            make_args(prompt_label="-bad"),
        ]

        for args in invalid_args:
            with self.subTest(args=args):
                with self.assertRaises(instantiate_template.InstantiationError):
                    instantiate_template.build_plan(args)

    def test_plan_rejects_token_values_that_do_not_start_with_alnum(self) -> None:
        invalid_args = [
            make_args(binary_name="-opsctl"),
            make_args(binary_name="_opsctl"),
            make_args(binary_name=".opsctl"),
            make_args(config_file="-opsctl.json"),
            make_args(config_file="_opsctl.json"),
            make_args(config_file=".opsctl.json"),
            make_args(prompt_label="_ops"),
            make_args(build_dir="-build"),
        ]

        for args in invalid_args:
            with self.subTest(args=args):
                with self.assertRaisesRegex(
                    instantiate_template.InstantiationError,
                    "must start with a letter or digit",
                ):
                    instantiate_template.build_plan(args)

    def test_plan_rejects_blank_display_names_and_unsafe_build_directories(self) -> None:
        invalid_args = [
            make_args(display_name="  "),
            make_args(display_name="Ops\nControl"),
            make_args(build_dir=""),
            make_args(build_dir="../build"),
            make_args(build_dir="build/local"),
        ]

        for args in invalid_args:
            with self.subTest(args=args):
                with self.assertRaises(instantiate_template.InstantiationError):
                    instantiate_template.build_plan(args)

    def test_plan_rejects_display_names_that_cannot_be_written_to_project_header(self) -> None:
        invalid_args = [
            make_args(display_name='Ops "Control"'),
            make_args(display_name=r"Ops\Control"),
        ]

        for args in invalid_args:
            with self.subTest(args=args):
                with self.assertRaisesRegex(
                    instantiate_template.InstantiationError,
                    "display name must not contain double quotes or backslashes",
                ):
                    instantiate_template.build_plan(args)

    def test_write_config_template_creates_config_file_and_refuses_unforced_overwrite(self) -> None:
        plan = instantiate_template.build_plan(make_args())

        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            output_path = instantiate_template.write_config_template(plan, repo_root, force=False)
            written = json.loads(output_path.read_text(encoding="utf-8"))

            self.assertEqual(output_path, repo_root / "config" / "my-cli.json")
            self.assertEqual(written["prompt"], "mycli")
            self.assertEqual(written["default_name"], "world")
            self.assertEqual(written["enabled_commands"], EXPECTED_ENABLED_COMMANDS)

            with self.assertRaises(instantiate_template.InstantiationError):
                instantiate_template.write_config_template(plan, repo_root, force=False)

            output_path.write_text("stale\n", encoding="utf-8")
            instantiate_template.write_config_template(plan, repo_root, force=True)
            self.assertTrue(output_path.read_text(encoding="utf-8").startswith("{\n"))

    def test_write_config_template_refuses_non_regular_output_path(self) -> None:
        plan = instantiate_template.build_plan(make_args())

        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            output_path = repo_root / "config" / "my-cli.json"
            output_path.mkdir(parents=True)

            with self.assertRaisesRegex(
                instantiate_template.InstantiationError,
                "not a regular file",
            ):
                instantiate_template.write_config_template(plan, repo_root, force=True)

            self.assertTrue(output_path.is_dir())

    def test_write_config_template_refuses_non_directory_config_path(self) -> None:
        plan = instantiate_template.build_plan(make_args())

        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            config_path = repo_root / "config"
            config_path.write_text("not a directory\n", encoding="utf-8")

            with self.assertRaisesRegex(
                instantiate_template.InstantiationError,
                "is not a directory",
            ):
                instantiate_template.write_config_template(plan, repo_root, force=True)

            self.assertEqual(config_path.read_text(encoding="utf-8"), "not a directory\n")

    def test_write_config_template_refuses_missing_repo_root(self) -> None:
        plan = instantiate_template.build_plan(make_args())

        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir) / "missing"

            with self.assertRaisesRegex(
                instantiate_template.InstantiationError,
                "repo root .* does not exist",
            ):
                instantiate_template.write_config_template(plan, repo_root, force=True)

            self.assertFalse(repo_root.exists())

    def test_write_config_template_refuses_non_directory_repo_root(self) -> None:
        plan = instantiate_template.build_plan(make_args())

        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir) / "repo"
            repo_root.write_text("not a directory\n", encoding="utf-8")

            with self.assertRaisesRegex(
                instantiate_template.InstantiationError,
                "repo root .* is not a directory",
            ):
                instantiate_template.write_config_template(plan, repo_root, force=True)

            self.assertEqual(repo_root.read_text(encoding="utf-8"), "not a directory\n")

    def test_write_config_template_refuses_symlink_repo_root(self) -> None:
        plan = instantiate_template.build_plan(make_args())

        with tempfile.TemporaryDirectory() as temp_dir:
            real_root = Path(temp_dir) / "repo"
            real_root.mkdir()
            repo_root = Path(temp_dir) / "repo-link"
            try:
                repo_root.symlink_to(real_root, target_is_directory=True)
            except (NotImplementedError, OSError) as exc:
                self.skipTest(f"symlink creation unavailable: {exc}")

            with self.assertRaisesRegex(
                instantiate_template.InstantiationError,
                "repo root .* must not be a symlink",
            ):
                instantiate_template.write_config_template(plan, repo_root, force=True)

            self.assertFalse((real_root / "config" / "my-cli.json").exists())

    def test_write_config_template_refuses_symlink_output_path_even_with_force(self) -> None:
        plan = instantiate_template.build_plan(make_args())

        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            target_path = repo_root / "outside.json"
            target_path.write_text("outside\n", encoding="utf-8")
            output_path = repo_root / "config" / "my-cli.json"
            output_path.parent.mkdir()
            try:
                output_path.symlink_to(target_path)
            except (NotImplementedError, OSError) as exc:
                self.skipTest(f"symlink creation unavailable: {exc}")

            with self.assertRaisesRegex(
                instantiate_template.InstantiationError,
                "must not be a symlink",
            ):
                instantiate_template.write_config_template(plan, repo_root, force=True)

            self.assertEqual(target_path.read_text(encoding="utf-8"), "outside\n")

    def test_write_config_template_refuses_symlink_config_directory(self) -> None:
        plan = instantiate_template.build_plan(make_args())

        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir) / "repo"
            repo_root.mkdir()
            outside_config = Path(temp_dir) / "outside-config"
            outside_config.mkdir()
            config_path = repo_root / "config"
            try:
                config_path.symlink_to(outside_config, target_is_directory=True)
            except (NotImplementedError, OSError) as exc:
                self.skipTest(f"symlink creation unavailable: {exc}")

            with self.assertRaisesRegex(
                instantiate_template.InstantiationError,
                "must not be a symlink",
            ):
                instantiate_template.write_config_template(plan, repo_root, force=True)

            self.assertFalse((outside_config / "my-cli.json").exists())

    def test_run_validation_executes_generated_commands_from_repo_root(self) -> None:
        plan = instantiate_template.build_plan(make_args())
        recorded: list[tuple[list[str], Path]] = []

        def fake_runner(
            command: list[str],
            **kwargs: object,
        ) -> subprocess.CompletedProcess[str]:
            recorded.append((command, kwargs["cwd"]))  # type: ignore[arg-type]
            return subprocess.CompletedProcess(command, 0, "", "")

        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            result = instantiate_template.run_validation(
                plan,
                repo_root,
                runner=fake_runner,
            )

        self.assertEqual(result.return_code, 0)
        self.assertIsNone(result.failed_command)
        self.assertEqual(
            [command for command, _ in recorded],
            plan.validation_commands,
        )
        self.assertTrue(all(cwd == repo_root for _, cwd in recorded))

    def test_run_validation_stops_at_first_failed_command(self) -> None:
        plan = instantiate_template.build_plan(make_args())
        recorded: list[list[str]] = []

        def fake_runner(command: list[str], **_: object) -> subprocess.CompletedProcess[str]:
            recorded.append(command)
            return_code = 7 if command == plan.build_command else 0
            return subprocess.CompletedProcess(command, return_code, "", "")

        with tempfile.TemporaryDirectory() as temp_dir:
            result = instantiate_template.run_validation(
                plan,
                Path(temp_dir),
                runner=fake_runner,
            )

        self.assertEqual(result.return_code, 7)
        self.assertEqual(result.failed_command, tuple(plan.build_command))
        self.assertEqual(recorded, [plan.cmake_command, plan.build_command])

    def test_json_output_includes_commands_and_written_config_path(self) -> None:
        plan = instantiate_template.build_plan(make_args(display_name="My CLI"))
        payload = json.loads(instantiate_template.plan_as_json(plan, "config/my-cli.json"))

        self.assertEqual(payload["display_name"], "My CLI")
        self.assertEqual(payload["config_path"], "config/my-cli.json")
        self.assertEqual(payload["wrote_config"], "config/my-cli.json")
        self.assertEqual(
            payload["cmake_command"],
            [
                "cmake",
                "-S",
                ".",
                "-B",
                "build-renamed",
                "-DCLI_STARTER_BINARY_NAME=my-cli",
                "-DCLI_STARTER_DISPLAY_NAME=My CLI",
                "-DCLI_STARTER_CONFIG_FILE=my-cli.json",
                "-DCLI_STARTER_PROMPT_LABEL=mycli",
                "-DBUILD_TESTING=ON",
                "-DCLI_STARTER_BUILD_TESTS=ON",
            ],
        )
        self.assertEqual(payload["build_command"], ["cmake", "--build", "build-renamed"])
        self.assertEqual(
            payload["ctest_command"],
            ["ctest", "--test-dir", "build-renamed", "--output-on-failure"],
        )
        self.assertIn("ctest", payload["validation_command"])
        self.assertNotIn("'&&'", payload["validation_command"])
        self.assertFalse(payload["ran_validation"])
        self.assertIsNone(payload["validation_return_code"])
        self.assertIsNone(payload["failed_validation_command"])

    def test_json_output_includes_validation_result(self) -> None:
        plan = instantiate_template.build_plan(make_args())
        result = instantiate_template.ValidationResult(7, tuple(plan.build_command))
        payload = json.loads(instantiate_template.plan_as_json(plan, None, result))

        self.assertTrue(payload["ran_validation"])
        self.assertEqual(payload["validation_return_code"], 7)
        self.assertEqual(payload["failed_validation_command"], plan.build_command)

    def test_main_prints_text_plan_without_writing_config(self) -> None:
        exit_code, stdout, stderr = run_main(
            [
                "--binary-name",
                "opsctl",
                "--display-name",
                "Ops Control",
                "--build-dir",
                "build-renamed",
            ]
        )

        self.assertEqual(exit_code, 0)
        self.assertEqual(stderr, "")
        self.assertIn("Template instantiation plan", stdout)
        self.assertIn("- binary name: opsctl", stdout)
        self.assertIn("- display name: Ops Control", stdout)
        self.assertIn("cmake -S . -B build-renamed", stdout)
        self.assertIn("Config template not written; pass --write-config to create it.", stdout)

    def test_main_text_write_config_reports_written_path_and_content(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            exit_code, stdout, stderr = run_main(
                [
                    "--binary-name",
                    "opsctl",
                    "--repo-root",
                    str(repo_root),
                    "--write-config",
                ]
            )

            config_path = repo_root / "config" / "opsctl.json"
            written = json.loads(config_path.read_text(encoding="utf-8"))

            self.assertEqual(exit_code, 0)
            self.assertEqual(stderr, "")
            self.assertIn("Template instantiation plan", stdout)
            self.assertIn(f"Wrote config template: {config_path}", stdout)
            self.assertEqual(written["prompt"], "opsctl")
            self.assertEqual(written["default_name"], "world")
            self.assertEqual(written["enabled_commands"], EXPECTED_ENABLED_COMMANDS)

    def test_main_json_write_config_uses_repo_root(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            exit_code, stdout, stderr = run_main(
                [
                    "--binary-name",
                    "opsctl",
                    "--repo-root",
                    str(repo_root),
                    "--write-config",
                    "--json",
                ]
            )

            config_path = repo_root / "config" / "opsctl.json"
            payload = json.loads(stdout)
            written = json.loads(config_path.read_text(encoding="utf-8"))

            self.assertEqual(exit_code, 0)
            self.assertEqual(stderr, "")
            self.assertEqual(payload["binary_name"], "opsctl")
            self.assertEqual(payload["config_path"], "config/opsctl.json")
            self.assertEqual(payload["wrote_config"], str(config_path))
            self.assertEqual(written["prompt"], "opsctl")
            self.assertEqual(written["enabled_commands"], EXPECTED_ENABLED_COMMANDS)

    def test_main_json_plan_without_write_config_reports_no_written_file(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            exit_code, stdout, stderr = run_main(
                [
                    "--binary-name",
                    "opsctl",
                    "--repo-root",
                    str(repo_root),
                    "--json",
                ]
            )

            payload = json.loads(stdout)

            self.assertEqual(exit_code, 0)
            self.assertEqual(stderr, "")
            self.assertEqual(payload["config_path"], "config/opsctl.json")
            self.assertIsNone(payload["wrote_config"])
            self.assertFalse((repo_root / "config" / "opsctl.json").exists())

    def test_main_json_run_validation_reports_result(self) -> None:
        original_run_validation = instantiate_template.run_validation

        def fake_run_validation(
            plan: instantiate_template.InstantiationPlan,
            repo_root: Path,
        ) -> instantiate_template.ValidationResult:
            self.assertEqual(repo_root, Path("."))
            self.assertEqual(plan.binary_name, "opsctl")
            return instantiate_template.ValidationResult(0, None)

        try:
            instantiate_template.run_validation = fake_run_validation
            exit_code, stdout, stderr = run_main(
                [
                    "--binary-name",
                    "opsctl",
                    "--run-validation",
                    "--json",
                ]
            )
        finally:
            instantiate_template.run_validation = original_run_validation

        payload = json.loads(stdout)
        self.assertEqual(exit_code, 0)
        self.assertEqual(stderr, "")
        self.assertTrue(payload["ran_validation"])
        self.assertEqual(payload["validation_return_code"], 0)
        self.assertIsNone(payload["failed_validation_command"])

    def test_main_run_validation_failure_returns_validation_status(self) -> None:
        original_run_validation = instantiate_template.run_validation

        def fake_run_validation(
            plan: instantiate_template.InstantiationPlan,
            repo_root: Path,
        ) -> instantiate_template.ValidationResult:
            return instantiate_template.ValidationResult(9, tuple(plan.ctest_command))

        try:
            instantiate_template.run_validation = fake_run_validation
            exit_code, stdout, stderr = run_main(
                [
                    "--binary-name",
                    "opsctl",
                    "--run-validation",
                    "--json",
                ]
            )
        finally:
            instantiate_template.run_validation = original_run_validation

        payload = json.loads(stdout)
        self.assertEqual(exit_code, 9)
        self.assertEqual(stderr, "")
        self.assertEqual(payload["validation_return_code"], 9)
        self.assertEqual(
            payload["failed_validation_command"],
            ["ctest", "--test-dir", "build", "--output-on-failure"],
        )

    def test_main_refuses_unforced_config_overwrite(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            config_path = repo_root / "config" / "opsctl.json"
            config_path.parent.mkdir()
            config_path.write_text("existing\n", encoding="utf-8")

            exit_code, stdout, stderr = run_main(
                [
                    "--binary-name",
                    "opsctl",
                    "--repo-root",
                    str(repo_root),
                    "--write-config",
                ]
            )

            self.assertEqual(exit_code, 2)
            self.assertEqual(stdout, "")
            self.assertIn("error:", stderr)
            self.assertIn("already exists; pass --force to replace it", stderr)
            self.assertEqual(config_path.read_text(encoding="utf-8"), "existing\n")

    def test_main_write_config_refuses_missing_repo_root_without_creating_it(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir) / "missing"
            exit_code, stdout, stderr = run_main(
                [
                    "--binary-name",
                    "opsctl",
                    "--repo-root",
                    str(repo_root),
                    "--write-config",
                ]
            )

            self.assertEqual(exit_code, 2)
            self.assertEqual(stdout, "")
            self.assertIn("error:", stderr)
            self.assertIn("repo root", stderr)
            self.assertFalse(repo_root.exists())

    def test_main_write_config_refuses_config_parent_file_without_traceback(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            config_path = repo_root / "config"
            config_path.write_text("not a directory\n", encoding="utf-8")

            exit_code, stdout, stderr = run_main(
                [
                    "--binary-name",
                    "opsctl",
                    "--repo-root",
                    str(repo_root),
                    "--write-config",
                ]
            )

            self.assertEqual(exit_code, 2)
            self.assertEqual(stdout, "")
            self.assertIn("error:", stderr)
            self.assertIn("is not a directory", stderr)
            self.assertNotIn("Traceback", stderr)
            self.assertEqual(config_path.read_text(encoding="utf-8"), "not a directory\n")
            self.assertFalse((repo_root / "config" / "opsctl.json").exists())

    def test_main_force_json_replaces_existing_config(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            config_path = repo_root / "config" / "opsctl.json"
            config_path.parent.mkdir()
            config_path.write_text("stale\n", encoding="utf-8")

            exit_code, stdout, stderr = run_main(
                [
                    "--binary-name",
                    "opsctl",
                    "--repo-root",
                    str(repo_root),
                    "--write-config",
                    "--force",
                    "--json",
                ]
            )

            payload = json.loads(stdout)
            written = json.loads(config_path.read_text(encoding="utf-8"))

            self.assertEqual(exit_code, 0)
            self.assertEqual(stderr, "")
            self.assertEqual(payload["wrote_config"], str(config_path))
            self.assertEqual(written["prompt"], "opsctl")
            self.assertEqual(written["default_name"], "world")
            self.assertNotEqual(config_path.read_text(encoding="utf-8"), "stale\n")

    def test_main_invalid_plan_does_not_create_config_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            exit_code, stdout, stderr = run_main(
                [
                    "--binary-name",
                    "opsctl",
                    "--prompt-label",
                    "../ops",
                    "--repo-root",
                    str(repo_root),
                    "--write-config",
                ]
            )

            self.assertEqual(exit_code, 2)
            self.assertEqual(stdout, "")
            self.assertIn("error:", stderr)
            self.assertIn("prompt label must be a file name, not a path", stderr)
            self.assertFalse((repo_root / "config").exists())

    def test_script_entrypoint_help_exits_successfully_without_side_effects(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            result = run_script(["--help"], cwd=repo_root)

            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stderr, "")
            self.assertIn("Generate the CMake and config steps", result.stdout)
            self.assertIn("--write-config", result.stdout)
            self.assertIn("--run-validation", result.stdout)
            self.assertIn("--json", result.stdout)
            self.assertFalse((repo_root / "config").exists())

    def test_script_entrypoint_reports_argparse_errors_without_traceback(self) -> None:
        result = run_script(["--write-config"])

        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertIn("the following arguments are required: --binary-name", result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_script_entrypoint_write_config_defaults_to_current_working_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            result = run_script(
                [
                    "--binary-name",
                    "opsctl",
                    "--write-config",
                    "--json",
                ],
                cwd=repo_root,
            )

            config_path = repo_root / "config" / "opsctl.json"
            payload = json.loads(result.stdout)
            written = json.loads(config_path.read_text(encoding="utf-8"))

            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stderr, "")
            self.assertEqual(payload["config_path"], "config/opsctl.json")
            self.assertEqual(payload["wrote_config"], "config/opsctl.json")
            self.assertEqual(written["prompt"], "opsctl")
            self.assertEqual(written["enabled_commands"], EXPECTED_ENABLED_COMMANDS)


if __name__ == "__main__":
    unittest.main()
