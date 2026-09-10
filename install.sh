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
control_desktop_target="${HOME}/.local/share/applications/studio.warbler.Kadunce.Control.desktop"
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
/usr/bin/install -Dm644 "${project_dir}/assets/studio.warbler.kadunce-logo.png" \
    "${HOME}/.local/share/icons/hicolor/512x512/apps/studio.warbler.kadunce-logo.png"
bash "${project_dir}/control/prepare-repair.sh"
systemctl --user stop kadunce-control.service \
    >/dev/null 2>&1 || true
/usr/bin/install -Dm755 "${control_binary_source}" "${control_binary_target}"
/usr/bin/install -Dm644 \
    "${project_dir}/control/kadunce-control.service" \
    "${control_service_target}"
/usr/bin/install -Dm644 \
    "${project_dir}/control/studio.warbler.Kadunce.Control.desktop" \
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
pkexec /usr/bin/install -Dm755 "${native_plugin_source}" \
    "${native_plugin_system_target}"
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

# Qt keeps a native plugin library mapped for the lifetime of KWin. Unloading
# the effect removes its instance but a same-ID replacement can still create
# the old cached factory. Therefore every native Kadunce binary update
# has one honest completion condition: restart the Plasma session after copy.
echo "Kadunce installed and enabled."
echo "All six installation steps completed. It is now safe to restart Plasma."
echo "Receipt: ${install_receipt}"
echo "Restart Plasma once to replace KWin's cached native plugin image."
echo "Lift after 300 ms; hold over a destination for 350 ms, then release to stack."
echo "Edge paging starts at 300 ms and repeats every 350 ms."
echo "Ctrl+Left/Right pages groups; Ctrl+Up/Down pages stack members."
echo "Ctrl+B toggles Bento under the pointer."
echo "Without an external display, Ctrl+B uses the tablet as the fallback Bento stage."
echo "A lifted Card Line card can be released on that display to hand it over."
echo "Ctrl+S toggles; Ctrl+Esc releases."
echo "The Kadunce tray icon exposes one persistent enable/disable switch."
command -v notify-send >/dev/null 2>&1 \
    && notify-send "Kadunce ready" \
        "All six steps passed. Restart Plasma to load the refactored plugin." \
    || true
