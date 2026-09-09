#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
control_dir="${project_dir}/control"
source_file="${control_dir}/src/main.cpp"
service_file="${control_dir}/kadunce-control.service"
desktop_file="${control_dir}/studio.warbler.Kadunce.Control.desktop"
build_dir="$(mktemp -d /tmp/kadunce-control-test.XXXXXX)"
runtime_dir="$(mktemp -d /tmp/kadunce-control-runtime.XXXXXX)"

cleanup() {
    find "${build_dir}" -depth -delete
    find "${runtime_dir}" -depth -delete
}
trap cleanup EXIT
chmod 700 "${runtime_dir}"

python3 - <<'PY' "${control_dir}/assets/kadunce-enabled.svg" \
    "${control_dir}/assets/kadunce-disabled.svg"
import sys
import xml.etree.ElementTree as ET
for path in sys.argv[1:]:
    ET.parse(path)
PY

rg -q 'KStatusNotifierItem::SystemServices' "${source_file}"
rg -q 'KStatusNotifierItem::Active' "${source_file}"
rg -q 'setIsMenu\(true\)' "${source_file}"
rg -q 'Kadunce enabled' "${source_file}"
rg -q 'm_toggle->setCheckable\(true\)' "${source_file}"
rg -q 'm_toggle->setChecked\(enabled\)' "${source_file}"
rg -q 'setDesktopFileName' "${source_file}"
rg -q 'studio\.warbler\.Kadunce\.Control' "${source_file}" "${desktop_file}"
rg -q 'Settings…' "${source_file}"
rg -q 'org\.kde\.kwin\.Effects\.%(1|2)' "${source_file}"
rg -q 'unloadEffect' "${source_file}"
rg -q 'loadEffect' "${source_file}"
rg -q 'writeEffectEnabled\(true\)' "${source_file}"
rg -q 'writeEffectEnabled\(false\)' "${source_file}"
rg -q 'Restart=on-failure' "${service_file}"
rg -q 'WantedBy=default\.target' "${service_file}"

cmake -S "${control_dir}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "${build_dir}" -j2 >/dev/null
test -x "${build_dir}/bin/kadunce-control"

status=0
timeout 2 env QT_QPA_PLATFORM=offscreen \
    XDG_RUNTIME_DIR="${runtime_dir}" \
    "${build_dir}/bin/kadunce-control" || status=$?
test "${status}" -eq 124

echo "Kadunce tray-control package checks passed"
