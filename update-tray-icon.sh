#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="$(mktemp -d /tmp/kadunce-control-update.XXXXXX)"
binary_target="${HOME}/.local/bin/kadunce-control"
binary_candidate="${binary_target}.new"

cleanup() {
    rm -rf -- "${build_dir}"
    rm -f -- "${binary_candidate}"
}
trap cleanup EXIT

echo "Verifying the tray control..."
bash "${project_dir}/tests/verify-control.sh"

echo "Building the updated outline icon..."
cmake -S "${project_dir}/control" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "${build_dir}" -j2

echo "Updating the tray helper..."
/usr/bin/install -Dm755 \
    "${build_dir}/bin/kadunce-control" \
    "${binary_candidate}"
mv -f -- "${binary_candidate}" "${binary_target}"
systemctl --user restart kadunce-control.service

echo "Done. The outline-only tray icon is live; no reboot is required."
