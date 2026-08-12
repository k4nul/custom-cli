#!/usr/bin/env python3
"""Focused tests for deterministic command-reference man page generation."""

from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path


sys.dont_write_bytecode = True
REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_PATH = REPO_ROOT / "scripts" / "generate-manpage.py"
SPEC = importlib.util.spec_from_file_location("generate_manpage", SCRIPT_PATH)
assert SPEC is not None
generate_manpage = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = generate_manpage
SPEC.loader.exec_module(generate_manpage)


EXPECTED_COMMANDS = ("about", "config", "doctor", "echo", "hello", "shell")
EXPECTED_GLOBAL_OPTIONS = ("--help", "--help-all", "--version", "-c, --config <path>")


class GenerateManpageTests(unittest.TestCase):
    def test_extracts_every_public_command_from_the_cpp_registry(self) -> None:
        commands = generate_manpage.extract_public_commands(REPO_ROOT)

        self.assertEqual(tuple(command.name for command in commands), EXPECTED_COMMANDS)
        self.assertTrue(all(command.description for command in commands))

    def test_extracts_every_global_option_from_the_root_cli_setup(self) -> None:
        options = generate_manpage.extract_global_options(REPO_ROOT)

        self.assertEqual(tuple(option.name for option in options), EXPECTED_GLOBAL_OPTIONS)
        self.assertTrue(all(option.description for option in options))

    def test_rendering_is_deterministic_and_covers_commands_and_options(self) -> None:
        first = generate_manpage.render_manpage(
            "cli-starter",
            generate_manpage.extract_public_commands(REPO_ROOT),
            generate_manpage.extract_global_options(REPO_ROOT),
        )

        self.assertEqual(
            first,
            generate_manpage.render_manpage(
                "cli-starter",
                generate_manpage.extract_public_commands(REPO_ROOT),
                generate_manpage.extract_global_options(REPO_ROOT),
            ),
        )
        self.assertIn(".TH CLI-STARTER 1", first)
        self.assertIn(".SH GLOBAL OPTIONS", first)
        self.assertIn(".SH COMMANDS", first)
        for name in (*EXPECTED_GLOBAL_OPTIONS, *EXPECTED_COMMANDS):
            self.assertIn(f".B {name}", first)

    def test_writes_only_to_an_explicit_regular_output_path(self) -> None:
        content = generate_manpage.render_manpage(
            "my-cli",
            generate_manpage.extract_public_commands(REPO_ROOT),
            generate_manpage.extract_global_options(REPO_ROOT),
        )
        with tempfile.TemporaryDirectory() as temp_dir:
            output_path = Path(temp_dir) / "my-cli.1"
            self.assertEqual(
                generate_manpage.write_manpage(output_path, content), output_path
            )
            self.assertIn(".TH MY-CLI 1", output_path.read_text(encoding="utf-8"))

            with self.assertRaises(generate_manpage.ManpageGenerationError):
                generate_manpage.write_manpage(output_path, content)
            generate_manpage.write_manpage(output_path, content, force=True)

    def test_refuses_unsafe_command_names_and_symlinked_outputs(self) -> None:
        for command_name in ("../my-cli", "-my-cli", "my cli"):
            with self.subTest(command_name=command_name):
                with self.assertRaises(generate_manpage.ManpageGenerationError):
                    generate_manpage.validate_command_name(command_name)

        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            target = temp_path / "outside.1"
            target.write_text("outside\n", encoding="utf-8")
            output_path = temp_path / "my-cli.1"
            try:
                output_path.symlink_to(target)
            except (NotImplementedError, OSError) as exc:
                self.skipTest(f"symlink creation unavailable: {exc}")

            with self.assertRaises(generate_manpage.ManpageGenerationError):
                generate_manpage.write_manpage(output_path, "test\n", force=True)
            self.assertEqual(target.read_text(encoding="utf-8"), "outside\n")


if __name__ == "__main__":
    unittest.main()
