#!/usr/bin/env bash
set -euo pipefail
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
candidate=$(realpath "${1:?supply an existing candidate build directory}")
test -f "$candidate/bin/kwin/effects/plugins/kwin4_effect_kadunce.so"
test_root=$(mktemp -d /tmp/kadunce-bento-candidate.XXXXXX)
echo "Isolated candidate evidence: $test_root"
mkdir -p "$test_root/runtime" "$test_root/config" "$test_root/data" "$test_root/state"
chmod 700 "$test_root/runtime"
timeout 40s env XDG_RUNTIME_DIR="$test_root/runtime" XDG_CONFIG_HOME="$test_root/config" \
    XDG_DATA_HOME="$test_root/data" XDG_STATE_HOME="$test_root/state" \
    QT_PLUGIN_PATH="$candidate/bin" KADUNCE_NATIVE_BUILD="$candidate" KWIN_COMPOSE=O2 \
    dbus-run-session -- kwin_wayland --virtual --width 1280 --height 800 --output-count 2 \
    --no-lockscreen --no-global-shortcuts --no-kactivities \
    --exit-with-session "$project_dir/tests/nested-bento-session.sh" >"$test_root/session.log" 2>&1
echo 'PASS: full candidate two-output Bento transfer, restoration and unload'
