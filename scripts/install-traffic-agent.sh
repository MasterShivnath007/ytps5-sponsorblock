#!/usr/bin/env bash
# Install (or remove) a LaunchAgent that snapshots GitHub traffic daily.
#
#   ./scripts/install-traffic-agent.sh            # install
#   ./scripts/install-traffic-agent.sh uninstall  # remove
#
# It runs scripts/snapshot-and-push.sh at 03:17 local time and logs to
# ~/Library/Logs/ytps5-traffic.log. If the Mac is asleep at that time, launchd
# runs the job when it wakes.

set -euo pipefail

LABEL="com.mastershivnath007.ytps5.traffic"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
LOG="$HOME/Library/Logs/ytps5-traffic.log"

case "${1:-install}" in
  uninstall)
    launchctl bootout "gui/$UID" "$PLIST" >/dev/null 2>&1 || true
    [ -f "$PLIST" ] && unlink "$PLIST"
    echo "removed $LABEL"
    exit 0
    ;;
  install) ;;
  *)
    echo "usage: $0 [install|uninstall]" >&2
    exit 2
    ;;
esac

mkdir -p "$HOME/Library/LaunchAgents" "$HOME/Library/Logs"

cat > "$PLIST" <<PLIST_EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>$LABEL</string>
  <key>ProgramArguments</key>
  <array>
    <string>/bin/bash</string>
    <string>$ROOT/scripts/snapshot-and-push.sh</string>
  </array>
  <key>WorkingDirectory</key>
  <string>$ROOT</string>
  <key>StartCalendarInterval</key>
  <dict>
    <key>Hour</key>
    <integer>3</integer>
    <key>Minute</key>
    <integer>17</integer>
  </dict>
  <key>RunAtLoad</key>
  <false/>
  <key>ProcessType</key>
  <string>Background</string>
  <key>LowPriorityIO</key>
  <true/>
  <key>StandardOutPath</key>
  <string>$LOG</string>
  <key>StandardErrorPath</key>
  <string>$LOG</string>
  <key>EnvironmentVariables</key>
  <dict>
    <key>PATH</key>
    <string>/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin</string>
  </dict>
</dict>
</plist>
PLIST_EOF

launchctl bootout "gui/$UID" "$PLIST" >/dev/null 2>&1 || true
launchctl bootstrap "gui/$UID" "$PLIST"
launchctl enable "gui/$UID/$LABEL"

echo "installed $LABEL"
echo "  runs: 03:17 local, script $ROOT/scripts/snapshot-and-push.sh"
echo "  log:  $LOG"
echo "  test: launchctl kickstart -k gui/$UID/$LABEL"
