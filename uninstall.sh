#!/usr/bin/env bash

set -euo pipefail

project_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
native_effect_id="kwin4_effect_kadunce"
# KWin looks for effects in Qt's plugin directory, which each distribution
# places differently (lib, lib64, a multiarch lib); Qt itself says where.
qt_plugin_dir() {
    local tool dir
    for tool in qtpaths6 qtpaths-qt6 /usr/lib/qt6/bin/qtpaths /usr/lib64/qt6/bin/qtpaths; do
        command -v "${tool}" >/dev/null 2>&1 || continue
        dir="$("${tool}" --query QT_INSTALL_PLUGINS 2>/dev/null)" || continue
        [[ "${dir}" == /* ]] && { printf '%s\n' "${dir}"; return 0; }
    done
    printf '%s\n' /usr/lib/qt6/plugins
}
native_plugin="$(qt_plugin_dir)/kwin/effects/plugins/kwin4_effect_kadunce.so"
control_binary="${HOME}/.local/bin/kadunce-control"
control_service="${HOME}/.config/systemd/user/kadunce-control.service"
control_desktop="${HOME}/.local/share/applications/co.goodinput.Kadunce.Control.desktop"
state_dir="${XDG_STATE_HOME:-${HOME}/.local/state}/kadunce"
repair_dir="${XDG_DATA_HOME:-${HOME}/.local/share}/kadunce/repair"
icon_dir="${HOME}/.local/share/icons/hicolor"

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
# The repair kit is source and a script kept for a plugin no longer here.
rm -rf -- "${repair_dir}"
rmdir --ignore-fail-on-non-empty "$(dirname -- "${repair_dir}")" 2>/dev/null || true
rm -f -- "${icon_dir}/512x512/apps/co.goodinput.kadunce-logo.png" \
    "${icon_dir}/scalable/apps/co.goodinput.kadunce-cards.svg" \
    "${icon_dir}/scalable/apps/co.goodinput.kadunce-cards-off.svg"
systemctl --user daemon-reload
command -v update-desktop-database >/dev/null 2>&1 \
    && update-desktop-database "${HOME}/.local/share/applications" \
        >/dev/null 2>&1 \
    || true

echo "Requesting permission to remove the native Kadunce plugin..."
install_key=/usr/local/libexec/shuffle/install-step
if [[ -x "${install_key}" ]] \
        && sudo -n -l "${install_key}" kadunce remove >/dev/null 2>&1; then
    sudo -n "${install_key}" kadunce remove
else
    pkexec /usr/bin/rm -f -- "${native_plugin}"
fi

kwriteconfig6 --file kwinrc --group Plugins \
    --key "${native_effect_id}Enabled" --delete
# Kadunce's state: the install receipt, the off mark, and repair's log, lock
# and the plugin it replaced. Settings in kaduncerc stay, for a later install.
rm -rf -- "${state_dir}"

echo "Kadunce was uninstalled. Log out and back in to clear KWin's plugin cache."
