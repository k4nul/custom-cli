#!/usr/bin/env python3
"""Focused tests for deterministic JSON command metadata generation."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path


sys.dont_write_bytecode = True
REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_PATH = REPO_ROOT / "scripts" / "generate-command-metadata.py"
SPEC = importlib.util.spec_from_file_location("generate_command_metadata", SCRIPT_PATH)
assert SPEC is not None
generate_command_metadata = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = generate_command_metadata
SPEC.loader.exec_module(generate_command_metadata)


EXPECTED_COMMANDS = ("about", "config", "doctor", "echo", "hello", "shell")
EXPECTED_GLOBAL_OPTIONS = ("--help", "--help-all", "--version", "-c, --config <path>")


class GenerateCommandMetadataTests(unittest.TestCase):
    def test_extracts_every_public_command_and_global_option_from_cpp_sources(self) -> None:
        commands = generate_command_metadata.extract_public_commands(REPO_ROOT)
        options = generate_command_metadata.extract_global_options(REPO_ROOT)

        self.assertEqual(tuple(command.name for command in commands), EXPECTED_COMMANDS)
        self.assertEqual(tuple(option.name for option in options), EXPECTED_GLOBAL_OPTIONS)
        self.assertTrue(all(command.description for command in commands))
        self.assertTrue(all(option.description for option in options))

    def test_rendering_is_deterministic_schema_versioned_json(self) -> None:
        first = generate_command_metadata.render_metadata(
            "cli-starter",
            generate_command_metadata.extract_public_commands(REPO_ROOT),
            generate_command_metadata.extract_global_options(REPO_ROOT),
        )

        self.assertEqual(
            first,
            generate_command_metadata.render_metadata(
                "cli-starter",
                generate_command_metadata.extract_public_commands(REPO_ROOT),
                generate_command_metadata.extract_global_options(REPO_ROOT),
            ),
        )
        self.assertTrue(first.endswith("\n"))
        metadata = json.loads(first)
        self.assertEqual(metadata["schema_version"], "1.0.0")
        self.assertEqual(metadata["command_name"], "cli-starter")
        self.assertEqual(
            tuple(command["name"] for command in metadata["commands"]), EXPECTED_COMMANDS
        )
        self.assertEqual(
            tuple(option["name"] for option in metadata["global_options"]),
            EXPECTED_GLOBAL_OPTIONS,
        )

    def test_writes_only_to_an_explicit_regular_output_path(self) -> None:
        content = generate_command_metadata.render_metadata(
            "my-cli",
            generate_command_metadata.extract_public_commands(REPO_ROOT),
            generate_command_metadata.extract_global_options(REPO_ROOT),
        )
        with tempfile.TemporaryDirectory() as temp_dir:
            output_path = Path(temp_dir) / "my-cli.commands.json"
            self.assertEqual(
                generate_command_metadata.write_metadata(output_path, content), output_path
            )
            self.assertEqual(
                json.loads(output_path.read_text(encoding="utf-8"))["command_name"], "my-cli"
            )

            with self.assertRaises(generate_command_metadata.CommandMetadataError):
                generate_command_metadata.write_metadata(output_path, content)
            generate_command_metadata.write_metadata(output_path, content, force=True)

    def test_command_line_writes_metadata_to_the_requested_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            output_path = Path(temp_dir) / "my-cli.commands.json"
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(
                    generate_command_metadata.main(
                        [
                            "--command-name",
                            "my-cli",
                            "--output",
                            str(output_path),
                        ]
                    ),
                    0,
                )
            self.assertEqual(
                json.loads(output_path.read_text(encoding="utf-8"))["command_name"], "my-cli"
            )

    def test_refuses_unsafe_command_names_and_symlinked_outputs(self) -> None:
        for command_name in ("../my-cli", "-my-cli", "my cli"):
            with self.subTest(command_name=command_name):
                with self.assertRaises(generate_command_metadata.CommandMetadataError):
                    generate_command_metadata.validate_command_name(command_name)

        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            target = temp_path / "outside.json"
            target.write_text("outside\n", encoding="utf-8")
            output_path = temp_path / "my-cli.commands.json"
            try:
                output_path.symlink_to(target)
            except (NotImplementedError, OSError) as exc:
                self.skipTest(f"symlink creation unavailable: {exc}")

            with self.assertRaises(generate_command_metadata.CommandMetadataError):
                generate_command_metadata.write_metadata(output_path, "{}\n", force=True)
            self.assertEqual(target.read_text(encoding="utf-8"), "outside\n")


if __name__ == "__main__":
    unittest.main()
