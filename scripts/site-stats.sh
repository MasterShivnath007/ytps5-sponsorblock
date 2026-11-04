#!/usr/bin/env bash
# Print the site's cookie-free counter totals.
#
#   ./scripts/site-stats.sh          # last 14 days
#   ./scripts/site-stats.sh 30       # last 30 days
#
# Reads the same abacus keys that docs/analytics.js writes. The service allows
# 30 requests per 10 seconds, so requests are paced and 429s are retried.

set -euo pipefail

DAYS="${1:-14}"
NS="ytps5-sponsorblock"
API="https://abacus.jasoncameron.dev"
HOSTS="t.co x.com twitter.com github.com google.com bing.com reddit.com discord.com youtube.com facebook.com forum.psx-place.com gbatemp.net internal direct other"

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required" >&2
  exit 1
fi

python3 - "$DAYS" "$NS" "$API" "$HOSTS" <<'PY'
import datetime as dt
import json
import sys
import time
import urllib.error
import urllib.request

days, ns, api, hosts = int(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4].split()


def get(key, attempt=0):
    url = f"{api}/get/{ns}/{key}"
    try:
        with urllib.request.urlopen(url, timeout=20) as r:
            return json.load(r).get("value", 0)
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return 0
        if e.code == 429 and attempt < 4:
            time.sleep(11)
            return get(key, attempt + 1)
        raise
    finally:
        time.sleep(0.35)  # stay under 30 requests / 10 s


today = dt.date.today()
rows = []
total_refs = 0

for i in range(days - 1, -1, -1):
    d = (today - dt.timedelta(days=i)).isoformat()
    pv = get(f"pv-{d}")
    dl = get(f"dl-{d}")
    if pv or dl:
        rows.append((d, pv, dl))

print(f"\n{'date':12} {'pageviews':>10} {'downloads':>10}")
print("-" * 34)
for date, pv, dl in rows:
    print(f"{date:12} {pv:>10} {dl:>10}")
print("-" * 34)
total_pv = sum(r[1] for r in rows)
total_dl = sum(r[2] for r in rows)
print(f"{'total':12} {total_pv:>10} {total_dl:>10}\n")

refs = []
for host in hosts:
    n = get(f"ref-{host}")
    if n:
        refs.append((host, n))
        total_refs += n

if refs:
    print("referrers (all time)")
    print("-" * 34)
    for host, n in sorted(refs, key=lambda r: -r[1]):
        print(f"{host:28} {n:>5}")
    print("-" * 34)
    if total_refs < total_pv:
        print(f"{'unattributed':28} {total_pv - total_refs:>5}  (new or unlisted source)")
    print()
else:
    print("referrers (all time): none yet\n")
PY
