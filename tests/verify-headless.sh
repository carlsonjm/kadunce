#!/usr/bin/env bash
# Build and run the native domain tests that do not link KWin.
#
# Requires only a C++20 compiler and Qt6Core, so it runs on a machine or
# container without the Plasma development stack. Targets are discovered from
# native/CMakeLists.txt, so a new non-KWin test is covered without editing this
# script.
#
# This is a fast pre-check for card membership, layout, motion and carry value
# logic. It is not a substitute for the full CMake/CTest suite, the KWin-linked
# tests, package and control checks, or physical review, and its Qt version may
# differ from the release target.
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
native="${project_root}/native"

command -v g++ >/dev/null 2>&1 || { echo "verify-headless: no C++ compiler (g++)" >&2; exit 2; }
command -v python3 >/dev/null 2>&1 || { echo "verify-headless: no python3" >&2; exit 2; }
pkg-config --exists Qt6Core 2>/dev/null || { echo "verify-headless: Qt6Core not found by pkg-config" >&2; exit 2; }

cflags="$(pkg-config --cflags Qt6Core)"
libs="$(pkg-config --libs Qt6Core)"
build="$(mktemp -d)"
trap 'rm -rf "${build}"' EXIT

targets="$(python3 - "${native}/CMakeLists.txt" <<'PY'
import re
import sys

body = open(sys.argv[1]).read()
body = body[body.index("if(BUILD_TESTING)"):]
for match in re.finditer(r"add_executable\(\s*([\w-]+)\s+([^)]*)\)", body):
    tail = body[match.end():]
    following = tail.find("add_executable")
    if "KWin::kwin" in (tail if following < 0 else tail[:following]):
        continue
    print(match.group(1), *match.group(2).split())
PY
)"

[ -n "${targets}" ] || { echo "verify-headless: no targets discovered" >&2; exit 2; }

cd "${native}"
passed=0
failed=0
while read -r name sources; do
    [ -n "${name}" ] || continue
    # -UNDEBUG keeps assert() live, matching the CMake test configuration.
    if ! g++ -std=c++20 -UNDEBUG -Isrc ${cflags} -fPIC \
            -o "${build}/${name}" ${sources} ${libs} >"${build}/${name}.log" 2>&1; then
        echo "BUILD  ${name}"
        sed -n '1,5p' "${build}/${name}.log" | sed 's/^/         /'
        failed=$((failed + 1))
        continue
    fi
    if "${build}/${name}" >"${build}/${name}.out" 2>&1; then
        echo "ok     ${name}"
        passed=$((passed + 1))
    else
        echo "FAIL   ${name}"
        sed -n '1,5p' "${build}/${name}.out" | sed 's/^/         /'
        failed=$((failed + 1))
    fi
done <<< "${targets}"

echo
if [ "${failed}" -ne 0 ]; then
    echo "Headless domain checks failed: ${passed} passed, ${failed} failed."
    exit 1
fi
echo "Headless domain checks passed: ${passed} tests. KWin-linked tests, package, control and physical review are still required."
