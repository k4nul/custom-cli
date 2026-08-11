# Generated Shell Completions

The interactive `Tab` completion described in the command reference works only
inside `cli-starter`'s own shell. Use `scripts/generate-completions.py` when a
copied starter should offer native command-name completion in Bash, Zsh, or
PowerShell. The helper reads the C++ root command registration sources and
renders deterministic scripts for every public root command; it never edits a
shell profile or installs files outside the chosen output directory.

Print one script to standard output and source it from the current session:

```bash
python3 scripts/generate-completions.py --shell bash > /tmp/cli-starter.bash
source /tmp/cli-starter.bash
```

For a renamed copied starter, pass the configured executable name:

```bash
python3 scripts/generate-completions.py --shell zsh --command-name my-cli > _my-cli
python3 scripts/generate-completions.py --shell powershell --command-name my-cli > my-cli.ps1
```

To generate all three files into a directory you control:

```bash
python3 scripts/generate-completions.py --command-name my-cli --output-dir ./completions
```

This writes `my-cli.bash`, `_my-cli`, and `my-cli.ps1`. Review and place those
files using your shell's normal completion setup; installation is intentionally
left to the operator. Regenerate the files after changing public command
registration in `src/app/cli_app.cpp` or `src/commands/register_commands.cpp`.
The helper refuses to replace an existing generated file unless `--force` is
explicitly supplied.

Validate generator behavior with:

```bash
bash scripts/test-generate-completions.sh
```
