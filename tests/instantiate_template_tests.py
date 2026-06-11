#!/usr/bin/env python3
"""Tests for the starter instantiation workflow."""

from __future__ import annotations

import argparse
import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


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

    def test_json_output_includes_commands_and_written_config_path(self) -> None:
        plan = instantiate_template.build_plan(make_args(display_name="My CLI"))
        payload = json.loads(instantiate_template.plan_as_json(plan, "config/my-cli.json"))

        self.assertEqual(payload["display_name"], "My CLI")
        self.assertEqual(payload["config_path"], "config/my-cli.json")
        self.assertEqual(payload["wrote_config"], "config/my-cli.json")
        self.assertIn("ctest", payload["validation_command"])
        self.assertNotIn("'&&'", payload["validation_command"])


if __name__ == "__main__":
    unittest.main()
