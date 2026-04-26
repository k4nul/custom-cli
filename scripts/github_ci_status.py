#!/usr/bin/env python3
"""Inspect GitHub Actions and commit checks for a repository commit.

The script prefers GH_TOKEN/GITHUB_TOKEN when present and otherwise falls back
to credentials returned by `git credential fill`, which lets private
repositories reuse the same HTTPS auth that already works for git fetch/push.
"""

from __future__ import annotations

import argparse
import base64
import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORKFLOW_PATH = ".github/workflows/ci.yml"
API_ROOT = "https://api.github.com"
WORKFLOW_RUN_URL_RE = re.compile(r"/actions/runs/(?P<run_id>\d+)")
BLOCKER_PATTERNS = {
    "github_actions_billing_or_budget_block": [
        re.compile(r"recent account payments failed", re.IGNORECASE),
        re.compile(r"spending limit", re.IGNORECASE),
        re.compile(r"budget limit", re.IGNORECASE),
        re.compile(r"payment method", re.IGNORECASE),
    ]
}
BLOCKER_NEXT_STEPS = {
    "github_actions_billing_or_budget_block": (
        "Check the repository owner's GitHub billing payment method and any "
        "Actions budgets that stop usage, then rerun the workflow."
    )
}
SUMMARY_TEXT_LIMIT = 240


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Inspect GitHub Actions runs, checks, and combined status for a commit."
    )
    parser.add_argument(
        "commit",
        help="Commit SHA to inspect.",
    )
    parser.add_argument(
        "--repo",
        help="Optional owner/repo override. Defaults to the origin remote.",
    )
    parser.add_argument(
        "--workflow-path",
        default=DEFAULT_WORKFLOW_PATH,
        help=f"Optional workflow path filter. Defaults to {DEFAULT_WORKFLOW_PATH}.",
    )
    parser.add_argument(
        "--wait-seconds",
        type=int,
        default=0,
        help="Poll for workflow runs and checks until they appear or the timeout expires.",
    )
    parser.add_argument(
        "--poll-interval",
        type=int,
        default=10,
        help="Polling interval in seconds when --wait-seconds is set. Defaults to 10.",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit machine-readable JSON instead of a text summary.",
    )
    return parser.parse_args()


def run_command(command: list[str], stdin: str | None = None) -> str:
    result = subprocess.run(
        command,
        input=stdin,
        text=True,
        capture_output=True,
        cwd=ROOT,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or f"Command failed: {' '.join(command)}")
    return result.stdout


def get_repo_slug(explicit_repo: str | None) -> str:
    if explicit_repo:
        return explicit_repo

    remote_url = run_command(["git", "remote", "get-url", "origin"]).strip()
    match = re.search(r"github\.com[:/](?P<owner>[^/]+)/(?P<repo>[^/.]+)(?:\.git)?$", remote_url)
    if not match:
        raise RuntimeError(f"Could not parse a GitHub owner/repo from origin URL: {remote_url}")
    return f"{match.group('owner')}/{match.group('repo')}"


def get_auth_headers(repo_slug: str) -> dict[str, str]:
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if token:
        return {
            "Authorization": f"Bearer {token}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
        }

    owner, repo = repo_slug.split("/", 1)
    credential_request = "\n".join(
        [
            "protocol=https",
            "host=github.com",
            f"path={owner}/{repo}.git",
            "",
            "",
        ]
    )
    output = run_command(["git", "credential", "fill"], stdin=credential_request)
    credentials: dict[str, str] = {}
    for line in output.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        credentials[key] = value

    username = credentials.get("username")
    password = credentials.get("password")
    if not username or not password:
        raise RuntimeError(
            "Could not resolve GitHub credentials. Set GH_TOKEN/GITHUB_TOKEN or ensure `git credential fill` can supply github.com credentials."
        )

    basic_token = base64.b64encode(f"{username}:{password}".encode("utf-8")).decode("ascii")
    return {
        "Authorization": f"Basic {basic_token}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
    }


def github_get(path: str, headers: dict[str, str]) -> Any:
    request = urllib.request.Request(f"{API_ROOT}{path}", headers=headers)
    try:
        with urllib.request.urlopen(request) as response:
            return json.load(response)
    except urllib.error.HTTPError as error:
        message = error.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"GitHub API request failed for {path}: {error.code} {message}") from error
    except urllib.error.URLError as error:
        raise RuntimeError(f"GitHub API request failed for {path}: {error.reason}") from error


def normalize_text(value: Any) -> str | None:
    if not isinstance(value, str):
        return None

    collapsed = " ".join(value.split())
    return collapsed or None


def trim_text(value: str, limit: int = SUMMARY_TEXT_LIMIT) -> str:
    if len(value) <= limit:
        return value
    return value[: limit - 3].rstrip() + "..."


def unique_texts(values: list[str]) -> list[str]:
    seen: set[str] = set()
    ordered: list[str] = []
    for value in values:
        if value in seen:
            continue
        seen.add(value)
        ordered.append(value)
    return ordered


def fetch_check_run_annotations(repo_slug: str, check_run_id: int, headers: dict[str, str]) -> list[dict[str, Any]]:
    annotations = github_get(
        f"/repos/{repo_slug}/check-runs/{check_run_id}/annotations?per_page=100",
        headers,
    )
    if not isinstance(annotations, list):
        return []
    return [annotation for annotation in annotations if isinstance(annotation, dict)]


def hydrate_check_run(
    repo_slug: str,
    check_run: dict[str, Any],
    headers: dict[str, str],
) -> dict[str, Any]:
    hydrated = dict(check_run)
    check_run_id = hydrated.get("id")
    if not isinstance(check_run_id, int):
        return hydrated

    try:
        details = github_get(f"/repos/{repo_slug}/check-runs/{check_run_id}", headers)
    except RuntimeError as error:
        hydrated["detail_fetch_error"] = str(error)
        details = None

    if isinstance(details, dict):
        hydrated.update(details)

    try:
        hydrated["annotations"] = fetch_check_run_annotations(repo_slug, check_run_id, headers)
    except RuntimeError as error:
        hydrated["annotation_fetch_error"] = str(error)

    return hydrated


def hydrate_check_runs(
    repo_slug: str,
    check_runs: list[dict[str, Any]],
    headers: dict[str, str],
) -> list[dict[str, Any]]:
    return [hydrate_check_run(repo_slug, check_run, headers) for check_run in check_runs]


def extract_check_run_notes(check_run: dict[str, Any]) -> list[str]:
    notes: list[str] = []

    output = check_run.get("output")
    if isinstance(output, dict):
        for key in ("title", "summary", "text"):
            text = normalize_text(output.get(key))
            if text:
                notes.append(text)

    annotations = check_run.get("annotations")
    if isinstance(annotations, list):
        for annotation in annotations:
            if not isinstance(annotation, dict):
                continue
            for key in ("title", "message", "raw_details"):
                text = normalize_text(annotation.get(key))
                if text:
                    notes.append(text)

    return unique_texts(notes)


def detect_known_blocker(check_run: dict[str, Any]) -> dict[str, Any] | None:
    for note in extract_check_run_notes(check_run):
        for blocker_kind, patterns in BLOCKER_PATTERNS.items():
            if any(pattern.search(note) for pattern in patterns):
                return {
                    "check_run_id": check_run.get("id"),
                    "check_run_name": check_run.get("name"),
                    "kind": blocker_kind,
                    "message": trim_text(note),
                    "next_step": BLOCKER_NEXT_STEPS[blocker_kind],
                }
    return None


def collect_known_blockers(check_runs: list[dict[str, Any]]) -> list[dict[str, Any]]:
    blockers: list[dict[str, Any]] = []
    for check_run in check_runs:
        blocker = detect_known_blocker(check_run)
        if blocker is not None:
            blockers.append(blocker)
    return blockers


def format_annotation(annotation: dict[str, Any]) -> str:
    location = normalize_text(annotation.get("path")) or "n/a"
    start_line = annotation.get("start_line")
    if isinstance(start_line, int):
        location = f"{location}:{start_line}"

    message = (
        normalize_text(annotation.get("message"))
        or normalize_text(annotation.get("title"))
        or "n/a"
    )
    return ", ".join(
        [
            f"level={annotation.get('annotation_level') or 'n/a'}",
            f"location={location}",
            f"message={trim_text(message, 160)}",
        ]
    )


def infer_workflow_run_ids(check_runs: list[dict[str, Any]]) -> set[int]:
    run_ids: set[int] = set()
    for check in check_runs:
        for candidate in (check.get("html_url"), check.get("details_url")):
            if not candidate:
                continue
            match = WORKFLOW_RUN_URL_RE.search(candidate)
            if match:
                run_ids.add(int(match.group("run_id")))
    return run_ids


def hydrate_workflow_runs(
    repo_slug: str,
    workflow_runs: list[dict[str, Any]],
    check_runs: list[dict[str, Any]],
    headers: dict[str, str],
) -> list[dict[str, Any]]:
    runs_by_id: dict[int, dict[str, Any]] = {}
    for run in workflow_runs:
        run_id = run.get("id")
        if isinstance(run_id, int):
            runs_by_id[run_id] = run

    for run_id in sorted(infer_workflow_run_ids(check_runs)):
        if run_id in runs_by_id:
            continue
        run = github_get(f"/repos/{repo_slug}/actions/runs/{run_id}", headers)
        if isinstance(run, dict):
            runs_by_id[run_id] = run

    return list(runs_by_id.values())


def fetch_commit_snapshot(repo_slug: str, commit: str, headers: dict[str, str]) -> dict[str, Any]:
    runs_query = urllib.parse.urlencode({"head_sha": commit, "per_page": 100})
    workflow_runs = github_get(f"/repos/{repo_slug}/actions/runs?{runs_query}", headers).get("workflow_runs", [])
    combined_status = github_get(f"/repos/{repo_slug}/commits/{commit}/status", headers)
    check_runs = github_get(f"/repos/{repo_slug}/commits/{commit}/check-runs?per_page=100", headers).get("check_runs", [])
    workflow_runs = hydrate_workflow_runs(repo_slug, workflow_runs, check_runs, headers)
    check_runs = hydrate_check_runs(repo_slug, check_runs, headers)
    return {
        "workflow_runs": workflow_runs,
        "combined_status": combined_status,
        "check_runs": check_runs,
    }


def filter_runs(workflow_runs: list[dict[str, Any]], workflow_path: str | None) -> list[dict[str, Any]]:
    push_runs = [run for run in workflow_runs if run.get("event") == "push"]
    if workflow_path:
        return [run for run in push_runs if run.get("path") == workflow_path]
    return push_runs


def has_observable_results(snapshot: dict[str, Any], workflow_path: str | None) -> bool:
    workflow_runs = filter_runs(snapshot["workflow_runs"], workflow_path)
    check_runs = snapshot["check_runs"]
    statuses = snapshot["combined_status"].get("statuses", [])
    return bool(workflow_runs or check_runs or statuses)


def wait_for_results(
    repo_slug: str,
    commit: str,
    headers: dict[str, str],
    workflow_path: str | None,
    wait_seconds: int,
    poll_interval: int,
) -> dict[str, Any]:
    snapshot = fetch_commit_snapshot(repo_slug, commit, headers)
    if wait_seconds <= 0 or has_observable_results(snapshot, workflow_path):
        return snapshot

    deadline = time.time() + wait_seconds
    while time.time() < deadline:
        time.sleep(poll_interval)
        snapshot = fetch_commit_snapshot(repo_slug, commit, headers)
        if has_observable_results(snapshot, workflow_path):
            return snapshot
    return snapshot


def summarize(snapshot: dict[str, Any], repo_slug: str, commit: str, workflow_path: str | None) -> str:
    lines: list[str] = [
        f"Repository: {repo_slug}",
        f"Commit: {commit}",
    ]

    filtered_runs = filter_runs(snapshot["workflow_runs"], workflow_path)
    if workflow_path:
        lines.append(f"Workflow filter: {workflow_path}")
    lines.append(f"Push workflow runs: {len(filtered_runs)}")
    for run in filtered_runs:
        lines.append(
            " - "
            + ", ".join(
                [
                    f"id={run.get('id')}",
                    f"name={run.get('name')}",
                    f"status={run.get('status')}",
                    f"conclusion={run.get('conclusion') or 'n/a'}",
                    f"event={run.get('event')}",
                    f"url={run.get('html_url')}",
                ]
            )
        )

    combined_status = snapshot["combined_status"]
    lines.append(f"Combined status state: {combined_status.get('state', 'none')}")
    statuses = combined_status.get("statuses", [])
    lines.append(f"Individual statuses: {len(statuses)}")
    for status in statuses:
        lines.append(
            " - "
            + ", ".join(
                [
                    f"context={status.get('context')}",
                    f"state={status.get('state')}",
                    f"description={status.get('description') or 'n/a'}",
                    f"url={status.get('target_url') or 'n/a'}",
                ]
            )
        )

    check_runs = snapshot["check_runs"]
    lines.append(f"Check runs: {len(check_runs)}")
    for check in check_runs:
        lines.append(
            " - "
            + ", ".join(
                [
                    f"name={check.get('name')}",
                    f"status={check.get('status')}",
                    f"conclusion={check.get('conclusion') or 'n/a'}",
                    f"url={check.get('html_url') or 'n/a'}",
                ]
            )
        )
        output = check.get("output")
        if isinstance(output, dict):
            summary = normalize_text(output.get("summary"))
            if summary:
                lines.append(f"   summary={trim_text(summary)}")

        annotations = check.get("annotations")
        if isinstance(annotations, list):
            for annotation in annotations[:3]:
                if isinstance(annotation, dict):
                    lines.append(f"   annotation={format_annotation(annotation)}")
            if len(annotations) > 3:
                lines.append(f"   annotation=... ({len(annotations) - 3} more)")

    blockers = collect_known_blockers(check_runs)
    if blockers:
        lines.append(f"Detected blockers: {len(blockers)}")
        for blocker in blockers:
            lines.append(
                " - "
                + ", ".join(
                    [
                        f"check={blocker.get('check_run_name') or 'n/a'}",
                        f"kind={blocker['kind']}",
                        f"message={blocker['message']}",
                        f"next_step={blocker['next_step']}",
                    ]
                )
            )

    return "\n".join(lines)


def main() -> int:
    args = parse_args()
    try:
        repo_slug = get_repo_slug(args.repo)
        headers = get_auth_headers(repo_slug)
        snapshot = wait_for_results(
            repo_slug=repo_slug,
            commit=args.commit,
            headers=headers,
            workflow_path=args.workflow_path,
            wait_seconds=args.wait_seconds,
            poll_interval=args.poll_interval,
        )
    except RuntimeError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1

    filtered_runs = filter_runs(snapshot["workflow_runs"], args.workflow_path)
    output = {
        "repo": repo_slug,
        "commit": args.commit,
        "workflow_path": args.workflow_path,
        "workflow_runs": filtered_runs,
        "combined_status": snapshot["combined_status"],
        "check_runs": snapshot["check_runs"],
        "detected_blockers": collect_known_blockers(snapshot["check_runs"]),
    }

    if args.json:
        json.dump(output, sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        print(summarize(snapshot, repo_slug, args.commit, args.workflow_path))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
