#!/usr/bin/env bash
# The first carry of an ordinary window. A window elsewhere closing while the
# carry waits for the edge changes nothing the carry depends on, so the edge
# must still take it, and a tooltip closing after the edge took it must not
# refuse the drop. Physically this is a notification or a tooltip going away
# during the first drag after sign-in.
set -euo pipefail
trap 'echo "FAIL: first carry line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe contactObserve
probe contactStart
for kind in pointer touch; do
    client ordinaryCompanion
    client tooltip
    sleep .5
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    probe contactPrepareDecoration
    probe contactFocus
    sleep .2
    probe pointer 500 350
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 62 500 350; fi
    sleep .15
    if [[ $kind == pointer ]]; then probe contactMotion 650 400; else probe motion 62 650 400; fi
    sleep .15
    kad nativeMoveTrace | jq -se 'last.event == "awaiting-entry"'
    client closeCompanion 'Ordinary neighbor probe'
    sleep .3
    if [[ $kind == pointer ]]; then probe contactMotion 5 400; else probe motion 62 5 400; fi
    sleep .15
    kad nativeCarryState | jq -e '.carrying and .inputBusy and .destinationPreview'
    client closeTooltip
    sleep .3
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 62; fi
    sleep .2
    kad nativeMoveTrace | jq -c '{event}'
    kad nativeMoveTrace | jq -se 'any(.[]; .event == "drop-committed")'
    kad workspaceContext | jq -e '[.displayContext.displays[]|select(.name=="Virtual-0" and .bentoActive)]|length==1'
    echo "PASS: $kind: a window closing elsewhere leaves the first carry to the edge"
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .2
done
