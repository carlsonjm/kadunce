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
# A short swipe that stops before letting go: the Active card goes back.
probe down 91 640 796
for y in 780 765 752 748; do probe motion 91 640 "$y"; sleep .15; done
sleep .2
probe up 91
sleep .6
context '.cardStage.presentation == "active"'
echo 'PASS: a short swipe from the bezel that stops lets the Active card go back'
# Pulled past halfway, however slowly, Spread opens.
probe down 92 640 796
for y in 780 760 745 720 690 660 630 600 570 540; do probe motion 92 640 "$y"; sleep .1; done
sleep .2
probe up 92
sleep .8
printf 'after %s\n' "$(kad workspaceContext | jq -c '.cardStage')"
context '.cardStage.presentation == "cardLine"'
echo 'PASS: a swipe from the bezel past halfway opens Spread'
kad showActive
sleep .6
context '.cardStage.presentation == "active"'
# A quick flick opens it however short.
probe down 93 640 796
for y in 770 745 725; do probe motion 93 640 "$y"; done
probe up 93
sleep .8
context '.cardStage.presentation == "cardLine"'
echo 'PASS: a quick flick from the bezel opens Spread'
