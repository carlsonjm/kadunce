#!/usr/bin/env bash
# Build both variants from one captured source tree. Never install either one.
set -euo pipefail
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_root=$(mktemp -d /tmp/kadunce-integrated-carry.XXXXXX)
echo "Integrated carry evidence: $test_root"
trap 'echo "FAIL: integrated carry gate; inspect $test_root" >&2' ERR
mkdir "$test_root/source"
cp -a "$project_dir/native" "$project_dir/tests" "$test_root/source/"
# Hashes describe the exact source/test snapshot, including uncommitted files.
find "$test_root/source" -type f -print0 | sort -z | xargs -0 sha256sum >"$test_root/source.sha256"
git -C "$project_dir" rev-parse HEAD >"$test_root/base-commit"
cmake -S "$test_root/source/native" -B "$test_root/production" -DBUILD_TESTING=ON >"$test_root/build.log" 2>&1
cmake --build "$test_root/production" -j2 >>"$test_root/build.log" 2>&1
ctest --test-dir "$test_root/production" --output-on-failure | tee "$test_root/ctest.log"
cp -a "$test_root/source/native" "$test_root/tablet-source"
# Apply the explicit one-line fixture patch only to the disposable copy.
mkdir "$test_root/fixture"
mv "$test_root/tablet-source" "$test_root/fixture/native"
patch --batch --fuzz=0 -d "$test_root/fixture" -p1 <"$test_root/source/tests/virtual-tablet.patch" >"$test_root/fixture.log"
cmake -S "$test_root/fixture/native" -B "$test_root/tablet" -DBUILD_TESTING=OFF >>"$test_root/fixture.log" 2>&1
cmake --build "$test_root/tablet" -j2 >>"$test_root/fixture.log" 2>&1
for session in native-entry-runtime first-carry-runtime x11-native-entry-runtime runtime local-runtime desktop-runtime x11-runtime x11-baseline-runtime x11-client-runtime x11-action-runtime x11-exit-runtime tablet-runtime line-runtime sleeping-pane-runtime settle-runtime stack-runtime keyboard-runtime keyboard-focus-runtime keyboard-search-runtime start-cards-runtime dialog-runtime dialog-waiting-runtime desktop-switch-runtime desktop-switch-bento-runtime exit-runtime tablet-desktop-runtime trace-runtime no-touch-runtime desktop-bezel-runtime; do
    candidate="$test_root/production"
    if [[ $session == tablet-runtime || $session == line-runtime || $session == sleeping-pane-runtime || $session == settle-runtime || $session == stack-runtime || $session == keyboard-runtime || $session == keyboard-focus-runtime || $session == keyboard-search-runtime || $session == start-cards-runtime || $session == dialog-runtime || $session == dialog-waiting-runtime || $session == desktop-switch-runtime || $session == desktop-switch-bento-runtime || $session == tablet-desktop-runtime || $session == desktop-bezel-runtime ]]; then candidate="$test_root/tablet"; fi
    script="$session-session.sh"
    if [[ $session == tablet-desktop-runtime ]]; then script=desktop-runtime-session.sh; fi
    KADUNCE_PROBE_SESSION="$script" KADUNCE_RUNTIME_BUILD="$candidate" \
        bash "$test_root/source/tests/verify-unload-isolated.sh" | tee "$test_root/$session.log"
done
bash "$test_root/source/tests/verify-bento-candidate.sh" "$test_root/production" | tee "$test_root/bento.log"
bash "$project_dir/tests/verify-source.sh" | tee "$test_root/source-guards.log"
bash "$project_dir/tests/verify-control.sh" | tee "$test_root/control.log"
# Read-only session registration check, never toggles the user's effect.
bash "$project_dir/tests/verify-live-control.sh" | tee "$test_root/live-control.log"
echo 'PASS: fresh-source integrated carry gate; physical appearance and unsupported routes remain separate'
