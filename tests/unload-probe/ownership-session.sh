#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: ownership line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
qdbus6 studio.warbler.UnloadClient /Client companion
sleep .4
result=$(probe ownershipEntry)
probe ownershipEvidence
test "$result" = true
sleep .4
test "$(probe ownershipRestored)" = true
echo 'PASS: shared Card Line entry owns every origin without Active visits or entry sizing; visits and release retain original geometry'
test "$(probe ownershipTransferPrepare)" = true
sleep .4
result=$(probe ownershipTransfer)
probe ownershipEvidence
test "$result" = true
sleep .4
test "$(probe ownershipRestored)" = true
echo 'PASS: rejected adoption retains monitor origin; committed adoption and release retain tablet ordinary geometry'
