#!/usr/bin/env bash
set -euo pipefail
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case ${KADUNCE_PROBE_SESSION:-session.sh} in
    guest-drawer-runtime-session.sh|guest-beside-runtime-session.sh|provenance-runtime-session.sh|active-launch-runtime-session.sh|placement-runtime-session.sh|placement-display-runtime-session.sh|companion-guest-runtime-session.sh|companion-focus-runtime-session.sh|adopt-reach-runtime-session.sh|late-maximize-runtime-session.sh) ;;
    side-runtime-session.sh|escape-carry-runtime-session.sh|sleeping-pane-runtime-session.sh|settle-runtime-session.sh) ;;
    stack-runtime-session.sh|start-cards-runtime-session.sh|minimized-start-runtime-session.sh|minimized-only-runtime-session.sh|sleeping-spread-runtime-session.sh|switcher-hidden-runtime-session.sh|gap-runtime-session.sh|bento-top-runtime-session.sh) ;;
    keyboard-runtime-session.sh|keyboard-minimized-runtime-session.sh|keyboard-chosen-runtime-session.sh|keyboard-dock-runtime-session.sh|keyboard-roll-runtime-session.sh|keyboard-focus-runtime-session.sh|keyboard-search-runtime-session.sh|keyboard-tap-runtime-session.sh|keyboard-table-runtime-session.sh) ;;
    keyboard-offscreen-runtime-session.sh|keyboard-spread-runtime-session.sh|keyboard-click-runtime-session.sh|keyboard-first-runtime-session.sh) ;;
    membership-runtime-session.sh|no-touch-runtime-session.sh|desktop-bezel-runtime-session.sh|output-unplug-runtime-session.sh|monitor-overflow-runtime-session.sh|monitor-lone-runtime-session.sh|monitor-full-runtime-session.sh|monitor-grid-runtime-session.sh|monitor-sent-runtime-session.sh|monitor-side-runtime-session.sh|first-entry-runtime-session.sh|bottom-release-runtime-session.sh|divider-tablet-runtime-session.sh|monitor-return-runtime-session.sh|monitor-bottom-runtime-session.sh|monitor-zones-runtime-session.sh) ;;
    lifetime-runtime-session.sh|ownership-session.sh|ownership-transition-session.sh) ;;
    spread-fingers-runtime-session.sh|spread-bento-drop-runtime-session.sh|flick-ask-runtime-session.sh|stack-still-runtime-session.sh) ;;
    active-admission-session.sh) ;;
    launch-runtime-session.sh) ;;
    native-entry-runtime-session.sh|x11-native-entry-runtime-session.sh|x11-tablet-runtime-session.sh) ;;
    card-exit-runtime-session.sh|bento-exit-partner-runtime-session.sh|first-carry-runtime-session.sh|dialog-runtime-session.sh|dialog-late-runtime-session.sh|dialog-electron-runtime-session.sh|dialog-card-runtime-session.sh|dialog-waiting-runtime-session.sh|desktop-switch-runtime-session.sh|desktop-switch-bento-runtime-session.sh) ;;
    table-preview-runtime-session.sh|table-multidisplay-runtime-session.sh|table-runtime-session.sh|table-pointer-runtime-session.sh|table-stack-runtime-session.sh|table-stack-layout-runtime-session.sh|table-spread-runtime-session.sh|desktop-settings-runtime-session.sh) ;;
    x11-client-runtime-session.sh|x11-baseline-runtime-session.sh|x11-action-runtime-session.sh|x11-exit-runtime-session.sh) ;;
    session.sh|bento-session.sh|snap-session.sh|contact-session.sh|runtime-session.sh|tablet-runtime-session.sh|line-runtime-session.sh|local-runtime-session.sh|desktop-runtime-session.sh|x11-runtime-session.sh|exit-runtime-session.sh|trace-runtime-session.sh) ;;
    *) echo 'Unknown isolated probe session' >&2; exit 1 ;;
esac
unload_root=$(mktemp -d /tmp/kadunce-unload-test.XXXXXX)
output_count=1
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == ownership-transition-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == ownership-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == bento-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == contact-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == active-admission-session.sh ]]; then output_count=2; fi
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == *runtime-session.sh ]]; then output_count=2; test -d "${KADUNCE_RUNTIME_BUILD:?runtime build required}/bin"; fi
if [[ -n ${KADUNCE_TEST_OUTPUT_COUNT:-} ]]; then
    [[ $KADUNCE_TEST_OUTPUT_COUNT == 1 || $KADUNCE_TEST_OUTPUT_COUNT == 2 ]] || exit 2
    output_count=$KADUNCE_TEST_OUTPUT_COUNT
fi
# A layout reaches its full pane count only on a monitor-sized display.
output_width=1280 output_height=800
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == monitor-full-runtime-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == monitor-grid-runtime-session.sh ]]; then
    output_width=2560 output_height=1440
fi
scale_args=()
case ${KADUNCE_PROBE_SESSION:-session.sh} in
    spread-fingers-runtime-session.sh|spread-bento-drop-runtime-session.sh|flick-ask-runtime-session.sh) scalable=1 ;;
    keyboard-runtime-session.sh|keyboard-offscreen-runtime-session.sh) scalable=1 ;;
    *) scalable=0 ;;
esac
session_script="$project_dir/tests/unload-probe/${KADUNCE_PROBE_SESSION:-session.sh}"
scaled_env=()
if ((scalable)) && [[ -n ${KADUNCE_TEST_SCALE:-} ]]; then
    # The Z13 panel at its own scale, where logical and device pixels differ.
    output_width=2560 output_height=1600
    if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == spread-fingers-runtime-session.sh ]]; then
        scale_args=(--scale "$KADUNCE_TEST_SCALE")
    else
        scaled_env=(KADUNCE_TEST_SCALE="$KADUNCE_TEST_SCALE" KADUNCE_SCALED_SESSION="$session_script")
        session_script="$project_dir/tests/unload-probe/scale-tablet.bash"
    fi
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
# A caller running many sessions builds the probe once and names it here; the
# build is identical for every session, so rebuilding it per session only adds time.
if [[ -n ${KADUNCE_PROBE_BUILD:-} ]]; then
    test -d "$KADUNCE_PROBE_BUILD/bin"
    ln -s "$KADUNCE_PROBE_BUILD" "$unload_root/build"
else
    cmake -S "$project_dir/tests/unload-probe" -B "$unload_root/build" -DBUILD_TESTING=OFF >"$unload_root/build.log" 2>&1
    cmake --build "$unload_root/build" -j2 >>"$unload_root/build.log" 2>&1
fi
mkdir -p "$unload_root/runtime" "$unload_root/config" "$unload_root/data" "$unload_root/state"
chmod 700 "$unload_root/runtime"
xwayland_args=()
xwayland_launcher=()
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == x11*runtime-session.sh ]]; then
    xwayland_args=(--xwayland)
    xwayland_launcher=(python3 "$project_dir/tests/private-xwayland.py")
fi
input_method_args=()
session_env=()
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == desktop-switch*-runtime-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == dialog-card-runtime-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == output-unplug-runtime-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == monitor-overflow-runtime-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == sleeping-spread-runtime-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == card-exit-runtime-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == bento-exit-partner-runtime-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == spread-bento-drop-runtime-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == stack-still-runtime-session.sh ]]; then
    session_env=(KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1)
fi
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == table-*runtime-session.sh ]]; then
    # Software rendering switches KWin's animations off; the slide must run
    # for the session to prove a Table switch stays out of it.
    session_env=(KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1 KWIN_EFFECTS_FORCE_ANIMATIONS=1)
fi
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == monitor-zones-runtime-session.sh ]]; then
    # The scene reads KWin's zones through scripts, whose printed lines are
    # kept only at debug level.
    session_env+=("QT_LOGGING_RULES=kwin_scripting.debug=true;js.debug=true;qml.debug=true")
fi
shortcut_args=(--no-global-shortcuts)
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == table-pointer-runtime-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == spread-fingers-runtime-session.sh ]]; then
    # Table's own key and a push into a top-left corner. KWin serves both only with
    # global shortcuts on; this compositor's input is its own either way.
    shortcut_args=()
fi
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == desktop-settings-runtime-session.sh ]]; then
    # The person's own settings: per-display switching and the pop-up both on.
    kwriteconfig6 --file "$unload_root/config/kwinrc" --group Windows --key PerOutputVirtualDesktops true
    kwriteconfig6 --file "$unload_root/config/kwinrc" --group Plugins --key desktopchangeosdEnabled true
fi
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == keyboard-*runtime-session.sh ]]; then
    # Lets the session photograph its own private outputs.
    session_env=(KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1)
    # A real input-method client, started by this private compositor only.
    input_method=$(command -v -- "${KADUNCE_TEST_INPUT_METHOD:-shuffle-keyboard}") || {
        echo 'keyboard-runtime needs an input method; name one in KADUNCE_TEST_INPUT_METHOD' >&2
        exit 1
    }
    input_method_args=(--inputmethod "$input_method")
    keyboard_mode=2
    case ${KADUNCE_PROBE_SESSION:-session.sh} in
        # Plasma's touch-only setting, as the tablet has it, from the first
        # moment: what a touch unlocks is part of what these scenes measure.
        keyboard-offscreen-runtime-session.sh|keyboard-spread-runtime-session.sh|keyboard-click-runtime-session.sh|keyboard-first-runtime-session.sh) keyboard_mode=1 ;;
    esac
    kwriteconfig6 --file "$unload_root/config/kwinrc" --group Wayland --key VirtualKeyboardMode "$keyboard_mode"
fi
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == contact-session.sh || ${#xwayland_args[@]} != 0 ]]; then
    kwriteconfig6 --file "$unload_root/config/kwinrc" --group org.kde.kdecoration2 --key library org.kde.breeze
fi
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == gap-runtime-session.sh ]]; then
    # Breeze with no borders keeps an invisible resize border outside a
    # window's frame, as an application drawing its own title bar does.
    kwriteconfig6 --file "$unload_root/config/kwinrc" --group org.kde.kdecoration2 --key library org.kde.breeze
    kwriteconfig6 --file "$unload_root/config/kwinrc" --group org.kde.kdecoration2 --key BorderSize None
fi
# Table's touch scene walks every gesture Table has, renaming included.
session_timeout=40s
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == table-runtime-session.sh ]]; then session_timeout=70s; fi
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == line-runtime-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == first-entry-runtime-session.sh ]]; then session_timeout=60s; fi
# Each bottom-edge exit on the card display, by pointer and by touch.
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == card-exit-runtime-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == spread-bento-drop-runtime-session.sh ]]; then session_timeout=120s; fi
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == keyboard-offscreen-runtime-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == keyboard-spread-runtime-session.sh ]]; then session_timeout=180s; fi
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == spread-fingers-runtime-session.sh ]]; then session_timeout=180s; fi
if [[ ${KADUNCE_PROBE_SESSION:-session.sh} == guest-beside-runtime-session.sh || ${KADUNCE_PROBE_SESSION:-session.sh} == table-spread-runtime-session.sh ]]; then session_timeout=90s; fi
# Every log line, Kadunce's included, goes to this session's log, not the
# journal of the person whose machine runs the test.
timeout "$session_timeout" env "${session_env[@]}" "${scaled_env[@]}" QT_FORCE_STDERR_LOGGING=1 XDG_RUNTIME_DIR="$unload_root/runtime" \
    XDG_CONFIG_HOME="$unload_root/config" XDG_DATA_HOME="$unload_root/data" \
    XDG_STATE_HOME="$unload_root/state" QT_PLUGIN_PATH="$unload_root/build/bin:${KADUNCE_RUNTIME_BUILD:-/nonexistent}/bin" \
    KADUNCE_UNLOAD_PROBE_BUILD="$unload_root/build" KWIN_COMPOSE=O2 \
    LIBGL_ALWAYS_SOFTWARE=1 QT_WAYLAND_RECONNECT=0 \
    dbus-run-session -- "${xwayland_launcher[@]}" "$kwin_binary" --virtual --width "$output_width" --height "$output_height" "${scale_args[@]}" --output-count "$output_count" \
    --no-lockscreen "${shortcut_args[@]}" --no-kactivities "${xwayland_args[@]}" "${input_method_args[@]}" \
    --exit-with-session "$session_script" >"$unload_root/session.log" 2>&1
rg '^PASS:' "$unload_root/session.log"
# Keep bounded test artifacts/logs for inspection. Never install this probe.
