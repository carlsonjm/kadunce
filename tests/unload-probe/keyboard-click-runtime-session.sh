#!/usr/bin/env bash
# INPUT.md § Active card: the keys rise when the person asks for them, by a
# tap or a click on the Keyboard entry in the tray, which asks Kadunce for
# them. With Plasma's touch-only setting, as the tablet has it, KWin shows the
# keys only while a touch was the last thing it heard, so after a click they
# never came.
#
# Needs the tablet fixture and a real input method. Every check is reported,
# so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
vk() { qdbus6 org.kde.KWin /VirtualKeyboard org.kde.kwin.VirtualKeyboard."$@"; }
visible() { probe keyboardState | jq -e '.visible' >/dev/null; }
hidden() { ! visible; }
lower() { probe hideKeyboard; sleep 1.5; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: keyboard click: $name" >&2; failures=$((failures + 1)); fi
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
QT_IM_MODULE=wayland "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
for attempt in {1..40}; do
    [[ $(vk available 2>/dev/null) == true ]] && break
    sleep .1
done
client textCompanion
sleep 1
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
lower
echo "start $(probe keyboardState | jq -c '{visible}') $(kad workspaceContext | jq -c '.cardStage | {presentation}')"

# A click, as on the tray's entry, then the request the entry makes.
for attempt in 1 2; do
    probe pointer 640 790
    probe button true; probe button false
    sleep .3
    kad raiseKeyboard
    sleep 1.2
    echo "after a click $attempt: $(probe keyboardState | jq -c '{visible}')"
    check "after a click, asking brings the keys ($attempt)" visible
    lower
done

# A touch, as on the tray's entry, then the same request.
probe down 81 640 790
probe up 81
sleep .3
kad raiseKeyboard
sleep 1.2
echo "after a touch: $(probe keyboardState | jq -c '{visible}')"
check "after a touch, asking brings the keys" visible
lower
check "the keys go when put away" hidden

if ((failures)); then echo "FAIL: keyboard click: $failures checks failed" >&2; exit 1; fi
echo 'PASS: the keys come when asked for, after a click as after a touch'
