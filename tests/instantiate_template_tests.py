#!/usr/bin/env python3
"""Tests for the starter instantiation workflow."""

from __future__ import annotations

import argparse
import contextlib
import io
import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


sys.dont_write_bytecode = True

REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_PATH = REPO_ROOT / "scripts" / "instantiate_template.py"
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
        self.assertEqual(plan.config_template["enabled_commands"], ["about", "hello", "echo", "config", "doctor"])

    def test_validation_command_shell_quotes_values_with_spaces(self) -> None:
        plan = instantiate_template.build_plan(make_args(binary_name="opsctl", display_name="Ops Control"))

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

    def test_write_config_template_creates_config_file_and_refuses_unforced_overwrite(self) -> None:
        plan = instantiate_template.build_plan(make_args())

        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            output_path = instantiate_template.write_config_template(plan, repo_root, force=False)
            written = json.loads(output_path.read_text(encoding="utf-8"))

            self.assertEqual(output_path, repo_root / "config" / "my-cli.json")
            self.assertEqual(written["prompt"], "mycli")
            self.assertEqual(written["default_name"], "world")
            self.assertEqual(written["enabled_commands"], ["about", "hello", "echo", "config", "doctor"])

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

            with self.assertRaisesRegex(instantiate_template.InstantiationError, "not a regular file"):
                instantiate_template.write_config_template(plan, repo_root, force=True)

            self.assertTrue(output_path.is_dir())

    def test_write_config_template_refuses_non_directory_config_path(self) -> None:
        plan = instantiate_template.build_plan(make_args())

        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            config_path = repo_root / "config"
            config_path.write_text("not a directory\n", encoding="utf-8")

            with self.assertRaisesRegex(instantiate_template.InstantiationError, "is not a directory"):
                instantiate_template.write_config_template(plan, repo_root, force=True)

            self.assertEqual(config_path.read_text(encoding="utf-8"), "not a directory\n")

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

            with self.assertRaisesRegex(instantiate_template.InstantiationError, "must not be a symlink"):
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

            with self.assertRaisesRegex(instantiate_template.InstantiationError, "must not be a symlink"):
                instantiate_template.write_config_template(plan, repo_root, force=True)

            self.assertFalse((outside_config / "my-cli.json").exists())

    def test_json_output_includes_commands_and_written_config_path(self) -> None:
        plan = instantiate_template.build_plan(make_args(display_name="My CLI"))
        payload = json.loads(instantiate_template.plan_as_json(plan, "config/my-cli.json"))

        self.assertEqual(payload["display_name"], "My CLI")
        self.assertEqual(payload["config_path"], "config/my-cli.json")
        self.assertEqual(payload["wrote_config"], "config/my-cli.json")
        self.assertIn("ctest", payload["validation_command"])
        self.assertNotIn("'&&'", payload["validation_command"])

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
            self.assertEqual(written["enabled_commands"], ["about", "hello", "echo", "config", "doctor"])

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


if __name__ == "__main__":
    unittest.main()
