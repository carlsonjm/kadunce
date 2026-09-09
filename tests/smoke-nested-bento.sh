#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
smoke_root="$(mktemp -d /tmp/kadunce-bento-smoke.XXXXXX)"
build_dir="${smoke_root}/build-native"

cleanup() {
    if [[ -d "${smoke_root}" ]]; then
        find "${smoke_root}" -depth -delete
    fi
}
trap cleanup EXIT

cmake -S "${project_dir}/native" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
cmake --build "${build_dir}" -j2 >/dev/null

mkdir -p "${smoke_root}/runtime" "${smoke_root}/config"
chmod 700 "${smoke_root}/runtime"
XDG_CONFIG_HOME="${smoke_root}/config" kwriteconfig6 \
    --file kwinrc --group Plugins \
    --key kwin4_effect_kadunceEnabled true

timeout 25s env \
    XDG_RUNTIME_DIR="${smoke_root}/runtime" \
    XDG_CONFIG_HOME="${smoke_root}/config" \
    QT_PLUGIN_PATH="${build_dir}/bin" \
    KADUNCE_NATIVE_BUILD="${build_dir}" \
    KWIN_COMPOSE=O2 \
    dbus-run-session -- kwin_wayland \
        --virtual --width 1280 --height 800 --output-count 2 \
        --no-lockscreen --no-global-shortcuts --no-kactivities \
        --exit-with-session "${project_dir}/tests/nested-bento-session.sh"

echo "Nested two-output Bento activation and restore checks passed"
