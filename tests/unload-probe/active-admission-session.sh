#!/usr/bin/env bash
# Bento-to-Active extraction: repeated extraction, last-member teardown, rollback,
# exact restore and output isolation.
#
# Two private outputs: the tablet holds the pair being extracted, the other one
# its own layout, so §11's isolation is witnessed rather than assumed.
set -euo pipefail
trap 'echo "FAIL: Bento-to-Active line $LINENO" >&2' ERR
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
# Print the evidence before asserting, so a failure names which rule broke
# instead of stopping the script before it can be read.
admission=$(probe bentoActiveAdmission)
probe bentoActiveEvidence
test "$admission" = true
echo 'PASS: a pane carried to the top edge becomes the Active card and the pane it leaves behind becomes its Spread neighbor; repeated extraction, rollback, exact restore and other-display isolation'
