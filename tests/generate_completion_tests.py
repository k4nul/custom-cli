#!/usr/bin/env python3
"""Focused tests for generated Bash, Zsh, and PowerShell completions."""

from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path


sys.dont_write_bytecode = True
REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_PATH = REPO_ROOT / "scripts" / "generate-completions.py"
SPEC = importlib.util.spec_from_file_location("generate_completions", SCRIPT_PATH)
assert SPEC is not None
generate_completions = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = generate_completions
SPEC.loader.exec_module(generate_completions)


EXPECTED_COMMANDS = ("about", "config", "doctor", "echo", "hello", "shell")


class GenerateCompletionTests(unittest.TestCase):
    def test_extracts_every_public_command_from_the_cpp_registry(self) -> None:
        self.assertEqual(generate_completions.extract_public_commands(REPO_ROOT), EXPECTED_COMMANDS)

    def test_every_renderer_covers_every_public_command_deterministically(self) -> None:
        commands = generate_completions.extract_public_commands(REPO_ROOT)
        for shell in generate_completions.RENDERERS:
            with self.subTest(shell=shell):
                first = generate_completions.render(shell, "cli-starter", commands)
                self.assertEqual(first, generate_completions.render(shell, "cli-starter", commands))
                for command in EXPECTED_COMMANDS:
                    self.assertIn(command, first)
                self.assertIn("cli-starter", first)

    def test_output_directory_writes_all_shell_scripts_for_the_selected_binary(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            output_dir = Path(temp_dir) / "completions"
            outputs = generate_completions.write_outputs(
                output_dir,
                "my-cli",
                EXPECTED_COMMANDS,
            )

            self.assertEqual(
                {path.name for path in outputs},
                {"my-cli.bash", "_my-cli", "my-cli.ps1"},
            )
            for output_path in outputs:
                content = output_path.read_text(encoding="utf-8")
                self.assertIn("my-cli", content)
                for command in EXPECTED_COMMANDS:
                    self.assertIn(command, content)

            with self.assertRaises(generate_completions.CompletionGenerationError):
                generate_completions.write_outputs(output_dir, "my-cli", EXPECTED_COMMANDS)
            generate_completions.write_outputs(output_dir, "my-cli", EXPECTED_COMMANDS, force=True)

    def test_rejects_unsafe_command_names(self) -> None:
        for command_name in ("../my-cli", "-my-cli", "my cli"):
            with self.subTest(command_name=command_name):
                with self.assertRaises(generate_completions.CompletionGenerationError):
                    generate_completions.validate_command_name(command_name)

    def test_refuses_a_symlinked_generated_output(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            output_dir = Path(temp_dir) / "completions"
            output_dir.mkdir()
            target = Path(temp_dir) / "outside.bash"
            target.write_text("outside\n", encoding="utf-8")
            output_path = output_dir / "cli-starter.bash"
            try:
                output_path.symlink_to(target)
            except (NotImplementedError, OSError) as exc:
                self.skipTest(f"symlink creation unavailable: {exc}")

            with self.assertRaises(generate_completions.CompletionGenerationError):
                generate_completions.write_outputs(output_dir, "cli-starter", EXPECTED_COMMANDS)

            self.assertEqual(target.read_text(encoding="utf-8"), "outside\n")


if __name__ == "__main__":
    unittest.main()
