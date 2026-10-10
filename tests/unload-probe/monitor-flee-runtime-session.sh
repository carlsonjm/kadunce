#!/usr/bin/env bash
# A new window admitted as the Active card that KWin then places on the
# monitor and maximizes there, as a screenshot tool restoring its last place
# does, takes only its own card with it. The cards already on the tablet stay.
#
# Needs the tablet fixture, so that a card display exists beside the monitor.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: monitor flee: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
cardCount() { kad workspaceContext | jq '[.applications[] | select(.hasCard)] | length'; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, apps: [.applications[] | {title, output, hasCard, minimized}]}')"
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
probe pointer 600 400
client titledCompanion 'Kept Card' 440 500
sleep 1.2
cards=$(cardCount)
report before
check 'the tablet holds two cards' test "$cards" -ge 2

for order in move-first maximize-first; do
    title="Flee $order"
    probe pointer 600 400
    client titledCompanion "$title" 440 500
    sleep .2
    if [[ $order == move-first ]]; then
        probe sendCaptionToOutput "$title" Virtual-1
        probe maximizeCaption "$title"
    else
        probe maximizeCaption "$title"
        probe sendCaptionToOutput "$title" Virtual-1
    fi
    sleep 1.5
    report "$order"
    check "$order: the window stands on the monitor" \
        context ".applications[] | select(.title == \"$title\") | .output == \"Virtual-1\""
    check "$order: the window has no card" \
        context ".applications[] | select(.title == \"$title\") | (.hasCard | not)"
    check "$order: the tablet keeps its cards" test "$(cardCount)" -eq "$cards"
    check "$order: the cards are still shown" \
        context '.cardStage.presentation == "active" or .cardStage.presentation == "spread"'
done

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .3
if ((failures)); then echo "FAIL: monitor flee: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a new card KWin places on the monitor takes only its own card with it'
