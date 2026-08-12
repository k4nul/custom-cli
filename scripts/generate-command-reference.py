#!/usr/bin/env python3
"""Render a deterministic Markdown reference from the public command registry."""

from __future__ import annotations

import argparse
import ast
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SAFE_COMMAND_NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
CPP_STRING_LITERAL = r'"(?:\\.|[^"\\])*"'
CPP_STRING_SEQUENCE = rf"(?:{CPP_STRING_LITERAL}\s*)+"


class CommandReferenceError(ValueError):
    """Raised when command reference input or an output path is unsafe."""


@dataclass(frozen=True)
class Command:
    name: str
    description: str


@dataclass(frozen=True)
class GlobalOption:
    name: str
    description: str


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as exc:
        raise CommandReferenceError(f"cannot read {path}: {exc}") from exc


def decode_cpp_string_sequence(value: str) -> str:
    literals = re.findall(CPP_STRING_LITERAL, value)
    if not literals or re.sub(CPP_STRING_LITERAL, "", value).strip():
        raise CommandReferenceError("unsupported C++ string literal sequence")
    try:
        decoded = [ast.literal_eval(literal) for literal in literals]
    except (SyntaxError, ValueError) as exc:
        raise CommandReferenceError(f"cannot decode C++ string literal: {exc}") from exc
    if not all(isinstance(item, str) for item in decoded):
        raise CommandReferenceError("C++ command metadata must decode to strings")
    return "".join(decoded)


def extract_string_pair(source: str, call: str) -> tuple[str, str] | None:
    match = re.search(
        rf"{call}\(\s*({CPP_STRING_SEQUENCE})\s*,\s*({CPP_STRING_SEQUENCE})",
        source,
    )
    if match is None:
        return None
    return decode_cpp_string_sequence(match.group(1)), decode_cpp_string_sequence(match.group(2))


def extract_public_commands(repo_root: Path) -> tuple[Command, ...]:
    """Read command names and descriptions from the C++ CLI registration code."""

    cli_app = read_text(repo_root / "src/app/cli_app.cpp")
    registrations = read_text(repo_root / "src/commands/register_commands.cpp")
    commands: dict[str, str] = {}

    for match in re.finditer(
        rf"app\.add_subcommand\(\s*({CPP_STRING_SEQUENCE})\s*,\s*({CPP_STRING_SEQUENCE})",
        cli_app,
    ):
        name = decode_cpp_string_sequence(match.group(1))
        description = decode_cpp_string_sequence(match.group(2))
        validate_command_name(name)
        commands[name] = description

    for name in re.findall(
        r"register_([a-z][a-z0-9_]*)_command\(\s*root\s*,", registrations
    ):
        source = read_text(repo_root / "src/commands" / f"{name}_command.cpp")
        command = extract_string_pair(source, r"root\.add_subcommand")
        if command is None or command[0] != name:
            raise CommandReferenceError(
                f"cannot find public command metadata for {name} in its registrar"
            )
        commands[name] = command[1]

    if not commands:
        raise CommandReferenceError("no public commands found in the command registry")
    return tuple(Command(name, commands[name]) for name in sorted(commands))


def extract_global_options(repo_root: Path) -> tuple[GlobalOption, ...]:
    """Read explicit root options and include CLI11's built-in help flag."""

    cli_app = read_text(repo_root / "src/app/cli_app.cpp")
    help_all = extract_string_pair(cli_app, r"app\.set_help_all_flag")
    version = re.search(
        rf"app\.set_version_flag\(\s*({CPP_STRING_SEQUENCE})", cli_app
    )
    config = re.search(
        rf"app\.add_option\(\s*({CPP_STRING_SEQUENCE})\s*,\s*config_path\s*,\s*({CPP_STRING_SEQUENCE})",
        cli_app,
    )
    if help_all is None or version is None or config is None:
        raise CommandReferenceError("cannot find required global options in the root CLI setup")

    config_names = decode_cpp_string_sequence(config.group(1))
    config_description = decode_cpp_string_sequence(config.group(2))
    return (
        GlobalOption("--help", "Show top-level help."),
        GlobalOption(help_all[0], help_all[1]),
        GlobalOption(
            decode_cpp_string_sequence(version.group(1)),
            "Print the configured display name and version.",
        ),
        GlobalOption(config_names.replace(",", ", ") + " <path>", config_description),
    )


def validate_command_name(command_name: str) -> str:
    if not SAFE_COMMAND_NAME.fullmatch(command_name):
        raise CommandReferenceError(
            "command name must start with a letter or digit and use only letters, "
            "digits, '.', '_', or '-'"
        )
    return command_name


def escape_markdown(value: str) -> str:
    """Escape a value for one Markdown table cell."""

    normalized = value.replace("\r\n", "\n").replace("\r", "\n")
    escaped = (
        normalized.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace("`", "&#96;")
        .replace("\\", "\\\\")
        .replace("|", "\\|")
    )
    return escaped.replace("\n", "<br>")


def render_reference(
    command_name: str,
    commands: tuple[Command, ...],
    global_options: tuple[GlobalOption, ...],
) -> str:
    command_name = validate_command_name(command_name)
    lines = [
        f"# {command_name} Command Reference",
        "",
        "Generated from the public command registry; do not edit this file manually.",
        "",
        "## Global Options",
        "",
        "| Option | Description |",
        "| --- | --- |",
    ]
    lines.extend(
        f"| `{escape_markdown(option.name)}` | {escape_markdown(option.description)} |"
        for option in global_options
    )
    lines.extend(["", "## Commands", "", "| Command | Description |", "| --- | --- |"])
    lines.extend(
        f"| `{escape_markdown(command.name)}` | {escape_markdown(command.description)} |"
        for command in commands
    )
    return "\n".join(lines) + "\n"


def write_reference(output_path: Path, content: str, force: bool = False) -> Path:
    parent = output_path.parent
    absolute_parent = Path(os.path.abspath(parent))
    if any(component.is_symlink() for component in (absolute_parent, *absolute_parent.parents)):
        raise CommandReferenceError(
            f"output path must not contain symlinked directories: {parent}"
        )
    if not parent.is_dir():
        raise CommandReferenceError(
            f"output directory must be an existing non-symlink directory: {parent}"
        )
    if output_path.is_symlink() or (output_path.exists() and not output_path.is_file()):
        raise CommandReferenceError(f"output path is not a regular file: {output_path}")
    if output_path.exists() and not force:
        raise CommandReferenceError(
            f"output path already exists (pass --force to replace it): {output_path}"
        )
    flags = os.O_WRONLY | os.O_CREAT | getattr(os, "O_BINARY", 0)
    flags |= os.O_TRUNC if force else os.O_EXCL
    flags |= getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(output_path, flags, 0o666)
        with os.fdopen(descriptor, "wb") as output:
            output.write(content.encode("utf-8"))
    except OSError as exc:
        raise CommandReferenceError(f"cannot write {output_path}: {exc}") from exc
    return output_path


def write_stdout(content: str) -> None:
    stream = getattr(sys.stdout, "buffer", None)
    if stream is None:
        sys.stdout.write(content)
    else:
        stream.write(content.encode("utf-8"))


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--command-name", default="cli-starter", help="installed executable name")
    parser.add_argument("--output", type=Path, help="write the reference to this path")
    parser.add_argument("--force", action="store_true", help="replace an existing output file")
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT, help="copied starter root")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv if argv is not None else sys.argv[1:])
    try:
        content = render_reference(
            args.command_name,
            extract_public_commands(args.repo_root),
            extract_global_options(args.repo_root),
        )
        if args.output is None:
            write_stdout(content)
        else:
            print(write_reference(args.output, content, args.force))
    except CommandReferenceError as exc:
        print(f"command reference generation failed: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
