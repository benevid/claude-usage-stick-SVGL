#!/bin/bash
# Helper for a Claude Code CLI `SessionStart` hook: starts token_bridge.py in
# the background if it isn't already running. See README -> "Tokens per
# session (optional bridge)" for how to wire this into ~/.claude/settings.json.
#
# Usage from ~/.claude/settings.json:
#   {
#     "hooks": {
#       "SessionStart": [
#         { "hooks": [
#           { "type": "command",
#             "command": "bash /absolute/path/to/claude-usage-stick-SVGL/tools/claude-code-token-bridge-hook.sh",
#             "timeout": 15 }
#         ] }
#       ]
#     }
#   }
#
# Multi-account setups: set CLAUDE_STICK_BRIDGE_ACCOUNT to the account label
# configured on the device before invoking this script (or copy/edit it per
# machine), same as the --account flag documented for token_bridge.py itself.
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCRIPT="$DIR/tools/token_bridge.py"
# ~/Library/Logs so existe no macOS; no Linux vale o XDG. Sem o mkdir o redirect
# falha e a ponte nao sobe — em silencio, porque o hook nao mostra stderr.
if [ -d "$HOME/Library/Logs" ]; then
  LOG_DIR="$HOME/Library/Logs"
else
  LOG_DIR="${XDG_STATE_HOME:-$HOME/.local/state}"
fi
LOG="${CLAUDE_STICK_BRIDGE_LOG:-$LOG_DIR/claude-usage-stick-token-bridge.log}"
mkdir -p "$(dirname "$LOG")"

ARGS=(--loop 120)
if [ -n "${CLAUDE_STICK_BRIDGE_ACCOUNT:-}" ]; then
  ARGS+=(--account "$CLAUDE_STICK_BRIDGE_ACCOUNT")
fi

if ! pgrep -f "$SCRIPT" >/dev/null; then
  nohup python3 -u "$SCRIPT" "${ARGS[@]}" >> "$LOG" 2>&1 &
fi
