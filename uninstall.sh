#!/usr/bin/env bash

set -euo pipefail

project_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
native_effect_id="kwin4_effect_kadunce"
native_plugin="/usr/lib/qt6/plugins/kwin/effects/plugins/kwin4_effect_kadunce.so"
control_binary="${HOME}/.local/bin/kadunce-control"
control_service="${HOME}/.config/systemd/user/kadunce-control.service"
control_desktop="${HOME}/.local/share/applications/studio.warbler.Kadunce.Control.desktop"
state_dir="${XDG_STATE_HOME:-${HOME}/.local/state}/kadunce"

if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded \
        "${native_effect_id}" 2>/dev/null | grep -qx true; then
    "${project_dir}/disable.sh"
else
    kwriteconfig6 --file kwinrc --group Plugins \
        --key "${native_effect_id}Enabled" false
fi

systemctl --user disable --now kadunce-control.service \
    >/dev/null 2>&1 || true
rm -f -- "${control_binary}" "${control_service}" "${control_desktop}"
systemctl --user daemon-reload
command -v update-desktop-database >/dev/null 2>&1 \
    && update-desktop-database "${HOME}/.local/share/applications" \
        >/dev/null 2>&1 \
    || true

echo "Requesting permission to remove the native Kadunce plugin..."
pkexec /usr/bin/rm -f -- "${native_plugin}"

kwriteconfig6 --file kwinrc --group Plugins \
    --key "${native_effect_id}Enabled" --delete
rm -f -- "${state_dir}/disabled" "${state_dir}/last-install.txt"
rmdir --ignore-fail-on-non-empty "${state_dir}" 2>/dev/null || true

echo "Kadunce was uninstalled. Log out and back in to clear KWin's plugin cache."
