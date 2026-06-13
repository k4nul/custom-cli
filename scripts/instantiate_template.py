#!/usr/bin/env python3
"""Create a safe rename plan for a copied CLI starter checkout."""

from __future__ import annotations

import argparse
import json
import re
import shlex
import sys
from dataclasses import dataclass
from pathlib import Path


DEFAULT_ENABLED_COMMANDS = ["about", "hello", "echo", "config", "doctor"]
GENERATED_TEMPLATE_NOTES = (
    "Rename values and trim sample commands once you start customizing the starter."
)
TOKEN_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")


class InstantiationError(ValueError):
    """Raised when requested starter names are unsafe or incomplete."""


@dataclass(frozen=True)
class InstantiationPlan:
    binary_name: str
    display_name: str
    config_file: str
    prompt_label: str
    build_dir: str

    @property
    def cmake_command(self) -> list[str]:
        return [
            "cmake",
            "-S",
            ".",
            "-B",
            self.build_dir,
            f"-DCLI_STARTER_BINARY_NAME={self.binary_name}",
            f"-DCLI_STARTER_DISPLAY_NAME={self.display_name}",
            f"-DCLI_STARTER_CONFIG_FILE={self.config_file}",
            f"-DCLI_STARTER_PROMPT_LABEL={self.prompt_label}",
            "-DBUILD_TESTING=ON",
            "-DCLI_STARTER_BUILD_TESTS=ON",
        ]

    @property
    def build_command(self) -> list[str]:
        return [
            "cmake",
            "--build",
            self.build_dir,
        ]

    @property
    def ctest_command(self) -> list[str]:
        return [
            "ctest",
            "--test-dir",
            self.build_dir,
            "--output-on-failure",
        ]

    @property
    def validation_command(self) -> str:
        return " && ".join(
            [
                shell_join(self.cmake_command),
                shell_join(self.build_command),
                shell_join(self.ctest_command),
            ]
        )

    @property
    def config_template(self) -> dict[str, object]:
        return {
            "prompt": self.prompt_label,
            "default_name": "world",
            "enabled_commands": list(DEFAULT_ENABLED_COMMANDS),
            "notes": GENERATED_TEMPLATE_NOTES,
        }

    @property
    def config_path(self) -> Path:
        return Path("config") / self.config_file


def shell_join(command: list[str]) -> str:
    return " ".join(shlex.quote(part) for part in command)


def reject_control_characters(label: str, value: str) -> None:
    if any(ord(character) < 32 or ord(character) == 127 for character in value):
        raise InstantiationError(f"{label} must not contain control characters")


def require_token(label: str, value: str) -> str:
    if not value:
        raise InstantiationError(f"{label} is required")
    reject_control_characters(label, value)
    if "/" in value or "\\" in value:
        raise InstantiationError(f"{label} must be a file name, not a path")
    if value in {".", ".."} or not TOKEN_PATTERN.match(value):
        raise InstantiationError(
            f"{label} must start with a letter or digit and use only letters, digits, '.', '_', or '-'"
        )
    return value


def require_display_name(value: str) -> str:
    if not value.strip():
        raise InstantiationError("display name is required")
    reject_control_characters("display name", value)
    return value


def derive_display_name(binary_name: str) -> str:
    words = [word for word in re.split(r"[-_.]+", binary_name) if word]
    return " ".join(word[:1].upper() + word[1:] for word in words) or binary_name


def derive_prompt_label(binary_name: str) -> str:
    label = "".join(character for character in binary_name if character.isalnum())
    return label or binary_name


def build_plan(args: argparse.Namespace) -> InstantiationPlan:
    binary_name = require_token("binary name", args.binary_name)
    display_name = require_display_name(args.display_name or derive_display_name(binary_name))
    config_file = require_token("config file", args.config_file or f"{binary_name}.json")
    if not config_file.endswith(".json"):
        raise InstantiationError("config file must end with .json")
    prompt_label = require_token("prompt label", args.prompt_label or derive_prompt_label(binary_name))
    build_dir = require_token("build directory", args.build_dir)
    return InstantiationPlan(
        binary_name=binary_name,
        display_name=display_name,
        config_file=config_file,
        prompt_label=prompt_label,
        build_dir=build_dir,
    )


def inspect_config_output_path(output_path: Path, force: bool) -> None:
    if output_path.is_symlink():
        raise InstantiationError(f"{output_path} must not be a symlink")
    if not output_path.exists():
        return
    if not output_path.is_file():
        raise InstantiationError(f"{output_path} is not a regular file")
    if not force:
        raise InstantiationError(f"{output_path} already exists; pass --force to replace it")


def write_config_template(plan: InstantiationPlan, repo_root: Path, force: bool) -> Path:
    output_path = repo_root / plan.config_path
    try:
        output_path.parent.mkdir(parents=True, exist_ok=True)
    except OSError as exc:
        raise InstantiationError(f"failed to prepare config directory for {output_path}: {exc}") from exc
    inspect_config_output_path(output_path, force)
    try:
        output_path.write_text(json.dumps(plan.config_template, indent=2) + "\n", encoding="utf-8")
    except OSError as exc:
        raise InstantiationError(f"failed to write config template to {output_path}: {exc}") from exc
    return output_path


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate the CMake and config steps for renaming a copied CLI starter."
    )
    parser.add_argument("--binary-name", required=True, help="Renamed executable, such as my-cli.")
    parser.add_argument("--display-name", help="Human-readable application name.")
    parser.add_argument("--config-file", help="Runtime config file name under config/.")
    parser.add_argument("--prompt-label", help="Prompt label used by generated config templates.")
    parser.add_argument("--build-dir", default="build", help="CMake build directory.")
    parser.add_argument(
        "--repo-root",
        default=".",
        help="Copied starter checkout where --write-config writes config/<name>.json.",
    )
    parser.add_argument(
        "--write-config",
        action="store_true",
        help="Write config/<config-file> with the renamed prompt label.",
    )
    parser.add_argument("--force", action="store_true", help="Allow --write-config to replace an existing file.")
    parser.add_argument("--json", action="store_true", help="Print the plan as JSON.")
    return parser.parse_args(argv)


def plan_as_json(plan: InstantiationPlan, wrote_config: str | None) -> str:
    payload = {
        "binary_name": plan.binary_name,
        "display_name": plan.display_name,
        "config_file": plan.config_file,
        "prompt_label": plan.prompt_label,
        "config_path": plan.config_path.as_posix(),
        "cmake_command": plan.cmake_command,
        "build_command": plan.build_command,
        "ctest_command": plan.ctest_command,
        "validation_command": plan.validation_command,
        "wrote_config": wrote_config,
    }
    return json.dumps(payload, indent=2)


def plan_as_text(plan: InstantiationPlan, wrote_config: str | None) -> str:
    lines = [
        "Template instantiation plan",
        f"- binary name: {plan.binary_name}",
        f"- display name: {plan.display_name}",
        f"- config file: {plan.config_path.as_posix()}",
        f"- prompt label: {plan.prompt_label}",
        "",
        "Configure:",
        shell_join(plan.cmake_command),
        "",
        "Validate:",
        plan.validation_command,
    ]
    if wrote_config is not None:
        lines.extend(["", f"Wrote config template: {wrote_config}"])
    else:
        lines.extend(["", "Config template not written; pass --write-config to create it."])
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        plan = build_plan(args)
        wrote_config = None
        if args.write_config:
            wrote_config = str(write_config_template(plan, Path(args.repo_root), args.force))
    except InstantiationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    if args.json:
        print(plan_as_json(plan, wrote_config))
    else:
        print(plan_as_text(plan, wrote_config))
    return 0


if __name__ == "__main__":
    sys.exit(main())
