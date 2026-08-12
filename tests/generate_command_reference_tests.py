#!/usr/bin/env python3
"""Focused tests for deterministic Markdown command reference generation."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


sys.dont_write_bytecode = True
REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_PATH = REPO_ROOT / "scripts" / "generate-command-reference.py"
SPEC = importlib.util.spec_from_file_location("generate_command_reference", SCRIPT_PATH)
assert SPEC is not None
generate_command_reference = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = generate_command_reference
SPEC.loader.exec_module(generate_command_reference)


EXPECTED_COMMANDS = ("about", "config", "doctor", "echo", "hello", "shell")
EXPECTED_GLOBAL_OPTIONS = ("--help", "--help-all", "--version", "-c, --config <path>")


class GenerateCommandReferenceTests(unittest.TestCase):
    def test_extracts_every_public_command_and_global_option_from_cpp_sources(self) -> None:
        commands = generate_command_reference.extract_public_commands(REPO_ROOT)
        options = generate_command_reference.extract_global_options(REPO_ROOT)

        self.assertEqual(tuple(command.name for command in commands), EXPECTED_COMMANDS)
        self.assertEqual(tuple(option.name for option in options), EXPECTED_GLOBAL_OPTIONS)
        self.assertTrue(all(command.description for command in commands))
        self.assertTrue(all(option.description for option in options))

    def test_rendering_is_deterministic_and_covers_commands_and_options(self) -> None:
        first = generate_command_reference.render_reference(
            "cli-starter",
            generate_command_reference.extract_public_commands(REPO_ROOT),
            generate_command_reference.extract_global_options(REPO_ROOT),
        )

        self.assertEqual(
            first,
            generate_command_reference.render_reference(
                "cli-starter",
                generate_command_reference.extract_public_commands(REPO_ROOT),
                generate_command_reference.extract_global_options(REPO_ROOT),
            ),
        )
        self.assertTrue(first.endswith("\n"))
        self.assertIn("# cli-starter Command Reference", first)
        self.assertIn("## Global Options", first)
        self.assertIn("## Commands", first)
        for name in (*EXPECTED_GLOBAL_OPTIONS, *EXPECTED_COMMANDS):
            self.assertIn(f"`{generate_command_reference.escape_markdown(name)}`", first)

    def test_escapes_markdown_table_cells(self) -> None:
        rendered = generate_command_reference.render_reference(
            "my-cli",
            (
                generate_command_reference.Command(
                    "say|hello", "A \\ pipe | `command` <details> & <!-- hidden\r\nnext"
                ),
            ),
            (generate_command_reference.GlobalOption("--label", "first\nsecond"),),
        )

        self.assertIn("`say\\|hello`", rendered)
        self.assertIn("A \\\\ pipe \\| &#96;command&#96;", rendered)
        self.assertIn("first<br>second", rendered)
        self.assertIn("&#96;command&#96;", rendered)
        self.assertIn("&lt;details&gt;", rendered)
        self.assertIn("&amp; &lt;!-- hidden<br>next", rendered)
        self.assertNotIn("<details>", rendered)
        self.assertNotIn("<!--", rendered)

    def test_decodes_escaped_and_concatenated_cpp_string_literals(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            (root / "src/app").mkdir(parents=True)
            (root / "src/commands").mkdir(parents=True)
            (root / "src/app/cli_app.cpp").write_text(
                'app.set_help_all_flag("--help-all", "Show \\"all\\" " "commands.");\n'
                'app.set_version_flag("--version", version);\n'
                'app.add_option("-c,--config", config_path, "Path\\\\name " "value.");\n'
                'app.add_subcommand("shell", "Use \\"fast\\" " "mode\\\\path.");\n',
                encoding="utf-8",
            )
            (root / "src/commands/register_commands.cpp").write_text("", encoding="utf-8")

            commands = generate_command_reference.extract_public_commands(root)
            options = generate_command_reference.extract_global_options(root)
            self.assertEqual(commands[0].description, 'Use "fast" mode\\path.')
            self.assertEqual(options[1].description, 'Show "all" commands.')
            self.assertEqual(options[3].description, "Path\\name value.")

    def test_writes_only_to_an_explicit_regular_output_path(self) -> None:
        content = generate_command_reference.render_reference(
            "my-cli",
            generate_command_reference.extract_public_commands(REPO_ROOT),
            generate_command_reference.extract_global_options(REPO_ROOT),
        )
        with tempfile.TemporaryDirectory() as temp_dir:
            output_path = Path(temp_dir) / "my-cli.commands.md"
            self.assertEqual(
                generate_command_reference.write_reference(output_path, content), output_path
            )
            self.assertIn("# my-cli Command Reference", output_path.read_text(encoding="utf-8"))
            self.assertNotIn(b"\r\n", output_path.read_bytes())

            with self.assertRaises(generate_command_reference.CommandReferenceError):
                generate_command_reference.write_reference(output_path, content)
            generate_command_reference.write_reference(output_path, content, force=True)

    def test_command_line_writes_reference_to_the_requested_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            output_path = Path(temp_dir) / "my-cli.commands.md"
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(
                    generate_command_reference.main(
                        ["--command-name", "my-cli", "--output", str(output_path)]
                    ),
                    0,
                )
            self.assertIn("# my-cli Command Reference", output_path.read_text(encoding="utf-8"))

    def test_command_line_stdout_is_complete_and_uses_lf_bytes(self) -> None:
        captured = io.StringIO()
        with contextlib.redirect_stdout(captured):
            self.assertEqual(generate_command_reference.main([]), 0)
        self.assertEqual(
            captured.getvalue(),
            generate_command_reference.render_reference(
                "cli-starter",
                generate_command_reference.extract_public_commands(REPO_ROOT),
                generate_command_reference.extract_global_options(REPO_ROOT),
            ),
        )

        result = subprocess.run(
            [sys.executable, str(SCRIPT_PATH)],
            cwd=REPO_ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", errors="replace"))
        self.assertIn(b"# cli-starter Command Reference\n", result.stdout)
        self.assertNotIn(b"\r\n", result.stdout)

    def test_refuses_unsafe_command_names_and_symlinked_outputs(self) -> None:
        for command_name in ("../my-cli", "-my-cli", "my cli"):
            with self.subTest(command_name=command_name):
                with self.assertRaises(generate_command_reference.CommandReferenceError):
                    generate_command_reference.validate_command_name(command_name)

        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            target = temp_path / "outside.md"
            target.write_text("outside\n", encoding="utf-8")
            output_path = temp_path / "my-cli.commands.md"
            try:
                output_path.symlink_to(target)
            except (NotImplementedError, OSError) as exc:
                self.skipTest(f"symlink creation unavailable: {exc}")

            with self.assertRaises(generate_command_reference.CommandReferenceError):
                generate_command_reference.write_reference(output_path, "# test\n", force=True)
            self.assertEqual(target.read_text(encoding="utf-8"), "outside\n")

            real_parent = temp_path / "real"
            nested_parent = real_parent / "nested"
            nested_parent.mkdir(parents=True)
            linked_parent = temp_path / "linked"
            linked_parent.symlink_to(real_parent, target_is_directory=True)
            with self.assertRaises(generate_command_reference.CommandReferenceError):
                generate_command_reference.write_reference(
                    linked_parent / "nested" / "outside.md", "# test\n"
                )

            victim = temp_path / "victim.md"
            victim.write_text("keep\n", encoding="utf-8")
            with self.assertRaises(generate_command_reference.CommandReferenceError):
                generate_command_reference.write_reference(
                    linked_parent / ".." / "victim.md", "replace\n", force=True
                )
            self.assertEqual(victim.read_text(encoding="utf-8"), "keep\n")

    def test_force_write_is_atomic_when_replacement_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            output_path = Path(temp_dir) / "reference.md"
            output_path.write_text("original\n", encoding="utf-8")
            original_replace = generate_command_reference.os.replace

            def fail_replace(source, destination):
                raise OSError("simulated replacement failure")

            generate_command_reference.os.replace = fail_replace
            try:
                with self.assertRaises(generate_command_reference.CommandReferenceError):
                    generate_command_reference.write_reference(
                        output_path, "replacement\n", force=True
                    )
            finally:
                generate_command_reference.os.replace = original_replace

            self.assertEqual(output_path.read_bytes(), b"original\n")
            self.assertEqual(list(Path(temp_dir).glob(".reference.md.*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
