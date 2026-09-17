#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: A2 ownership line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
check() { local result; result=$(probe "$@"); probe a2Evidence; test "$result" = true; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
qdbus6 studio.warbler.UnloadClient /Client companion
qdbus6 studio.warbler.UnloadClient /Client companion
sleep .4
check a2Setup
sleep .4
check a2Begin
sleep .6
qdbus6 studio.warbler.UnloadClient /Client largeCompanion
sleep .4
check a2Arrival false
sleep .6
qdbus6 studio.warbler.UnloadClient /Client oversizedCompanion
sleep .4
check a2Arrival true
sleep .6
check a2Prepared
echo 'PASS: constrained tablet launch receives large pane; oversized launch retains ownership and acknowledges Active geometry before minimizing'
qdbus6 studio.warbler.UnloadClient /Client crossCompanion
sleep .4
check a2CrossPrepare
sleep .4
check a2CrossAdmit
sleep .6
check a2Project
sleep .4
qdbus6 studio.warbler.UnloadClient /Client ordinaryCompanion
sleep .4
check a2OrdinaryNeighbor
sleep .4
check a2Return
sleep .6
check a2Project
sleep .4
check a2Release
sleep .6
check a2Restored
echo 'PASS: Bento group exact resume preserves rejected source state, ordinary neighbors, pane/overflow ownership, origins and monitor isolation'
check a2Reactivate
sleep .6
qdbus6 studio.warbler.UnloadClient /Client immediateCompanion
sleep .4
check a2ImmediatePlace
sleep .4
check a2ImmediateRelease
sleep .6
check a2Restored
echo 'PASS: release retires pending preparation; no delayed minimize can escape ownership lifetime'
