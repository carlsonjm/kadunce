#!/usr/bin/env bash
# CARD-LIFECYCLE.md §10 on the display that can own cards: the Active card,
# carried by its title bar to the bottom edge, shows where it will land and,
# let go, returns to the ordinary desktop there, by pointer and by touch. Alone,
# it ends the display's session. Beside a neighbour, §2's desktop is shown with
# it and the neighbour is held aside, out of sight and out of reach: Spread
# brings it back and going back returns to the desktop, choosing it shows it
# Active, asking for the returned window shows the desktop again, and switching
# Kadunce off leaves nothing hidden. A Bento pane let go there beside another
# returns to the desktop the same way, the rest held aside in Spread.
#
# Needs the tablet fixture: only a display that can own cards holds cards.
set -euo pipefail
trap 'echo "FAIL: card exit runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
shots=$(dirname "$XDG_RUNTIME_DIR")
red() { read -r r g b <<<"$1"; (( r > 150 && g < 100 && b < 100 )); }
light() { read -r r g b <<<"$1"; (( r + g + b > 600 )); }
# The mean red, green and blue of a small square of the display.
colour() {
    python3 "$(dirname "$0")/capture-png.py" "$1" "$2" 24 24 "$shots/$3.png"
    python3 -c 'import sys; from PIL import Image; im = Image.open(sys.argv[1]).convert("RGB"); px = list(im.getdata()); print(*(sum(p[i] for p in px) // len(px) for i in range(3)))' "$shots/$3.png"
}
touch_id=80
for scenario in alone neighbour pane; do
for kind in pointer touch; do
    touch_id=$((touch_id + 1))
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    trap 'kill "$client_pid" 2>/dev/null || true' EXIT
    sleep 1
    probe contactObserve
    test "$(probe contactStart)" = true
    neighbour=
    if [[ $scenario == neighbour || $scenario == pane ]]; then
        client colouredCompanion "Neighbour" "c03a3a" 700 500
        sleep .8
        neighbour=$(probe windowIdByCaption "Neighbour")
        [[ -n $neighbour ]]
    fi
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep 1
    main=$(kad workspaceContext | jq -r --arg n "$neighbour" '[.applications[] | select(.windowId != $n)][0].windowId')
    probe contactFocus
    sleep .5
    kad workspaceContext | jq -e --arg id "$main" '.cardStage.active and .cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
    if [[ $scenario == pane ]]; then
        # The Active card and its neighbour become a Bento pair.
        test "$(kad toggleBentoOnOutput Virtual-0)" = true
        sleep .8
        kad workspaceContext | jq -e '.desktopStage.active'
    fi
    bounds=$(probe windowGeometry "$main")
    x=$(jq '.x+.width/2|floor' <<<"$bounds"); y=$(jq '.y+40|floor' <<<"$bounds")
    probe pointer "$x" "$y"
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down "$touch_id" "$x" "$y"; fi
    sleep .15
    kad nativeCarryState | jq -e '.carrying and .inputBusy and (.detachPreview|not)'
    for step in 500 650 760 799; do
        if [[ $kind == pointer ]]; then probe contactMotion "$x" "$step"; else probe motion "$touch_id" "$x" "$step"; fi
        sleep .1
    done
    if [[ $scenario == neighbour ]]; then
        kad nativeCarryState | jq -e '.detachPreview and .placementOutline'
        target=$(kad nativeCarryState | jq -c '.destinationRect')
        if [[ $kind == pointer ]]; then probe contactButton false; else probe up "$touch_id"; fi
        sleep .8
        kad nativeMoveTrace | jq -se 'any(.[]; .event == "destination-exit:Virtual-0") and any(.[]; .event == "drop-committed")'
        probe windowGeometry "$main" | jq -e --argjson target "$target" '. == $target'
        kad workspaceContext | jq -e '.cardStage.active and .cardStage.presentation == "desktop"'
        cx=$(jq '.x+.width/2|floor' <<<"$target"); cy=$(jq '.y+.height/2|floor' <<<"$target")
        seen() {
            test "$(probe windowAt "$cx" "$cy" | jq -r .target.class)" = unload-client
            light "$(colour "$cx" "$cy" "returned-$kind-$1")"
            # Elsewhere the desktop: no card is drawn there, and none takes a touch.
            test "$(probe windowAt 40 40 | jq -r '.target.caption // ""')" != Neighbour
            if red "$(colour 40 40 "desktop-$kind-$1")"; then echo "a card shows over the desktop" >&2; exit 1; fi
        }
        seen released
        # Spread brings the neighbour back; going back returns to the desktop.
        kad showCardLine
        sleep .8
        kad workspaceContext | jq -e '.cardStage.presentation == "cardLine"'
        red "$(colour 628 388 "spread-$kind")"
        probe key 1 0
        sleep .8
        kad workspaceContext | jq -e '.cardStage.presentation == "desktop"'
        seen back
        # Chosen in Spread, the neighbour is shown Active.
        kad showCardLine
        sleep .8
        probe key 28 0
        sleep .8
        kad workspaceContext | jq -e --arg id "$neighbour" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
        red "$(colour 40 40 "active-$kind")"
        # Asking for the returned window, as the dock does, shows the desktop.
        probe contactFocus
        sleep .8
        kad workspaceContext | jq -e '.cardStage.presentation == "desktop"'
        seen asked
        # Switched off, every window is back, the neighbour taking touches again.
        test "$(probe releaseRuntime)" = true
        sleep .5
        g=$(probe windowGeometry "$neighbour")
        test "$(probe windowAt "$(jq '.x+.width/2|floor' <<<"$g")" "$(jq '.y+.height/2|floor' <<<"$g")" | jq -r .target.caption)" = Neighbour
        echo "PASS: $kind neighbour: let go at the bottom edge, the card returns to the shown desktop, its neighbour held aside until Spread or a choice brings it back"
    elif [[ $scenario == pane ]]; then
        kad nativeCarryState | jq -e '.detachPreview'
        target=$(kad nativeCarryState | jq -c '.destinationRect')
        if [[ $kind == pointer ]]; then probe contactButton false; else probe up "$touch_id"; fi
        sleep 1
        probe windowGeometry "$main" | jq -e --argjson target "$target" '. == $target'
        kad workspaceContext | jq -e '.cardStage.active and .cardStage.presentation == "desktop" and (.desktopStage.active|not)'
        cx=$(jq '.x+.width/2|floor' <<<"$target"); cy=$(jq '.y+.height/2|floor' <<<"$target")
        test "$(probe windowAt "$cx" "$cy" | jq -r .target.class)" = unload-client
        light "$(colour "$cx" "$cy" "pane-returned-$kind")"
        test "$(probe windowAt 40 40 | jq -r '.target.caption // ""')" != Neighbour
        if red "$(colour 40 40 "pane-desktop-$kind")"; then echo "a pane shows over the desktop" >&2; exit 1; fi
        # Spread brings back what the layout left; going back returns here.
        kad showCardLine
        sleep .8
        kad workspaceContext | jq -e '.cardStage.presentation == "cardLine"'
        probe key 1 0
        sleep .8
        kad workspaceContext | jq -e '.cardStage.presentation == "desktop"'
        light "$(colour "$cx" "$cy" "pane-back-$kind")"
        test "$(probe releaseRuntime)" = true
        sleep .5
        g=$(probe windowGeometry "$neighbour")
        test "$(probe windowAt "$(jq '.x+.width/2|floor' <<<"$g")" "$(jq '.y+.height/2|floor' <<<"$g")" | jq -r .target.caption)" = Neighbour
        echo "PASS: $kind pane: a Bento pane let go at the bottom edge returns to the shown desktop, the rest held aside in Spread"
    else
        # The window's landing shows, labelled, above the dock.
        kad nativeCarryState | jq -e '.detachPreview and .placementOutline and (.destinationRect.y + .destinationRect.height <= 790)'
        target=$(kad nativeCarryState | jq -c '.destinationRect')
        if [[ $kind == pointer ]]; then probe contactButton false; else probe up "$touch_id"; fi
        sleep .6
        kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not) and (.detachPreview|not)'
        kad nativeMoveTrace | jq -se 'any(.[]; .event == "destination-exit:Virtual-0") and any(.[]; .event == "drop-committed")'
        probe windowGeometry "$main" | jq -e --argjson target "$target" '. == $target'
        kad workspaceContext | jq -e '(.cardStage.active|not)'
        # Drawn where it was let go, over the dark desktop, and taking input there.
        cx=$(jq '.x+.width/2|floor' <<<"$target"); cy=$(jq '.y+.height/2|floor' <<<"$target")
        test "$(probe windowAt "$cx" "$cy" | jq -r .target.class)" = unload-client
        read -r r g b <<<"$(colour "$cx" "$cy" "released-$kind")"
        echo "released window drawn as $r $g $b"
        (( r + g + b > 300 ))
        echo "PASS: $kind alone: the display's last card let go at the bottom edge returns to the desktop where its outline showed"
    fi
    if [[ $scenario == alone ]]; then test "$(probe releaseRuntime)" = true; fi
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    kill "$client_pid"; wait "$client_pid" || true
    sleep .3
done
done
