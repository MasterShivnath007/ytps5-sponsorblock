#!/usr/bin/env bash
# Download the PS5 payload SDK binary distribution into third_party/.
# Usage: ./scripts/setup_sdk.sh [dest-dir]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${1:-$ROOT/third_party/ps5-sdk}"
URL="https://github.com/ps5-payload-dev/pacbrew-repo/releases/download/v0.41.1/ps5-payload-dev.tar.gz"

mkdir -p "$DEST"
echo "downloading $URL"
curl -L --fail --retry 3 -o "$DEST/ps5-payload-dev.tar.gz" "$URL"
tar -xzf "$DEST/ps5-payload-dev.tar.gz" -C "$DEST"
echo
echo "SDK extracted."
echo "Set:"
echo "  export PS5_PAYLOAD_SDK=$DEST/opt/ps5-payload-sdk"
echo "  export LLVM_CONFIG=/opt/homebrew/opt/llvm@18/bin/llvm-config   # macOS"
echo "  # or: export LLVM_CONFIG=llvm-config-18                         # Linux"
