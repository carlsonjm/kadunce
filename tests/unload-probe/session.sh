#!/usr/bin/env bash
set -euo pipefail
: "${KADUNCE_UNLOAD_PROBE_BUILD:?isolated probe build required}"
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || { echo 'Private runtime required' >&2; exit 1; }
tr '\0' '\n' < "/proc/${PPID}/cmdline" | rg -q '^--virtual$'
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
expect_client() {
    local actual
    actual=$(client state)
    if [[ "$actual" != "$1" ]]; then
        echo "FAIL: expected $1; received $actual" >&2
        exit 1
    fi
}
empty='{"cancel":0,"press":0,"release":0,"touchDown":0,"touchUp":0}'
touch='{"cancel":0,"press":0,"release":0,"touchDown":1,"touchUp":1}'
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe >/dev/null 2>&1; then break; fi
    sleep .1
done
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded kadunce_unload_probe | rg -qx true
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
for attempt in {1..40}; do
    if client state >/dev/null 2>&1; then break; fi
    sleep .1
done
sleep .5
probe pointer 500 300
probe down 1 500 300
probe up 1
sleep .1
echo BASELINE_TOUCH
client state
expect_client "$touch"
client reset
for delay in .05 .45; do
    probe arm
    probe down 2 500 300
    sleep "$delay"
    probe state
    if [[ $delay == .05 ]]; then
        test "$(probe state)" = '{"cancels":0,"grabbed":false,"starts":0}'
    else
        test "$(probe state)" = '{"cancels":0,"grabbed":true,"starts":1}'
    fi
    probe drop
    probe state
    if [[ $delay == .45 ]]; then
        test "$(probe state)" = '{"cancels":1,"grabbed":false,"starts":1}'
    fi
    # Recreating the router before release must not claim the old contact.
    probe arm
    probe up 2
    probe drop
    sleep .1
    echo AFTER_TOUCH_DROP
    client state
    expect_client "$empty"
    probe down 3 500 300
    probe up 3
    sleep .1
    echo FRESH_TOUCH
    client state
    expect_client "$touch"
    client reset
done
probe arm
probe pointer 500 300
probe button true
sleep .45
probe state
probe drop
probe state
probe button false
sleep .1
echo AFTER_POINTER_DROP
client state
expect_client "$empty"
probe button true
probe button false
sleep .1
echo FRESH_POINTER
client state
expect_client '{"cancel":0,"press":1,"release":1,"touchDown":0,"touchUp":0}'
client reset
# Mixed sequence: the guest/client owns one contact; Kadunce owns the other.
probe arm
probe guest true
probe down 10 500 300
probe down 11 50 300
probe drop
probe up 11
sleep .1
expect_client '{"cancel":0,"press":0,"release":0,"touchDown":1,"touchUp":0}'
probe up 10
sleep .1
expect_client "$touch"
client reset
# Client pointer ownership survives filter removal as well.
probe arm
probe guest true
probe pointer 500 300
probe button true
probe drop
probe button false
sleep .1
expect_client '{"cancel":0,"press":1,"release":1,"touchDown":0,"touchUp":0}'
echo 'PASS: held touch/pointer teardown, router recreation, mixed client ownership, and fresh input'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kadunce_unload_probe
