#!/usr/bin/env bash
# Private compositor only: verify diagnostic lifecycle, bounded storage and reset.
set -euo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep .5
probe contactObserve
test "$(probe contactStart)" = true
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
for attempt in {1..12}; do
    probe contactPrepareDecoration
    sleep .1
    probe contactFocus
    probe pointer 500 350
    client armMove
    probe contactButton true
    sleep .1
    for x in 510 520 530 540; do probe contactMotion "$x" 360; done
    probe contactButton false
    # Keep repeated test clicks outside Qt's double-click interval so all
    # twelve iterations actually initiate a move and exercise the 48-entry cap.
    sleep .55
done
kad nativeMoveTrace
kad nativeMoveTrace | jq -se '
    length == 48 and
    any(.[]; .event == "staged-ordinary") and
    any(.[]; .event == "proof-app-pointer") and
    any(.[]; .event == "awaiting-entry") and
    any(.[]; .event == "native-finished") and
    all(.[]; .event != "adopted" and .event != "drop-committed") and
    all(.[]; (has("title")|not) and (has("coordinates")|not))'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
test -z "$(kad nativeMoveTrace)"
echo 'PASS: bounded transition history, deferred native lifecycle and unload reset'
