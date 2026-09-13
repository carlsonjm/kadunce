#!/usr/bin/env bash
set -euo pipefail
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case ${KADUNCE_PROBE_SESSION:-session.sh} in
    launch-runtime-session.sh) ;;
    native-entry-runtime-session.sh|x11-native-entry-runtime-session.sh|x11-tablet-runtime-session.sh) ;;
    x11-client-runtime-session.sh|x11-baseline-runtime-session.sh|x11-action-runtime-session.sh|x11-exit-runtime-session.sh) ;;
    session.sh|bento-session.sh|snap-session.sh|contact-session.sh|runtime-session.sh|tablet-runtime-session.sh|line-runtime-session.sh|local-runtime-session.sh|desktop-runtime-session.sh|tablet-entry-runtime-session.sh|x11-runtime-session.sh|exit-runtime-session.sh|trace-runtime-session.sh) ;;
    *) echo 'Unknown isolated probe session' >&2; exit 1 ;;
esac
unload_root=$(mktemp -d /tmp/kadunce-unload-test.XXXXXX)
output_count=1
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == bento-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == contact-session.sh ]]; then output_count=2; fi
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == *runtime-session.sh ]]; then output_count=2; test -d "${KADUNCE_RUNTIME_BUILD:?runtime build required}/bin"; fi
if [[ -n ${KADUNCE_TEST_OUTPUT_COUNT:-} ]]; then
    [[ $KADUNCE_TEST_OUTPUT_COUNT == 1 || $KADUNCE_TEST_OUTPUT_COUNT == 2 ]] || exit 2
    output_count=$KADUNCE_TEST_OUTPUT_COUNT
fi
echo "Isolated unload evidence: $unload_root"
kwin_binary=${KADUNCE_TEST_KWIN:-kwin_wayland}
if [[ -n ${KADUNCE_TEST_KWIN:-} ]]; then
    # An opt-in, disposable compositor build only; never replace the live KWin.
    [[ $kwin_binary == /tmp/* && -x $kwin_binary ]] || {
        echo 'KADUNCE_TEST_KWIN must name an executable disposable /tmp build' >&2
        exit 1
    }
fi
resolved_kwin=$(command -v -- "$kwin_binary")
sha256sum "$resolved_kwin" >"$unload_root/compositor.sha256"
ldd "$resolved_kwin" >"$unload_root/compositor-libraries.txt"
kwin_library=$(awk '$1 == "libkwin.so.6" && $2 == "=>" {print $3}' "$unload_root/compositor-libraries.txt")
if [[ -n $kwin_library && -r $kwin_library ]]; then
    sha256sum "$kwin_library" >>"$unload_root/compositor.sha256"
fi
cmake -S "$project_dir/tests/unload-probe" -B "$unload_root/build" -DBUILD_TESTING=OFF >"$unload_root/build.log" 2>&1
cmake --build "$unload_root/build" -j2 >>"$unload_root/build.log" 2>&1
mkdir -p "$unload_root/runtime" "$unload_root/config" "$unload_root/data" "$unload_root/state"
chmod 700 "$unload_root/runtime"
xwayland_args=()
xwayland_launcher=()
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == x11*runtime-session.sh ]]; then
    xwayland_args=(--xwayland)
    xwayland_launcher=(python3 "$project_dir/tests/private-xwayland.py")
fi
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == contact-session.sh || ${#xwayland_args[@]} != 0 ]]; then
    kwriteconfig6 --file "$unload_root/config/kwinrc" --group org.kde.kdecoration2 --key library org.kde.breeze
fi
timeout 40s env XDG_RUNTIME_DIR="$unload_root/runtime" \
    XDG_CONFIG_HOME="$unload_root/config" XDG_DATA_HOME="$unload_root/data" \
    XDG_STATE_HOME="$unload_root/state" QT_PLUGIN_PATH="$unload_root/build/bin:${KADUNCE_RUNTIME_BUILD:-/nonexistent}/bin" \
    KADUNCE_UNLOAD_PROBE_BUILD="$unload_root/build" KWIN_COMPOSE=O2 \
    LIBGL_ALWAYS_SOFTWARE=1 QT_WAYLAND_RECONNECT=0 \
    dbus-run-session -- "${xwayland_launcher[@]}" "$kwin_binary" --virtual --width 1280 --height 800 --output-count "$output_count" \
    --no-lockscreen --no-global-shortcuts --no-kactivities "${xwayland_args[@]}" \
    --exit-with-session "$project_dir/tests/unload-probe/${KADUNCE_PROBE_SESSION:-session.sh}" >"$unload_root/session.log" 2>&1
rg '^PASS:' "$unload_root/session.log"
# Keep bounded test artifacts/logs for inspection. Never install this probe.
