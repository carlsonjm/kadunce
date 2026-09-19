#!/usr/bin/env bash
set -euo pipefail
: "${KADUNCE_UNLOAD_PROBE_BUILD:?}" "${KADUNCE_FULL_FIXTURE_BUILD:?}"
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
tr '\0' '\n' < "/proc/${PPID}/cmdline" | rg -q '^--virtual$'
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
effect() { qdbus6 org.kde.KWin /Effects "org.kde.kwin.Effects.$1" kwin4_effect_kadunce; }
empty='{"cancel":0,"press":0,"release":0,"touchDown":0,"touchUp":0}'
touch='{"cancel":0,"press":0,"release":0,"touchDown":1,"touchUp":1}'
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe >/dev/null 2>&1; then break; fi
    sleep .1
done
"$KADUNCE_UNLOAD_PROBE_BUILD/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
for attempt in {1..40}; do
    if client state >/dev/null 2>&1; then break; fi
    sleep .1
done
sleep .5
probe pointer 500 300
probe down 1 500 300; probe up 1
sleep .1
test "$(client state)" = "$touch"
client reset
for delay in .05 .45; do
    test "$(effect loadEffect)" = true
    rg -Fq "$KADUNCE_FULL_FIXTURE_BUILD/bin/kwin/effects/plugins/kwin4_effect_kadunce.so" "/proc/${PPID}/maps"
    qdbus6 org.kde.KWin /Kadunce studio.warbler.Kadunce.showCardLine
    qdbus6 org.kde.KWin /Kadunce studio.warbler.Kadunce.workspaceContext | rg -q '"presentation":"spread"'
    sleep .1
    probe down 2 500 300
    sleep "$delay"
    effect unloadEffect
    test "$(effect isEffectLoaded)" = false
    probe up 2
    sleep .1
    test "$(client state)" = "$empty"
    probe down 3 500 300; probe up 3
    sleep .1
    test "$(client state)" = "$touch"
    client reset
done
test "$(effect loadEffect)" = true
qdbus6 org.kde.KWin /Kadunce studio.warbler.Kadunce.showCardLine
probe pointer 500 300; probe button true
sleep .45
effect unloadEffect
probe button false
sleep .1
test "$(client state)" = "$empty"
probe button true; probe button false
sleep .1
test "$(client state)" = '{"cancel":0,"press":1,"release":1,"touchDown":0,"touchUp":0}'
echo 'PASS: full effect unload during pending touch, lifted touch and lifted pointer; fresh input after unload'
