#!/usr/bin/env bash
# Snapshot GitHub traffic and push the updated CSV.
#
# Used by the local LaunchAgent installed with scripts/install-traffic-agent.sh,
# because the default GITHUB_TOKEN in Actions cannot read traffic endpoints.

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PYTHON="$(command -v python3 || echo /opt/homebrew/bin/python3)"
GH="$(command -v gh || echo /opt/homebrew/bin/gh)"

echo "[$(date -u +%Y-%m-%dT%H:%M:%SZ)] snapshot start"

PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
  GH_TOKEN="$("$GH" auth token 2>/dev/null || true)" \
  "$PYTHON" scripts/snapshot_traffic.py || exit 1

if git diff --quiet -- metrics/traffic.csv; then
  echo "no change"
  exit 0
fi

git add metrics/traffic.csv
git -c user.name="ytps5 metrics" \
    -c user.email="metrics@users.noreply.github.com" \
    commit -q -m "[skip ci] metrics: traffic snapshot"

# The daily Actions job writes the same file, so rebase before pushing.
git pull --rebase --autostash -q origin main >/dev/null 2>&1 || true
if git push -q origin HEAD:main; then
  echo "pushed"
else
  echo "push failed; will retry next run"
  exit 1
fi
