#!/usr/bin/env bash
# While Kadunce runs, KWin's per-display desktop switching and KDE's
# desktop-name pop-up stay off, whatever the person's own settings say, through
# a reconfigure and without writing those settings; switching Kadunce off gives
# both back as the settings have them. The harness starts this session with
# both switched on in kwinrc.
set -Eeuo pipefail
trap 'echo "FAIL: desktop settings $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
popup() { qdbus6 org.kde.KWin /Scripting isScriptLoaded desktopchangeosd; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
for attempt in {1..40}; do [[ $(popup) == true ]] && break; sleep .1; done
test "$(probe perOutputDesktops)" = true
test "$(popup)" = true
echo "PASS: KDE starts with per-display switching and the pop-up on, as its settings say"
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
test "$(probe perOutputDesktops)" = false
test "$(popup)" = false
qdbus6 org.kde.KWin /KWin reconfigure
sleep .8
test "$(probe perOutputDesktops)" = false
test "$(popup)" = false
test "$(kreadconfig6 --file "$XDG_CONFIG_HOME/kwinrc" --group Windows --key PerOutputVirtualDesktops)" = true
test "$(kreadconfig6 --file "$XDG_CONFIG_HOME/kwinrc" --group Plugins --key desktopchangeosdEnabled)" = true
echo 'PASS: while Kadunce runs both stay off, through a reconfigure, and the settings are untouched'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
test "$(probe perOutputDesktops)" = true
test "$(popup)" = true
echo 'PASS: switching Kadunce off gives both back as the settings have them'
