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
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded kadunce_unload_probe | rg -qx true
"$KADUNCE_UNLOAD_PROBE_BUILD/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
for attempt in {1..40}; do
    if qdbus6 studio.warbler.UnloadClient /Client state >/dev/null 2>&1; then break; fi
    sleep .1
done
sleep .5
if [[ ${KADUNCE_RESTORE_NATIVE_CONTROL:-0} == 1 ]]; then
    test "$(probe nativeRestoreControlPrepare)" = true
    sleep .3
    together=true
    if [[ ${KADUNCE_RESTORE_SEPARATE_MINIMIZE:-0} == 1 ]]; then together=false; fi
    test "$(probe nativeRestoreControlApply "$together")" = true
    if [[ $together == false ]]; then
        sleep .3
        test "$(probe nativeRestoreControlMinimize)" = true
    fi
    sleep .3
    probe sourceEvidence
    test "$(probe nativeRestoreControlShow)" = true
    sleep .3
    probe sourceEvidence
    test "$(probe sourceVisibleRestored)" = true
    echo 'PASS: plain KWin maximize/minimize restoration without Kadunce controllers'
    exit 0
fi
test "$(probe bentoInterrupt)" = true
sleep .6
test "$(probe bentoRestored)" = true
test "$(probe bentoFresh)" = true
test "$(probe bentoRestoreReentry)" = true
test "$(probe bentoFresh)" = true
echo 'PASS: admission attempted synchronously during restoration is rejected; fresh admission afterward works'
echo 'PASS: real KWin synchronous Bento restore during placement, delayed geometry/minimized restoration, fresh activation'
# A pane whose client keeps contesting its rect now leaves for card ownership
# rather than returning to the desktop; settle-runtime covers that rule.
test "$(probe bentoBeginStableGeometry)" = true
sleep .8
test "$(probe bentoStableGeometry)" = true
echo 'PASS: normal geometry remains managed beyond the failure-recovery deadline'
test "$(probe sourcePrepare true)" = true
sleep .3
test "$(probe sourceRestoreWithoutMove)" = true
sleep .3
probe sourceEvidence
test "$(probe sourceRestored)" = true
sleep .3
probe sourceEvidence
test "$(probe sourceVisibleRestored)" = true
echo 'PASS: minimized/maximized Bento restores without native takeover'
for bento_source in false true; do
    for interrupt in false true; do
        sleep .3
        test "$(probe sourcePrepare "$bento_source")" = true
        sleep .3
        test "$(probe sourceBeginMove)" = true
        sleep .1
        adopted=$(probe sourceAdopt "$interrupt")
        probe sourceEvidence
        test "$adopted" = true
        sleep .3
        probe sourceEvidence
        restored=$(probe sourceRestored)
        echo "SOURCE_RESTORED $restored"
        test "$restored" = true
        sleep .3
        probe sourceEvidence
        test "$(probe sourceVisibleRestored)" = true
    done
done
echo 'PASS: Active/Bento authoritative source records survive native takeover, reject foreign/stale reservations, and restore after cancel/interruption'
for reason in 1 2 3; do
    test "$(probe sourcePrepare true)" = true
    sleep .3
    test "$(probe sourceCancelPendingRestore "$reason")" = true
    sleep .4
    test "$(probe sourceCanceledRestoreVisible)" = true
done
sleep 2.1
test "$(probe sourceCanceledRestoreVisible)" = true
echo 'PASS: pending minimization cancels for explicit shutdown, output retirement and native interaction; no late hide'
test "$(probe beginOwnedRestore)" = true
sleep .3
test "$(probe destroyOwnedRestore)" = true
sleep .4
test "$(probe sourceCanceledRestoreVisible)" = true
test "$(probe sourceVisibleRestored)" = true
echo 'PASS: destroying restoration owner cancels pending callbacks and preserves restored visible geometry'
test "$(probe setSourceFullScreen true)" = true
sleep .3
test "$(probe sourcePrepare true)" = true
sleep .3
test "$(probe sourceRestoreWithoutMove)" = true
sleep .4
test "$(probe sourceRestored)" = true
sleep .3
test "$(probe sourceVisibleRestored)" = true
test "$(probe setSourceFullScreen false)" = true
sleep .3
echo 'PASS: fullscreen plus minimized restoration preserves original state and visible geometry'
test "$(probe bentoOutputLostDuringRestore)" = true
test "$(probe bentoFresh)" = true
echo 'PASS: output-removal notification during restoration moves the saved state to a surviving output'
qdbus6 studio.warbler.UnloadClient /Client companion
sleep .3
test "$(probe cardAdmissionOrdering)" = true
echo 'PASS: card destination preparation, source rejection, publish-before-release and Bento/native destination priority'
tablet_admitted=$(probe tabletAdmissionOrdering)
probe tabletAdmissionEvidence
test "$tablet_admitted" = true
echo 'PASS: Bento sender retains rejected/stale tablet admissions; accepted source commit is one-shot before placement (test receiver)'
test "$(probe prepareProductionTablet)" = true
sleep .3 # Allow the companion's native configure before tablet discovery.
production_admitted=$(probe productionTabletAdmission)
probe tabletAdmissionEvidence
test "$production_admitted" = true
sleep .3
test "$(probe productionTabletPlaced)" = true
echo 'PASS: production Bento and Card Stage controllers publish tablet membership before native placement'
# Batch edge admission was suspended from Block 3 on 20 September; the tablet's
# shortcut names two windows instead of sweeping an output.
