#!/usr/bin/env bash
# One-shot test helper: clean old logs, upload, send, wait, fetch logs.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ $# -ge 1 ]; then
  HOST="$1"
else
  read -r -p "PS5 IP address: " HOST
fi

if [ -z "$HOST" ]; then
  echo "no host given" >&2
  exit 1
fi

python3 "$ROOT/tools/ytps5.py" clean-logs --host "$HOST" || true
python3 "$ROOT/tools/ytps5.py" send --host "$HOST" --wait 35 --save
