# Template Instantiation

Use `scripts/instantiate_template.py` after copying this starter into a new
repository and before replacing the sample commands. The script keeps the rename
workflow in one executable place: it validates safe project names, prints the
CMake cache variables needed for the renamed build, and can write the matching
`config/<name>.json` template.

Plan a rename without changing files:

```bash
python3 scripts/instantiate_template.py \
  --binary-name my-cli \
  --display-name "My CLI" \
  --config-file my-cli.json \
  --prompt-label mycli
```

Create the matching config template in a copied checkout:

```bash
python3 scripts/instantiate_template.py \
  --binary-name my-cli \
  --display-name "My CLI" \
  --config-file my-cli.json \
  --prompt-label mycli \
  --write-config
```

Then run the printed validation command. It configures the renamed executable,
builds it, and runs the CTest suite with output-on-failure enabled.

The script intentionally refuses path-like names, control characters, unsafe
prompt labels, and non-JSON config file names. It also refuses to replace an
existing config file unless `--force` is passed, and it refuses symlink or
non-regular output paths before writing.

Validate this workflow with:

```bash
python3 tests/instantiate_template_tests.py
```

The normal CMake/CTest validation flow also runs these tests through the
`template_instantiation_workflow` CTest entry:

```bash
ctest --test-dir build --output-on-failure -R '^template_instantiation_workflow$'
```
