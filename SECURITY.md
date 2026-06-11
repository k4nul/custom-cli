# Security Policy

## Supported Versions

Security fixes target the current `main` branch until the project publishes
versioned releases.

## Reporting a Vulnerability

Use GitHub private vulnerability reporting if it is enabled for the repository.
If it is not available, open a public issue with a short summary only and ask
for a private disclosure channel. Do not include exploit steps, private keys,
tokens, or sensitive reproduction data in a public issue.

Include:

- affected commit or release
- operating system and compiler
- impact summary
- minimal non-sensitive reproduction notes

## Secret Handling

Do not commit local configs, generated artifacts, credentials, tokens, private
keys, or terminal transcripts that contain sensitive values. Use ignored local
files such as `config/local.json` for experiments.
