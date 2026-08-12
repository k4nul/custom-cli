# Generated Man Pages

Use `scripts/generate-manpage.py` to render a deterministic roff man page from
the public command registry. It reads the same C++ root command registration
sources as the starter CLI, includes every public command and global option,
and never installs a man page into the system.

Print the default `cli-starter` man page to standard output:

```bash
python3 scripts/generate-manpage.py > cli-starter.1
```

For a renamed copied starter, choose the command name and an explicit output
path. The parent directory must already exist and must not be a symlink:

```bash
mkdir -p ./man
python3 scripts/generate-manpage.py \
  --command-name my-cli \
  --output ./man/my-cli.1
```

The helper refuses to replace an existing output file unless `--force` is
provided, and it refuses symlinked output paths. Review the generated roff
before placing it in a package or a platform-specific documentation location.
System-wide installation is intentionally left to the operator.

Validate generator behavior with:

```bash
bash scripts/test-generate-manpage.sh
```
