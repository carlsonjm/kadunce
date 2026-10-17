#!/usr/bin/env bash
set -euo pipefail
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
scratch=$(mktemp -d /tmp/kadunce-repair-test.XXXXXX)
trap 'rm -rf -- "$scratch"' EXIT
bash "$project_dir/control/prepare-repair.sh" "$scratch/snapshot"
mkdir "$scratch/bin"
install -m755 "$project_dir/control/tests/deny-install.sh" "$scratch/bin/pkexec"
target=$(qtpaths6 --query QT_INSTALL_PLUGINS 2>/dev/null || echo /usr/lib/qt6/plugins)/kwin/effects/plugins/kwin4_effect_kadunce.so
before=$(sha256sum "$target")
status=0
KADUNCE_INSTALL_KEY=/nonexistent PATH="$scratch/bin:$PATH" XDG_STATE_HOME="$scratch/state" bash "$scratch/snapshot/repair.sh" >"$scratch/result" 2>&1 || status=$?
[[ $status == 126 ]]
grep -q '100% tests passed' "$scratch/result"
grep -q 'TEST: administrator authorization canceled' "$scratch/result"
# The administrator is asked to run only the system's own install, never a
# script from the home folder.
grep -q '^TEST: administrator asked to run: /usr/bin/install -Dm755 .*/kwin4_effect_kadunce\.so /usr/lib/qt6/plugins/kwin/effects/plugins/kwin4_effect_kadunce\.so$' "$scratch/result"
[[ $(sha256sum "$target") == "$before" ]]
# Snapshot corruption fails before any build or permission request.
truncate -s 0 "$scratch/snapshot/source.tar"
status=0
KADUNCE_INSTALL_KEY=/nonexistent PATH="$scratch/bin:$PATH" XDG_STATE_HOME="$scratch/state" bash "$scratch/snapshot/repair.sh" >"$scratch/corrupt-result" 2>&1 || status=$?
[[ $status != 0 ]]
! grep -q 'Building approved source' "$scratch/corrupt-result"
! grep -q 'TEST: administrator' "$scratch/corrupt-result"
[[ $(sha256sum "$target") == "$before" ]]
echo 'Repair tests passed: snapshot build/tests, canceled authorization, corrupt snapshot, installed plugin unchanged.'
