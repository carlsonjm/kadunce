#!/usr/bin/env bash
set -euo pipefail
# Disposable fixture only: Virtual-0 tablet predicate and direct system edges.
# Neither override belongs in the production plugin.
trap 'echo "FAIL: lifetime line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
# The bezel is Kadunce's to recognise only with the Z13 kit's posture file.
mkdir -p "$XDG_RUNTIME_DIR/z13-tablet-kit"
echo tablet >"$XDG_RUNTIME_DIR/z13-tablet-kit/posture"
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
# Plasma always draws a desktop background under the bezel.
client desktopSurface
sleep .5
probe pointer 500 350
# KDE's Overview is set aside while Kadunce runs, and loaded again as it stops.
overview() { qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded overview; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect overview >/dev/null || true
overview_before=$(overview)
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
main=$(kad workspaceContext | jq -r '.applications[0].windowId')
# Loading makes the window in use the Active card at once, so its desktop
# geometry is read with the effect unloaded.
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .3
original=$(probe windowGeometry "$main")
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .3
kad showCardLine
kad showActive
sleep .3
active=$(probe windowGeometry "$main")
for cycle in 1 2 3; do
    kad showCardLine
    sleep .2
    test "$(probe windowGeometry "$main")" = "$active"
    kad showActive
    sleep .2
    test "$(probe windowGeometry "$main")" = "$active"
done
echo 'PASS: repeated Active/Spread keeps native geometry'
# A native resize request must not discard managed ownership or geometry.
client armResize
probe down 71 500 600
sleep .15
probe motion 71 500 550
probe up 71
sleep .2
kad workspaceContext | jq -e '.cardStage.presentation == "active"'
test "$(probe windowGeometry "$main")" = "$active"
echo 'PASS: Active resize rejected without ownership release'
# Three fingers drawn down short of halfway and let go: back to the Active
# card. Past halfway: Spread, with no native resize either way.
for p in 0.1 0.2 0.3; do probe spreadGesture "$p"; sleep .03; done
kad workspaceContext | jq -e '.cardStage.presentation == "cardLine"'
probe spreadGestureEnd
sleep .5
kad workspaceContext | jq -e '.cardStage.presentation == "active"'
test "$(probe windowGeometry "$main")" = "$active"
for p in 0.2 0.4 0.6 0.8; do probe spreadGesture "$p"; sleep .03; done
probe spreadGestureEnd
sleep .6
kad workspaceContext | jq -e '.cardStage.presentation == "cardLine"'
test "$(probe windowGeometry "$main")" = "$active"
echo 'PASS: three fingers open Spread past halfway, go back short of it, and never resize natively'
kad showActive
# A second Active app must not overwrite the first app restore record.
client companion
sleep .7
test "$(probe windowGeometry "$main")" = "$active"
kad showCardLine
sleep .2
test "$(probe windowGeometry "$main")" = "$active"
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .3
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .3
# A layout ends into card ownership (CARD-LIFECYCLE.md §5) rather than
# returning the window to the desktop; the unload below proves its restore
# record survived the second app and the round trip.
kad showCardLine
kad showActive
kad showCardLine
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .3
test "$(probe windowGeometry "$main")" = "$original"
echo 'PASS: unload from Spread restores original desktop geometry'
if [[ $overview_before == true ]]; then
    for attempt in {1..20}; do [[ $(overview) == true ]] && break; sleep .1; done
    test "$(overview)" = true
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep .3
    test "$(overview)" = false
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    for attempt in {1..20}; do [[ $(overview) == true ]] && break; sleep .1; done
    test "$(overview)" = true
    echo "PASS: KDE's Overview is set aside while Kadunce runs and back once it stops"
else
    echo "SKIP: this compositor has no Overview to set aside"
fi
