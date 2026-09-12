#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
native_build_dir="${KADUNCE_NATIVE_BUILD:?missing native build path}"
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-*/runtime ]]
tr '\0' '\n' < "/proc/${PPID}/cmdline" | rg -q '^--virtual$'

for attempt in $(seq 1 30); do
    if qdbus6 org.kde.KWin /Effects \
            org.kde.kwin.Effects.loadEffect \
            kwin4_effect_kadunce >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done

test "$(qdbus6 org.kde.KWin /Effects \
    org.kde.kwin.Effects.isEffectLoaded \
    kwin4_effect_kadunce)" = "true"
rg -q "${native_build_dir}/bin/kwin/effects/plugins/kwin4_effect_kadunce\\.so" \
    "/proc/${PPID}/maps"

konsole --separate --title Kadunce-Bento-A >/dev/null 2>&1 &
first_client=$!
konsole --separate --title Kadunce-Bento-B >/dev/null 2>&1 &
second_client=$!
sleep 1

mapfile -t stages < <(qdbus6 org.kde.KWin /Kadunce \
    studio.warbler.Kadunce.outputStageState)
test "${#stages[@]}" -eq 2
first_output="${stages[0]%%|*}"
second_output="${stages[1]%%|*}"
test "$(qdbus6 org.kde.KWin /Kadunce \
    studio.warbler.Kadunce.toggleBentoOnOutput \
    "${first_output}")" = "true"
sleep 1

mapfile -t active_stages < <(qdbus6 org.kde.KWin /Kadunce \
    studio.warbler.Kadunce.outputStageState)
printf '%s\n' "${active_stages[@]}"
printf '%s\n' "${active_stages[@]}" | rg -q '^Virtual-0\|external\|.*\|2\|0$'

# Carry one Bento member to an ordinary destination. The source reflows from
# two panes to one and the destination remains an ordinary, independent stage.
test "$(qdbus6 org.kde.KWin /Kadunce \
    studio.warbler.Kadunce.handoffBentoLeadToOutput \
    "${first_output}" "${second_output}")" = "true"
sleep .7 # Source reflow must survive the reconcile-once observation.
mapfile -t first_handoff < <(qdbus6 org.kde.KWin /Kadunce \
    studio.warbler.Kadunce.outputStageState)
printf '%s\n' "${first_handoff[@]}" | rg -q '^Virtual-0\|external\|.*\|1\|0$'
printf '%s\n' "${first_handoff[@]}" | rg -q '^Virtual-1\|external\|.*\|0\|0$'

# Activate the destination around the transferred window, then carry the
# remaining source member into that live Bento transaction.
test "$(qdbus6 org.kde.KWin /Kadunce \
    studio.warbler.Kadunce.toggleBentoOnOutput \
    "${second_output}")" = "true"
test "$(qdbus6 org.kde.KWin /Kadunce \
    studio.warbler.Kadunce.handoffBentoLeadToOutput \
    "${first_output}" "${second_output}")" = "true"
mapfile -t second_handoff < <(qdbus6 org.kde.KWin /Kadunce \
    studio.warbler.Kadunce.outputStageState)
printf '%s\n' "${second_handoff[@]}"
printf '%s\n' "${second_handoff[@]}" | rg -q '^Virtual-0\|external\|.*\|0\|0$'
printf '%s\n' "${second_handoff[@]}" | rg -q '^Virtual-1\|external\|.*\|2\|0$'

test "$(qdbus6 org.kde.KWin /Kadunce \
    studio.warbler.Kadunce.toggleBentoOnOutput \
    "${second_output}")" = "true"
mapfile -t restored_stages < <(qdbus6 org.kde.KWin /Kadunce \
    studio.warbler.Kadunce.outputStageState)
printf '%s\n' "${restored_stages[@]}" | rg -q '\|0\|0$'

kill "${first_client}" "${second_client}" >/dev/null 2>&1 || true
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect \
    kwin4_effect_kadunce
