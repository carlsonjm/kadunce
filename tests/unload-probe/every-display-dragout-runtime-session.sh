#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11, Every display, as a tablet with a monitor is used:
# Virtual-0 at scale 2 is small, sized Solo, primary and holds no window;
# Virtual-1 is large and sized Connected, with three windows. Every window is
# let go by Meta+Esc, and Meta+B on the monitor lays its three out. A pane
# carried by KWin's own title bar to the small display is the card shown
# there, and the two it leaves stay a layout. From then on a click lands where
# it is made: on a monitor pane, on the small display's card, and on the
# monitor again. Every check is reported, so a failure does not hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: every display dragout: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation,
        displays: [.displayContext.displays[] | {name, primary, sizing, holdsCards, shows,
            s: .cardStage.presentation, a: .cardStage.activeCardId, n: .cardStage.cardCount}],
        apps: [.applications[] | {title, windowId, output, hasCard, pane, stage, focused, minimized}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client" and .normal) | {caption, output, x, y, width, height, hidden})')"
    echo "stages $1 $(kad outputStageState | tr '\n' ' ')"
}
# What a display's own card stage presents.
stage_is() { context --arg d "$1" --arg p "$2" 'first(.displayContext.displays[] | select(.name == $d)) | .cardStage.presentation == $p'; }
# No display's stage shows a Spread nobody opened.
no_spread() { context '[.displayContext.displays[] | select(.cardStage.presentation == "cardLine")] | length == 0'; }
shown_on() { context --arg id "$1" --arg d "$2" '(first(.displayContext.displays[] | select(.name == $d)) | .cardStage
    | .presentation == "active" and .activeCardId == $id)
    and (first(.applications[] | select(.windowId == $id)) | .output == $d and (.minimized | not))'; }
monitor_pane() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id))
    | .pane and .output == "Virtual-1" and (.hasCard | not) and (.minimized | not)'; }
monitor_panes() { context --argjson n "$1" '[.applications[] | select(.pane and .output == "Virtual-1")] | length == $n'; }
focused() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .focused'; }
title_at() { probe titleBarOf "$1" | jq -r 'select(.x != null) | [.x, .y] | map(floor) | @tsv'; }
# Inside a window, below its title bar, where a person clicks to use it.
body_at() { probe windowGeometry "$1" | jq -r '[.x + .width / 2, .y + .height * 0.6] | map(floor) | @tsv'; }
# A person's mouse clicks once, where it stands.
click_at() {
    probe contactMotion "$1" "$2" >/dev/null
    sleep .15
    probe contactButton true >/dev/null
    sleep .08
    probe contactButton false >/dev/null
    sleep 1
}
# A person's mouse presses a window's title bar, KWin's own, and carries it
# through each point given in turn before letting go, quickly, as a flick
# across to the other display is.
title_drag() {
    local id=$1 x y; shift
    read -r x y <<<"$(title_at "$id")"
    [[ -n ${y:-} ]] || { echo "no title bar on $id"; return 0; }
    probe pointer "$x" "$y" >/dev/null
    sleep .1
    probe contactButton true >/dev/null
    sleep .1
    probe contactMotion "$((x - 30))" "$((y + 20))" >/dev/null
    sleep .08
    while (( $# >= 2 )); do
        probe contactMotion "$1" "$2" >/dev/null
        shift 2
        sleep .06
    done
    sleep .2
    echo "state carry-$id $(kad nativeCarryState | jq -c '{carrying, lineCarrying, destinationRect}')"
    echo "state trace-$id $(kad nativeMoveTrace | jq -Rsc 'split("\n") | map(select(length > 0)) | .[-6:]')"
    probe contactButton false >/dev/null
    sleep 1.6
}
for _ in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
kscreen-doctor output.Virtual-0.scale.2 >/dev/null 2>&1
sleep 1
echo "displays: $(kscreen-doctor -o 2>/dev/null | sed 's/\x1b\[[0-9;]*m//g' | grep -E 'Output|Geometry|Scale' | tr -s ' \n' ' ')"
kwriteconfig6 --file kaduncerc --group Spread --key Display Virtual-0
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
for title in 'Mon A' 'Mon B' 'Mon C'; do client titledCompanion "$title" 700 500; sleep .5; done
for title in 'Mon A' 'Mon B' 'Mon C'; do probe sendCaptionToOutput "$title" Virtual-1; done
sleep .5
probe minimizeWindow "$(idOf unload-client)" true >/dev/null
sleep .4
ma=$(idOf 'Mon A') mb=$(idOf 'Mon B') mc=$(idOf 'Mon C')
for id in "$ma" "$mb" "$mc"; do probe serverDecorate "$id" >/dev/null; done
sleep .4
probe pointer 2560 720 >/dev/null
probe activateWindowId "$ma" >/dev/null
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.8
report loaded

# Meta+Esc lets every window go; Meta+B on the monitor, with no layout there,
# lays its three windows out.
probe key 1 125 >/dev/null
sleep 1.4
report released
probe pointer 2560 720 >/dev/null
probe activateWindowId "$ma" >/dev/null
sleep .6
probe key 48 125 >/dev/null
sleep 1.8
report laid-out
check 'meta+b: the monitor shows its three windows as a layout' monitor_panes 3
check 'meta+b: the small display shows no Spread' no_spread

# A pane carried by its title bar onto the small display, let go there.
title_drag "$mc" 2200 500 1600 420 900 380 640 360
report carried
check 'carry: the carried pane is the card the small display shows' shown_on "$mc" Virtual-0
check 'carry: the two it left stay a layout on the monitor' monitor_panes 2
check 'carry: no display shows a Spread' no_spread
check 'carry: the monitor still presents its layout' bash -c "qdbus6 org.kde.KWin /Kadunce workspaceContext | jq -e 'first(.displayContext.displays[] | select(.name == \"Virtual-1\")) | .cardStage.presentation != \"cardLine\"'"

# Clicks land where they are made.
read -r x y <<<"$(body_at "$ma")"
click_at "$x" "$y"
report click-monitor
check 'click: a click on a monitor pane gives it the keys' focused "$ma"
check 'click: the monitor pane is still a pane' monitor_pane "$ma"
check 'click: no display shows a Spread' no_spread
read -r x y <<<"$(body_at "$mc")"
click_at "$x" "$y"
report click-small
check 'click: a click on the small display'"'"'s card gives it the keys' focused "$mc"
check 'click: the small display still shows that card' shown_on "$mc" Virtual-0
check 'click: no display shows a Spread' no_spread
read -r x y <<<"$(body_at "$mb")"
click_at "$x" "$y"
report click-monitor-again
check 'click: a click on the other monitor pane gives it the keys' focused "$mb"
check 'click: the monitor still shows two panes' monitor_panes 2
check 'click: no display shows a Spread' no_spread

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
report switched-off
kill "$client_pid" 2>/dev/null || true
if (( failures > 0 )); then
    echo "FAIL: every display dragout: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: after Meta+Esc and Meta+B, a pane carried by its title bar to the small display is its card, the monitor keeps a layout of two, and every click after lands where it is made'
