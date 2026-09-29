#!/usr/bin/env bash

set -euo pipefail

native_effect_id="kwin4_effect_kadunce"
mkdir -p "${HOME}/.local/state/kadunce"
touch "${HOME}/.local/state/kadunce/disabled"

# Unloading the native effect invokes its destructor. If Active is engaged,
# that destructor performs the saved geometry/tiling restoration first.
kwriteconfig6 --file kwinrc --group Plugins \
    --key "${native_effect_id}Enabled" false
if ! qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect \
        "${native_effect_id}" >/dev/null 2>&1; then
    echo "Kadunce could not confirm the Active restore/unload." >&2
    echo "Press Ctrl+Esc before logging out or rebooting." >&2
    exit 1
fi

qdbus6 org.kde.KWin /KWin org.kde.KWin.reconfigure \
    >/dev/null 2>&1 || true

echo "Kadunce released and disabled; Active and Bento geometry were restored."
