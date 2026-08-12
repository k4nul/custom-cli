#!/usr/bin/env python3
"""Render deterministic schema-versioned JSON from the public command registry."""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import asdict, dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SCHEMA_VERSION = "1.0.0"
SAFE_COMMAND_NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")


class CommandMetadataError(ValueError):
    """Raised when command metadata or an output path is unsafe."""


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
        raise CommandMetadataError(f"cannot read {path}: {exc}") from exc


def extract_public_commands(repo_root: Path) -> tuple[Command, ...]:
    """Read command names and descriptions from the C++ CLI registration code."""

    cli_app = read_text(repo_root / "src/app/cli_app.cpp")
    registrations = read_text(repo_root / "src/commands/register_commands.cpp")
    commands: dict[str, str] = {}

    for name, description in re.findall(
        r'app\.add_subcommand\(\s*"([A-Za-z0-9._-]+)"\s*,\s*"([^"]+)',
        cli_app,
    ):
        commands[name] = description

    for name in re.findall(
        r"register_([a-z][a-z0-9_]*)_command\(\s*root\s*,", registrations
    ):
        source = read_text(repo_root / "src/commands" / f"{name}_command.cpp")
        match = re.search(
            rf'root\.add_subcommand\(\s*"{re.escape(name)}"\s*,\s*"([^"]+)',
            source,
        )
        if match is None:
            raise CommandMetadataError(
                f"cannot find public command metadata for {name} in its registrar"
            )
        commands[name] = match.group(1)

    if not commands:
        raise CommandMetadataError("no public commands found in the command registry")
    return tuple(Command(name, commands[name]) for name in sorted(commands))


def extract_global_options(repo_root: Path) -> tuple[GlobalOption, ...]:
    """Read explicit root options and include CLI11's built-in help flag."""

    cli_app = read_text(repo_root / "src/app/cli_app.cpp")
    help_all = re.search(
        r'app\.set_help_all_flag\(\s*"([^"]+)"\s*,\s*"([^"]+)', cli_app
    )
    version = re.search(r'app\.set_version_flag\(\s*"([^"]+)', cli_app)
    config = re.search(
        r'app\.add_option\(\s*"([^"]+)"\s*,\s*config_path\s*,\s*"([^"]+)',
        cli_app,
    )
    if help_all is None or version is None or config is None:
        raise CommandMetadataError("cannot find required global options in the root CLI setup")

    config_names, config_description = config.groups()
    return (
        GlobalOption("--help", "Show top-level help."),
        GlobalOption(help_all.group(1), help_all.group(2)),
        GlobalOption(version.group(1), "Print the configured display name and version."),
        GlobalOption(config_names.replace(",", ", ") + " <path>", config_description),
    )


def validate_command_name(command_name: str) -> str:
    if not SAFE_COMMAND_NAME.fullmatch(command_name):
        raise CommandMetadataError(
            "command name must start with a letter or digit and use only letters, "
            "digits, '.', '_', or '-'"
        )
    return command_name


def render_metadata(
    command_name: str,
    commands: tuple[Command, ...],
    global_options: tuple[GlobalOption, ...],
) -> str:
    metadata = {
        "schema_version": SCHEMA_VERSION,
        "command_name": validate_command_name(command_name),
        "commands": [asdict(command) for command in commands],
        "global_options": [asdict(option) for option in global_options],
    }
    return json.dumps(metadata, indent=2, sort_keys=True) + "\n"


def write_metadata(output_path: Path, content: str, force: bool = False) -> Path:
    parent = output_path.parent
    if parent.is_symlink() or not parent.is_dir():
        raise CommandMetadataError(
            f"output directory must be an existing non-symlink directory: {parent}"
        )
    if output_path.is_symlink() or (output_path.exists() and not output_path.is_file()):
        raise CommandMetadataError(f"output path is not a regular file: {output_path}")
    if output_path.exists() and not force:
        raise CommandMetadataError(
            f"output path already exists (pass --force to replace it): {output_path}"
        )
    try:
        output_path.write_text(content, encoding="utf-8")
    except OSError as exc:
        raise CommandMetadataError(f"cannot write {output_path}: {exc}") from exc
    return output_path


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--command-name", default="cli-starter", help="installed executable name")
    parser.add_argument("--output", type=Path, help="write JSON metadata to this path")
    parser.add_argument("--force", action="store_true", help="replace an existing output file")
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT, help="copied starter root")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv if argv is not None else sys.argv[1:])
    try:
        content = render_metadata(
            args.command_name,
            extract_public_commands(args.repo_root),
            extract_global_options(args.repo_root),
        )
        if args.output is None:
            print(content, end="")
        else:
            print(write_metadata(args.output, content, args.force))
    except CommandMetadataError as exc:
        print(f"command metadata generation failed: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
