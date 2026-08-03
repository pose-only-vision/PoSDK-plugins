#!/bin/sh
set -eu
umask 077

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
if [ -n "${POSDK_PLUGIN_SDK_ROOT:-}" ]; then
  POSDK="$POSDK_PLUGIN_SDK_ROOT/bin/posdk"
else
  POSDK="$(command -v posdk || true)"
fi
[ -n "$POSDK" ] && [ -x "$POSDK" ] || { echo "PoSDK CLI is unavailable; set POSDK_PLUGIN_SDK_ROOT or add posdk to PATH" >&2; exit 69; }
PLUGIN_ID="degensac_two_view_estimator"
cd "$SCRIPT_DIR"
echo "PoSDK Marketplace Publisher · $PLUGIN_ID"
echo "1. Enter the GitHub owner/repository that will contain this plugin."
echo "2. Complete GitHub's official browser/device authorization if requested."
echo "3. Review the exact create/append/update plan before any GitHub change."
echo "4. Open the printed posdk.net URL, sign in, and approve the one-time plugin authorization."
echo "PoSDK never asks for, reads, or stores GitHub or posdk.net passwords."
"$POSDK" plugin publisher-init --source "$SCRIPT_DIR"
exec "$POSDK" plugin marketplace-publish --source "$SCRIPT_DIR" --interactive "$@"
