#!/usr/bin/env bash

set -euo pipefail

project_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
native_effect_id="kwin4_effect_kadunce"
build_root="$(mktemp -d /tmp/kadunce-install.XXXXXX)"
native_build_dir="${build_root}/native"
control_build_dir="${build_root}/control"
native_plugin_source="${native_build_dir}/bin/kwin/effects/plugins/kwin4_effect_kadunce.so"
native_plugin_system_target="/usr/lib/qt6/plugins/kwin/effects/plugins/kwin4_effect_kadunce.so"
control_binary_source="${control_build_dir}/bin/kadunce-control"
control_binary_target="${HOME}/.local/bin/kadunce-control"
control_service_target="${HOME}/.config/systemd/user/kadunce-control.service"
control_desktop_target="${HOME}/.local/share/applications/co.goodinput.Kadunce.Control.desktop"
install_state_dir="${XDG_STATE_HOME:-${HOME}/.local/state}/kadunce"
install_receipt="${install_state_dir}/last-install.txt"
install_succeeded=false
registration_started=false
previous_native_enabled="$(kreadconfig6 --file kwinrc --group Plugins \
    --key "${native_effect_id}Enabled" --default false)"

cleanup() {
    local status=$?
    if [[ -d "${build_root}" ]]; then
        find "${build_root}" -depth -delete
    fi
    if [[ "${install_succeeded}" != true \
          && "${registration_started}" == true ]]; then
        # Restore every pre-install effect flag if registration fails after
        # the byte-verified plugin copy. Validation and build failures happen
        # before this point and therefore never touch the live desktop.
        kwriteconfig6 --file kwinrc --group Plugins \
            --key "${native_effect_id}Enabled" \
            "${previous_native_enabled}" || true
        qdbus6 org.kde.KWin /KWin org.kde.KWin.reconfigure \
            >/dev/null 2>&1 || true
        # Registration unloaded the effect, and reconfiguring does not load
        # it again, so an effect that was on is asked for by name.
        if [[ "${previous_native_enabled}" == true ]]; then
            qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect \
                "${native_effect_id}" >/dev/null 2>&1 || true
        fi
        echo "Kadunce installation stopped before completion." >&2
        echo "The previous effect configuration was restored." >&2
        command -v notify-send >/dev/null 2>&1 \
            && notify-send "Kadunce install stopped" \
                "The previous workspace configuration was restored." \
            || true
    fi
    return "${status}"
}
trap cleanup EXIT

echo "[1/6] Verifying the accepted behavior contracts..."
"${project_dir}/tests/verify-source.sh"
"${project_dir}/tests/verify-package.sh"
bash "${project_dir}/tests/verify-control.sh"

echo "[2/6] Building the per-user workspace control..."
cmake -S "${project_dir}/control" -B "${control_build_dir}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "${control_build_dir}" -j2

echo "[3/6] Building the native workspace plugin..."
cmake -S "${project_dir}/native" -B "${native_build_dir}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "${native_build_dir}" -j2

echo "[4/6] Updating the per-user workspace control..."
/usr/bin/install -Dm644 "${project_dir}/assets/co.goodinput.kadunce-logo.png" \
    "${HOME}/.local/share/icons/hicolor/512x512/apps/co.goodinput.kadunce-logo.png"
for icon in co.goodinput.kadunce-cards co.goodinput.kadunce-cards-off; do
    /usr/bin/install -Dm644 "${project_dir}/control/assets/${icon}.svg" \
        "${HOME}/.local/share/icons/hicolor/scalable/apps/${icon}.svg"
done
bash "${project_dir}/control/prepare-repair.sh"
systemctl --user stop kadunce-control.service \
    >/dev/null 2>&1 || true
/usr/bin/install -Dm755 "${control_binary_source}" "${control_binary_target}"
/usr/bin/install -Dm644 \
    "${project_dir}/control/kadunce-control.service" \
    "${control_service_target}"
/usr/bin/install -Dm644 \
    "${project_dir}/control/co.goodinput.Kadunce.Control.desktop" \
    "${control_desktop_target}"
systemctl --user daemon-reload
systemctl --user reenable kadunce-control.service >/dev/null
systemctl --user start kadunce-control.service
systemctl --user is-active --quiet kadunce-control.service

# KWin's native plugin search path on this Plasma installation is the system
# Qt directory. User-local installation succeeds as a file copy but is not
# discovered when KWin starts, so the effect constructor and shortcuts never
# exist. Use the same narrow, proven destination as the existing native KWin
# effects; the administrator prompt authorizes this one file only.
echo "[5/6] Requesting permission for the one native plugin file..."
echo "Do not reboot until this window reports all six steps complete."
# Shuffle's install key, where it is set up, places this one file with no
# password: a root-owned helper takes it as a stream and puts it only here.
install_key=/usr/local/libexec/shuffle/install-step
if [[ -x "${install_key}" ]] \
        && sudo -n -l "${install_key}" kadunce install >/dev/null 2>&1; then
    tar -C "$(dirname -- "${native_plugin_source}")" -cf - \
        "$(basename -- "${native_plugin_source}")" \
        | sudo -n "${install_key}" kadunce install
else
    pkexec /usr/bin/install -Dm755 "${native_plugin_source}" \
        "${native_plugin_system_target}"
fi
cmp "${native_plugin_source}" "${native_plugin_system_target}"

# Only a fully built, copied, byte-verified candidate may disturb the running
# effect. KWin keeps the old mapped image alive until restart, so this is a
# short registration transition rather than an in-session binary swap.
echo "[6/6] Registering the verified candidate..."
registration_started=true
kwriteconfig6 --file kwinrc --group Plugins \
    --key "${native_effect_id}Enabled" false
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect \
    "${native_effect_id}" >/dev/null 2>&1 || true

kwriteconfig6 --file kwinrc --group Plugins \
    --key "${native_effect_id}Enabled" true

/usr/bin/install -d "${install_state_dir}"
rm -f "${install_state_dir}/disabled"
{
    printf 'installed_at=%s\n' "$(date --iso-8601=seconds)"
    printf 'candidate_revision=0.1.0-kadunce-baseline\n'
    printf 'candidate_sha256=%s\n' \
        "$(sha256sum "${native_plugin_source}" | cut -d' ' -f1)"
    printf 'installed_sha256=%s\n' \
        "$(sha256sum "${native_plugin_system_target}" | cut -d' ' -f1)"
    printf 'enabled=%s\n' \
        "$(kreadconfig6 --file kwinrc --group Plugins \
            --key "${native_effect_id}Enabled")"
} >"${install_receipt}"
systemctl --user start kadunce-control.service
systemctl --user is-active --quiet kadunce-control.service
bash "${project_dir}/tests/verify-live-control.sh"
install_succeeded=true

# A file-manager launch normally inherits the graphical session bus. Repair
# it from the standard user bus path if necessary, then make KWin discover the
# newly installed native plugin.
if ! qdbus6 org.kde.KWin /KWin org.kde.KWin.reconfigure \
        >/dev/null 2>&1; then
    export DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$(id -u)/bus"
    qdbus6 org.kde.KWin /KWin org.kde.KWin.reconfigure \
        >/dev/null 2>&1 || true
fi

# Enabling the plugin and reconfiguring does not bring the effect back: KWin
# reads the flag but does not instantiate an effect that was unloaded in this
# session. Without this the install finishes with the workspace carrying no
# Kadunce at all until something else loads it. Ask for it by name, then prove
# the effect constructed by talking to the object it registers, because a
# loaded plugin that failed to build its controllers still reports loaded.
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect \
    "${native_effect_id}" >/dev/null 2>&1 || true
effect_loaded=false
for _ in $(seq 1 20); do
    if [[ "$(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded \
            "${native_effect_id}" 2>/dev/null)" == true ]] \
       && qdbus6 org.kde.KWin /Kadunce >/dev/null 2>&1; then
        effect_loaded=true
        break
    fi
    sleep 0.1
done

# Qt keeps a native plugin library mapped for the lifetime of KWin. Unloading
# the effect removes its instance but a same-ID replacement can still create
# the old cached factory, so loading after an install can re-instantiate the
# previous build. Replacing the file gives it a new inode, so the mapping KWin
# holds is the evidence. This script cannot read it: under a restricted-ptrace
# kernel a process may only read the maps of its own descendants, and KWin is
# not one. The effect reads its own and reports it, so ask the effect. An empty
# answer means the running effect has no such method, which is itself the
# answer: it is an older build than the one just installed.
installed_inode="$(stat -c %i "${native_plugin_system_target}")"
live_provenance="$(qdbus6 org.kde.KWin /Kadunce loadedPluginProvenance \
    2>/dev/null || true)"
live_inode="${live_provenance%% *}"
live_state="${live_provenance##* }"

# Prints the controls map docs/INPUT.md opens with, a destination to a line.
controls_map() {
    awk -F'|' '/^\| *---/ && !seen { inmap = 1; seen = 1; next }
        inmap && /^\|/ {
            gsub(/`/, "")
            for (i = 2; i <= 4; i++) gsub(/^ +| +$/, "", $i)
            printf "  %s: %s, or %s\n", $2, $3, $4
            next
        }
        inmap { exit }' "$1"
}

echo "Kadunce installed and enabled."
echo "All six installation steps completed. It is now safe to restart Plasma."
echo "Receipt: ${install_receipt}"
if [[ "${effect_loaded}" != true ]]; then
    live_summary="Restart Plasma: the workspace has no Kadunce running."
    echo "WARNING: the effect did not come back after this install." >&2
    echo "The plugin is in place and enabled, but KWin is not running it," >&2
    echo "so the workspace currently has no Kadunce. Restart Plasma." >&2
elif [[ -z "${live_provenance}" ]]; then
    live_summary="Restart Plasma: KWin is running an older build."
    echo "WARNING: KWin is running an OLDER Kadunce build." >&2
    echo "The effect that answered cannot report which plugin image it is," >&2
    echo "which only older builds do, so it is not the one just installed." >&2
    echo "Restart Plasma once, then test." >&2
elif [[ "${live_inode}" == "${installed_inode}" && "${live_state}" == present ]]; then
    live_summary="KWin is running the build just installed."
    echo "KWin is running the build this install just placed."
else
    live_summary="Restart Plasma: KWin still has the previous build mapped."
    echo "WARNING: KWin is still running the PREVIOUS Kadunce build." >&2
    echo "It kept the earlier plugin image mapped across the unload, so what" >&2
    echo "you are about to test is not what was just installed." >&2
    echo "Restart Plasma once, then test." >&2
fi
if [[ -f "${project_dir}/docs/INPUT.md" ]]; then
    echo "Controls (docs/INPUT.md has every gesture and key):"
    controls_map "${project_dir}/docs/INPUT.md"
fi
echo "The Kadunce tray icon exposes one persistent enable/disable switch."
command -v notify-send >/dev/null 2>&1 \
    && notify-send "Kadunce ready" \
        "All six steps passed. ${live_summary}" \
    || true
