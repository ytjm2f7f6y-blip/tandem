#!/bin/bash
# Installs Tandem (Audio Unit) for the current user.
# Run it from Terminal (this picks the newest Tandem disk that is open):
#   bash "$(ls -td /Volumes/tandem*/ | head -1)install.sh"
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
NAME="Tandem"
SHOWN="Tandem"
COMPONENTS="$HOME/Library/Audio/Plug-Ins/Components"
VERSION="$(defaults read "$HERE/$NAME.component/Contents/Info" CFBundleShortVersionString 2>/dev/null || echo "?")"

# A music app that has the plugin open would keep running the old one.
for app in "Logic Pro" "Logic Pro X" "GarageBand" "MainStage"; do
    if pgrep -xq "$app"; then
        echo "Please quit $app first (Cmd+Q), then run this again."
        exit 1
    fi
done

echo "Installing $SHOWN version $VERSION from: $HERE"
mkdir -p "$COMPONENTS"
rm -rf "$COMPONENTS/$NAME.component"
cp -R "$HERE/$NAME.component" "$COMPONENTS/"

# These builds are signed for personal use rather than by a registered Apple
# developer, so macOS would block them as "downloaded from the internet".
# Clearing that flag on this one item is what lets it load.
xattr -dr com.apple.quarantine "$COMPONENTS/$NAME.component" 2>/dev/null || true

# Make Logic notice the new Audio Unit: drop the system's Audio Unit cache
# (macOS rebuilds it) and restart its registrar.
rm -f "$HOME/Library/Caches/AudioUnitCache/com.apple.audiounits.cache" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo
echo "Done: $SHOWN $VERSION installed to:"
echo "  $COMPONENTS/$NAME.component"
echo "Restart Logic, then find '$SHOWN' under Audio Units > Rumpus Room Records."
