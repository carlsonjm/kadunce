#!/usr/bin/env bash
# With no dock on the tablet, as when a monitor becomes the primary display and
# takes it, the bottom bezel lies over the desktop background alone. Plasma draws
# that background as a layer surface, and a swipe from the bezel must still open
# Spread: the background owns no touch of its own.
#
# Needs the tablet fixture, and the Z13 kit's posture file so the bezel is
# Kadunce's to recognise rather than Plasma's.
set -Eeuo pipefail
trap 'echo "FAIL: desktop bezel $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
mkdir -p "$XDG_RUNTIME_DIR/z13-tablet-kit"
echo tablet >"$XDG_RUNTIME_DIR/z13-tablet-kit/posture"
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client desktopSurface
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
context() { kad workspaceContext | jq -e "$1" >/dev/null; }
context '.displayContext.edgeBackend == "z13-direct"'
context '.cardStage.presentation == "active"'
# The bezel band holds nothing but the background.
bottom=$(probe windowAt 640 796)
printf 'bezel %s\n' "$bottom"
jq -e '.target.class == "unload-client" and .target.layer == 0' <<<"$bottom" >/dev/null
echo 'PASS: the tablet presents an Active card over a desktop background, with no dock'
probe down 91 640 796
for y in 760 720 660 600 540; do probe motion 91 640 "$y"; sleep .03; done
probe up 91
sleep .6
printf 'after %s\n' "$(kad workspaceContext | jq -c '.cardStage')"
context '.cardStage.presentation == "cardLine"'
echo 'PASS: a swipe from the bezel over the desktop background opens Spread'
