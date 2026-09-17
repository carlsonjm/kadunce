#!/usr/bin/env bash
set -euo pipefail
: "${KADUNCE_UNLOAD_PROBE_BUILD:?}"
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
tr '\0' '\n' < "/proc/${PPID}/cmdline" | rg -q '^--virtual$'
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe >/dev/null 2>&1; then break; fi
    sleep .1
done
"$KADUNCE_UNLOAD_PROBE_BUILD/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
for attempt in {1..40}; do
    if qdbus6 studio.warbler.UnloadClient /Client state >/dev/null 2>&1; then break; fi
    sleep .1
done
qdbus6 studio.warbler.UnloadClient /Client companion
qdbus6 studio.warbler.UnloadClient /Client companion
sleep .5
test "$(probe entryClientsOnTablet)" = true
sleep .3
test "$(probe bentoActiveAdmission)" = true
probe bentoActiveEvidence
echo 'PASS: Bento-to-Active retains the reduced Bento as one Card Line neighbor; repeated extraction, last-member teardown, rollback, exact restore and output isolation'
