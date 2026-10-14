#!/usr/bin/env bash

set -euo pipefail

native_effect_id="kwin4_effect_kadunce"
# Unloading the native effect invokes its destructor. If Active is engaged,
# that destructor performs the saved geometry/tiling restoration first.
kwriteconfig6 --file kwinrc --group Plugins \
    --key "${native_effect_id}Enabled" false
# KWin answers false, or nothing, when the effect stayed loaded. Then kwinrc
# says on again, as the tray switch does, so it matches what is running.
if ! unloaded="$(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect \
        "${native_effect_id}" 2>/dev/null)" || [[ "${unloaded}" == false ]]; then
    kwriteconfig6 --file kwinrc --group Plugins \
        --key "${native_effect_id}Enabled" true
    echo "Kadunce could not confirm the Active restore/unload; Cards stay on." >&2
    echo "Press Meta+Esc before logging out or rebooting." >&2
    exit 1
fi

mkdir -p "${HOME}/.local/state/kadunce"
touch "${HOME}/.local/state/kadunce/disabled"

qdbus6 org.kde.KWin /KWin org.kde.KWin.reconfigure \
    >/dev/null 2>&1 || true

echo "Kadunce released and disabled; Active and Bento geometry were restored."
