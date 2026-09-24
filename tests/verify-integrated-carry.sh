#!/usr/bin/env bash
# Build both variants from one captured source tree. Never install either one.
#
# KADUNCE_GATE_SCENES="a b" runs only the named scenes, for iterating on one
# change; a candidate is handed over only after a run with every scene.
# KADUNCE_GATE_JOBS sets how many private compositors run at once.
set -euo pipefail
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
jobs=${KADUNCE_GATE_JOBS:-4}
test_root=$(mktemp -d /tmp/kadunce-integrated-carry.XXXXXX)
echo "Integrated carry evidence: $test_root"
trap 'echo "FAIL: integrated carry gate; inspect $test_root" >&2' ERR

# Scene, then the build it loads: the production plugin, or the disposable
# virtual-tablet fixture for scenes that need a display able to own cards.
scenes=(
    native-entry-runtime production
    first-carry-runtime production
    x11-native-entry-runtime production
    runtime production
    local-runtime production
    desktop-runtime production
    x11-runtime production
    x11-baseline-runtime production
    x11-client-runtime production
    x11-action-runtime production
    x11-exit-runtime production
    exit-runtime production
    trace-runtime production
    no-touch-runtime production
    provenance-runtime production
    active-admission production
    ownership production
    ownership-transition production
    membership-runtime production
    launch-runtime production
    side-runtime production
    contact production
    bento production
    snap production
    unload production
    tablet-runtime tablet
    line-runtime tablet
    sleeping-pane-runtime tablet
    settle-runtime tablet
    stack-runtime tablet
    keyboard-runtime tablet
    keyboard-focus-runtime tablet
    keyboard-search-runtime tablet
    start-cards-runtime tablet
    dialog-runtime tablet
    dialog-waiting-runtime tablet
    desktop-switch-runtime tablet
    desktop-switch-bento-runtime tablet
    tablet-desktop-runtime tablet
    desktop-bezel-runtime tablet
    output-unplug-runtime tablet
    lifetime-runtime tablet
    guest-drawer-runtime tablet
    x11-tablet-runtime tablet
)

selected=()
for ((i = 0; i < ${#scenes[@]}; i += 2)); do
    if [[ -z ${KADUNCE_GATE_SCENES:-} || " $KADUNCE_GATE_SCENES " == *" ${scenes[i]} "* ]]; then
        selected+=("${scenes[i]}" "${scenes[i + 1]}")
    fi
done
[[ ${#selected[@]} != 0 ]] || { echo "No scene matches KADUNCE_GATE_SCENES" >&2; exit 2; }

mkdir "$test_root/source"
cp -a "$project_dir/native" "$project_dir/tests" "$project_dir/control" "$test_root/source/"
# Hashes describe the exact source/test snapshot, including uncommitted files.
find "$test_root/source" -type f -print0 | sort -z | xargs -0 sha256sum >"$test_root/source.sha256"
git -C "$project_dir" rev-parse HEAD >"$test_root/base-commit"
cmake -S "$test_root/source/native" -B "$test_root/production" -DBUILD_TESTING=ON >"$test_root/build.log" 2>&1
cmake --build "$test_root/production" -j8 >>"$test_root/build.log" 2>&1

# The native tests share no state with the private compositors, so they run
# beside the scenes rather than ahead of them.
(
    status=0
    ctest --test-dir "$test_root/production" --output-on-failure -j4 >"$test_root/ctest.log" 2>&1 || status=$?
    echo "$status" >"$test_root/ctest.status"
) &

# The guided repair builds and tests its own snapshot with installation refused,
# independent of the compositors, so it runs beside them too.
(
    status=0
    bash "$test_root/source/tests/verify-repair.sh" >"$test_root/repair.log" 2>&1 || status=$?
    echo "$status" >"$test_root/repair.status"
) &

# Apply the explicit one-line fixture patch only to the disposable copy.
mkdir "$test_root/fixture"
cp -a "$test_root/source/native" "$test_root/fixture/native"
patch --batch --fuzz=0 -d "$test_root/fixture" -p1 <"$test_root/source/tests/virtual-tablet.patch" >"$test_root/fixture.log"
cmake -S "$test_root/fixture/native" -B "$test_root/tablet" -DBUILD_TESTING=OFF >>"$test_root/fixture.log" 2>&1
cmake --build "$test_root/tablet" -j8 >>"$test_root/fixture.log" 2>&1
# Every scene loads the same probe; build it once.
cmake -S "$test_root/source/tests/unload-probe" -B "$test_root/probe" -DBUILD_TESTING=OFF >"$test_root/probe.log" 2>&1
cmake --build "$test_root/probe" -j8 >>"$test_root/probe.log" 2>&1

run_scene() {
    local session=$1 build=$2 script="$1-session.sh"
    if [[ $session == tablet-desktop-runtime ]]; then script=desktop-runtime-session.sh; fi
    if [[ $session == unload ]]; then script=session.sh; fi
    if KADUNCE_PROBE_SESSION="$script" KADUNCE_RUNTIME_BUILD="$test_root/$build" \
        KADUNCE_PROBE_BUILD="$test_root/probe" \
        bash "$test_root/source/tests/verify-unload-isolated.sh" >"$test_root/$session.log" 2>&1; then
        echo "ok    $session"
    else
        echo "FAIL  $session  ($test_root/$session.log)"
        touch "$test_root/$session.failed"
    fi
}

# Every scene runs even after one fails, so one failure cannot hide the next.
export -f run_scene
export test_root
printf '%s\n' "${selected[@]}" | xargs -n 2 -P "$jobs" bash -c 'run_scene "$@"' _
wait

failed=0
if [[ $(cat "$test_root/ctest.status") != 0 ]]; then
    echo "FAIL  native tests  ($test_root/ctest.log)"
    failed=1
else
    tail -3 "$test_root/ctest.log"
fi
if [[ $(cat "$test_root/repair.status") != 0 ]]; then
    echo "FAIL  guided repair  ($test_root/repair.log)"
    failed=1
fi
if compgen -G "$test_root/*.failed" >/dev/null; then failed=1; fi
((failed == 0)) || { echo "FAIL: integrated carry gate; inspect $test_root" >&2; trap - ERR; exit 1; }

bash "$test_root/source/tests/verify-bento-candidate.sh" "$test_root/production" | tee "$test_root/bento.log"
bash "$project_dir/tests/verify-source.sh" | tee "$test_root/source-guards.log"
bash "$project_dir/tests/verify-control.sh" | tee "$test_root/control.log"
# Read-only session registration check, never toggles the user's effect.
bash "$project_dir/tests/verify-live-control.sh" | tee "$test_root/live-control.log"
if [[ -n ${KADUNCE_GATE_SCENES:-} ]]; then
    echo "PASS: selected scenes only ($KADUNCE_GATE_SCENES); run every scene before a handover"
else
    echo 'PASS: fresh-source integrated carry gate; physical appearance and unsupported routes remain separate'
fi
