#!/usr/bin/env bash
# Tell Bing, DuckDuckGo, Yandex and Naver that the site changed.
# Google ignores IndexNow and uses the sitemap in Search Console instead.
#
# Usage: ./scripts/submit-indexnow.sh [extra-url ...]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KEY="$(tr -d '[:space:]' < "$ROOT/scripts/indexnow-key.txt")"
HOST="mastershivnath007.github.io"
BASE="https://$HOST/ytps5-sponsorblock"

urls=(
  "$BASE/"
  "$BASE/how-it-works.html"
  "$BASE/testing.html"
)
urls+=("$@")

json_urls=""
for u in "${urls[@]}"; do
  json_urls+="${json_urls:+,}\"$u\""
done

payload=$(cat <<JSON
{
  "host": "$HOST",
  "key": "$KEY",
  "keyLocation": "$BASE/$KEY.txt",
  "urlList": [$json_urls]
}
JSON
)

echo "Submitting ${#urls[@]} URLs to IndexNow..."
curl -sS -o /tmp/indexnow.out -w 'HTTP %{http_code}\n' \
  -H 'Content-Type: application/json; charset=utf-8' \
  -d "$payload" https://api.indexnow.org/indexnow
if [ -s /tmp/indexnow.out ]; then cat /tmp/indexnow.out; echo; fi
