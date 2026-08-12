# Generated Command Reference

Use `scripts/generate-command-reference.py` to render a deterministic Markdown
reference from the public command registry. It reads the C++ root command
registration sources, includes every public command and global option, and does
not install documentation files.

Print the default `cli-starter` reference to standard output:

```bash
python3 scripts/generate-command-reference.py
```

For a renamed copied starter, choose the command name and an explicit output
path. The parent directory must already exist and must not be a symlink:

```bash
mkdir -p ./reference
python3 scripts/generate-command-reference.py \
  --command-name my-cli \
  --output ./reference/my-cli.commands.md
```

The helper refuses to replace an existing output file unless `--force` is
provided, and it refuses symlinked output paths. Regenerate and review the
reference after changing public command registration in `src/app/cli_app.cpp`
or `src/commands/register_commands.cpp`.

Validate generator behavior with:

```bash
bash scripts/test-generate-command-reference.sh
```
