#!/usr/bin/env bash
# Print the site's cookie-free counter totals.
#
#   ./scripts/site-stats.sh          # last 14 days
#   ./scripts/site-stats.sh 30       # last 30 days
#
# Reads the same abacus keys that docs/analytics.js writes.

set -euo pipefail

DAYS="${1:-14}"
NS="ytps5-sponsorblock"
API="https://abacus.jasoncameron.dev"

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required" >&2
  exit 1
fi

python3 - "$DAYS" "$NS" "$API" <<'PY'
import datetime as dt
import json
import sys
import urllib.error
import urllib.request

days, ns, api = int(sys.argv[1]), sys.argv[2], sys.argv[3]


def get(key):
    try:
        with urllib.request.urlopen(f"{api}/get/{ns}/{key}", timeout=20) as r:
            return json.load(r).get("value", 0)
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return 0
        raise
    except Exception:
        return 0


today = dt.date.today()
rows = []
for i in range(days - 1, -1, -1):
    d = today - dt.timedelta(days=i)
    pv = get(f"pv-{d.isoformat()}")
    dl = get(f"dl-{d.isoformat()}")
    if pv or dl:
        rows.append((d.isoformat(), pv, dl))

print(f"\n{'date':12} {'pageviews':>10} {'downloads':>10}")
print("-" * 34)
for date, pv, dl in rows:
    print(f"{date:12} {pv:>10} {dl:>10}")
print("-" * 34)
print(f"{'total':12} {sum(r[1] for r in rows):>10} {sum(r[2] for r in rows):>10}\n")

# Referrers are cumulative keys, not per day.
refs = []
for host in [
    "t.co", "x.com", "twitter.com", "github.com", "google.com", "bing.com",
    "reddit.com", "discord.com", "youtube.com", "facebook.com", "forum.psx-place.com",
    "gbatemp.net", "internal", "direct", "other",
]:
    n = get(f"ref-{host}")
    if n:
        refs.append((host, n))

if refs:
    print("referrers (all time)")
    print("-" * 34)
    for host, n in sorted(refs, key=lambda r: -r[1]):
        print(f"{host:28} {n:>5}")
    print()
PY
