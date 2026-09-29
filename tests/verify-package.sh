#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$(mktemp -d /tmp/kadunce-native-build.XXXXXX)"
stage_dir="$(mktemp -d /tmp/kadunce-native-stage.XXXXXX)"

cleanup() {
    # Preserve the checks' own result: this trap runs on every exit, so a
    # cleanup hiccup must not turn a passed run into a reported failure, and a
    # racing delete of a temporary tree is not a packaging defect.
    local status=$?
    for directory in "${build_dir}" "${stage_dir}"; do
        if [[ -d "${directory}" ]]; then
            find "${directory}" -depth -delete 2>/dev/null || true
        fi
    done
    return "${status}"
}
trap cleanup EXIT

cmake -S "${project_dir}/native" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
cmake --build "${build_dir}" -j2 >/dev/null
ctest --test-dir "${build_dir}" --output-on-failure >/dev/null
DESTDIR="${stage_dir}" cmake --install "${build_dir}" --prefix /usr \
    >/dev/null

plugin="${stage_dir}/usr/lib/qt6/plugins/kwin/effects/plugins/kwin4_effect_kadunce.so"
test -f "${plugin}"
# Match against captured output, not through a pipe. `rg -q` exits on its first
# match and closes the pipe, so under `pipefail` the producer can lose a
# SIGPIPE race and fail a check that actually passed.
plugin_kind="$(file "${plugin}")"
rg -q 'shared object' <<<"${plugin_kind}"
plugin_links="$(ldd "${plugin}")"
rg -q 'libkwin\.so' <<<"${plugin_links}"
if rg -q 'not found' <<<"${plugin_links}"; then
    echo "Native Scene Gate has unresolved runtime libraries" >&2
    exit 1
fi

for script in install.sh disable.sh uninstall.sh; do
    test -x "${project_dir}/${script}"
    bash -n "${project_dir}/${script}"
done
rg -q 'tests/verify-source\.sh' "${project_dir}/install.sh"
rg -q 'tests/verify-package\.sh' "${project_dir}/install.sh"
rg -q 'mktemp -d /tmp/kadunce-install' "${project_dir}/install.sh"
rg -q 'pkexec /usr/bin/install -Dm755' "${project_dir}/install.sh"
rg -q 'pkexec /usr/bin/rm -f' "${project_dir}/uninstall.sh"
test ! -d "${project_dir}/search"
test ! -e "${project_dir}/update-presentation.sh"
test ! -e "${project_dir}/rollback-presentation.sh"

echo "Kadunce native package checks passed"
