#!/usr/bin/env python3
"""Snapshot GitHub repo metrics into metrics/traffic.csv.

GitHub only keeps the last 14 days of traffic data, so anything worth
remembering has to be copied out daily. This script is idempotent: run it as
often as you like, it upserts one row per day and never rewrites history.

    python3 scripts/snapshot_traffic.py

Auth: uses GITHUB_TOKEN / GH_TOKEN if set, otherwise falls back to
`gh auth token`, so it works both in Actions and on your machine.
"""

from __future__ import annotations

import csv
import json
import os
import subprocess
import sys
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

REPO = os.environ.get("GH_REPO", "MasterShivnath007/ytps5-sponsorblock")
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "metrics" / "traffic.csv"

FIELDS = [
    "date",
    "views",
    "view_uniques",
    "clones",
    "clone_uniques",
    "stars",
    "forks",
    "elf_downloads",
    "zip_downloads",
    "referrers",
    "top_paths",
]


def token() -> str | None:
    for env in ("GITHUB_TOKEN", "GH_TOKEN"):
        if os.environ.get(env):
            return os.environ[env]
    try:
        return subprocess.run(
            ["gh", "auth", "token"], capture_output=True, text=True, check=True
        ).stdout.strip()
    except Exception:
        return None


TOKEN = token()
FAILED: list[tuple[str, int]] = []


def get(path: str):
    url = f"https://api.github.com/repos/{REPO}{path}"
    req = urllib.request.Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "ytps5-metrics",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )
    if TOKEN:
        req.add_header("Authorization", f"Bearer {TOKEN}")
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            return json.load(r)
    except urllib.error.HTTPError as e:
        FAILED.append((path, e.code))
        if e.code == 403:
            # Expected inside Actions with the default token.
            print(f"warn: {path} -> HTTP 403 (no access)", file=sys.stderr)
        else:
            print(f"warn: {path} -> HTTP {e.code}", file=sys.stderr)
        return None
    except Exception as e:  # network hiccup: keep the previous history
        FAILED.append((path, 0))
        print(f"warn: {path} -> {e}", file=sys.stderr)
        return None


def main() -> int:
    views = get("/traffic/views") or {}
    clones = get("/traffic/clones") or {}
    referrers = get("/traffic/popular/referrers") or []
    paths = get("/traffic/popular/paths") or []
    repo = get("") or {}
    releases = get("/releases?per_page=100") or []

    # Traffic endpoints need push access, which the default GITHUB_TOKEN does
    # not have. Say so loudly instead of writing a file that looks fine.
    traffic_blocked = any(
        path.startswith("/traffic/") and code == 403 for path, code in FAILED
    )
    if traffic_blocked:
        print(
            "::warning::Traffic endpoints returned 403. Add a repo secret "
            "TRAFFIC_TOKEN (classic PAT with 'repo' scope, or the account's "
            "gh auth token) to collect views and clones from Actions.",
            file=sys.stderr,
        )

    per_day: dict[str, dict[str, str]] = {}

    for entry in views.get("views", []):
        d = entry["timestamp"][:10]
        per_day.setdefault(d, {})["views"] = str(entry["count"])
        per_day[d]["view_uniques"] = str(entry["uniques"])

    for entry in clones.get("clones", []):
        d = entry["timestamp"][:10]
        per_day.setdefault(d, {})["clones"] = str(entry["count"])
        per_day[d]["clone_uniques"] = str(entry["uniques"])

    elf = 0
    bundles = 0
    for rel in releases:
        for asset in rel.get("assets", []):
            if asset["name"].lower().endswith(".elf"):
                elf += asset.get("download_count", 0)
            else:
                bundles += asset.get("download_count", 0)

    today = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    per_day.setdefault(today, {})

    # Days before the repo existed are noise, not history.
    created = (repo.get("created_at") or "")[:10]
    if created:
        per_day = {d: v for d, v in per_day.items() if d >= created or d == today}

    # Referrers and paths are 14-day aggregates, so they only belong on the
    # row for the day this snapshot was taken.
    ref_summary = ";".join(f'{r["referrer"]}:{r["count"]}' for r in referrers[:10])
    path_summary = ";".join(f'{p["path"].rstrip("/") or "/"}:{p["count"]}' for p in paths[:10])

    for d, row in per_day.items():
        if traffic_blocked and d == today:
            # Do not overwrite numbers recorded by a run that had access.
            for key in ("views", "view_uniques", "clones", "clone_uniques"):
                row.pop(key, None)
            continue
        row.setdefault("views", "0")
        row.setdefault("view_uniques", "0")
        row.setdefault("clones", "0")
        row.setdefault("clone_uniques", "0")

    per_day[today]["stars"] = str(repo.get("stargazers_count", 0))
    per_day[today]["forks"] = str(repo.get("forks_count", 0))
    per_day[today]["elf_downloads"] = str(elf)
    per_day[today]["zip_downloads"] = str(bundles)
    if ref_summary:
        per_day[today]["referrers"] = ref_summary
    if path_summary:
        per_day[today]["top_paths"] = path_summary

    existing: dict[str, dict[str, str]] = {}
    if OUT.exists():
        with OUT.open(newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f):
                if row.get("date"):
                    existing[row["date"]] = {k: (row.get(k) or "") for k in FIELDS}

    # Merge: traffic numbers always refresh (they can still change for a day
    # or two), but a referrer summary recorded on its own day is kept.
    for d, row in per_day.items():
        merged = existing.get(d, {k: "" for k in FIELDS})
        for k, v in row.items():
            if v not in ("", None):
                merged[k] = v
        merged["date"] = d
        existing[d] = merged

    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS, lineterminator="\n")
        w.writeheader()
        for d in sorted(existing):
            w.writerow({k: existing[d].get(k, "") for k in FIELDS})

    print(f"wrote {OUT.relative_to(ROOT)} ({len(existing)} days, elf downloads {elf})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
