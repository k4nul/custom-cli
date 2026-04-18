# Project Overview

## Purpose

This repository is a reusable C++ CLI starter, not a product-specific operations console. Its job is to provide a clean base for building custom command-line tools with:

- a simple build entry point,
- consistent command registration,
- a small JSON configuration story,
- an interactive mode for exploration, and
- starter-grade tests and docs.

## Direction

The legacy repository mixed three concerns:

- a CLI host,
- a plugin framework,
- and organization-specific operational commands.

The new direction intentionally narrows scope. This starter keeps the reusable CLI experience and removes domain-specific operational behavior.

## What Was Removed

- Organization names, product names, support details, and deployment paths
- Product update logic, admin escalation flows, and hidden maintenance commands
- External build assumptions tied to unavailable sibling repositories

## What Was Kept Or Rebuilt

- Dual usage model: one-shot commands plus an interactive shell
- Nested command structure
- Config-file workflow
- Clear extension points for adding new commands

## Intended Users

- Developers who want a copyable CLI template
- Teams that need a small C++ command shell without inheriting old product baggage
- Future maintainers who need a repository that explains itself quickly

