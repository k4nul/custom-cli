# Generated Command Metadata

Use `scripts/generate-command-metadata.py` to render deterministic,
schema-versioned JSON from the public command registry. The generated document
contains the selected executable name, every public root command, and every
global option with its description. It reads the C++ registration sources and
does not install files into the system.

Print metadata for the default `cli-starter` command to standard output:

```bash
python3 scripts/generate-command-metadata.py
```

For a renamed copied starter, choose the command name and an explicit output
path. The parent directory must already exist and must not be a symlink:

```bash
mkdir -p ./metadata
python3 scripts/generate-command-metadata.py \
  --command-name my-cli \
  --output ./metadata/my-cli.commands.json
```

The helper refuses to replace an existing output file unless `--force` is
provided, and it refuses symlinked output paths. Consumers should use the
`schema_version` field to identify the document shape before reading command
metadata. Regenerate and review the file after changing root command
registration in `src/app/cli_app.cpp` or `src/commands/register_commands.cpp`.

Validate generator behavior with:

```bash
bash scripts/test-generate-command-metadata.sh
```
