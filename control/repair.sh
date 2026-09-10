#!/usr/bin/env bash
set -euo pipefail

# Only the final copy is privileged: never build or execute source as root.
if [[ ${1:-} == --install ]]; then
    [[ $EUID == 0 && $# == 4 ]] || exit 2
    candidate=$2
    expected=$3
    version=$4
    [[ $expected =~ ^[0-9a-f]{64}$ && $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || exit 2
    [[ $(/usr/bin/kwin_wayland --version) == "kwin $version" ]] || { echo 'KWin changed during repair; rebuild again.'; exit 1; }
    target=/usr/lib/qt6/plugins/kwin/effects/plugins/kwin4_effect_kadunce.so
    [[ -f $target && ! -L $target ]] || { echo 'Expected plugin missing; use the full installer.'; exit 1; }
    staged=$(mktemp /usr/lib/qt6/plugins/kwin/effects/plugins/.kadunce-repair.XXXXXX)
    trap 'rm -f -- "$staged"' EXIT
    /usr/bin/install -o root -g root -m 755 "$candidate" "$staged"
    [[ $(sha256sum "$staged" | cut -d' ' -f1) == "$expected" ]] || exit 1
    backup=$(mktemp /var/tmp/kadunce-plugin-backup.XXXXXX)
    cp --preserve=all "$target" "$backup"
    mv -fT -- "$staged" "$target"
    echo "Installed. Previous plugin backup: $backup"
    exit 0
fi

[[ $# == 0 && $EUID != 0 ]] || { echo 'Run repair as your normal user, not root.'; exit 2; }
repair_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
state_dir=${XDG_STATE_HOME:-$HOME/.local/state}/kadunce
mkdir -p "$state_dir"
exec 9>"$state_dir/repair.lock"
flock -n 9 || { echo 'Another repair is already running.'; exit 1; }
exec > >(tee "$state_dir/repair.log") 2>&1
echo 'Checking the installed source snapshot…'
cd "$repair_dir"
sha256sum --check source.sha256
for tool in cmake ctest c++ kwin_wayland pkexec; do
    command -v "$tool" >/dev/null || { echo "Missing prerequisite: $tool. Nothing installed."; exit 1; }
done
version=$(kwin_wayland --version)
[[ $version =~ ^kwin\ ([0-9]+\.[0-9]+\.[0-9]+)$ ]] || { echo 'Cannot identify installed KWin.'; exit 1; }
version=${BASH_REMATCH[1]}
work_dir=$(mktemp -d /tmp/kadunce-repair.XXXXXX)
trap 'rm -rf -- "$work_dir"' EXIT
tar -xf source.tar -C "$work_dir" --no-same-owner --no-same-permissions
echo "Building approved source for KWin $version…"
cmake -S "$work_dir/native" -B "$work_dir/build" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON
cmake --build "$work_dir/build" -j2
ctest --test-dir "$work_dir/build" --output-on-failure
candidate=$work_dir/build/bin/kwin/effects/plugins/kwin4_effect_kadunce.so
[[ -f $candidate ]] || exit 1
if ldd -r "$candidate" 2>&1 | grep -E 'not found|undefined symbol'; then
    echo 'Runtime dependency check failed; nothing installed.'
    exit 1
fi
[[ $(kwin_wayland --version) == "kwin $version" ]] || { echo 'KWin changed during build; retry repair.'; exit 1; }
# Check the factory built from headers, not just the executable queried above.
strings "$candidate" | grep -F "org.kde.kwin.EffectPluginFactory$version" >/dev/null
expected=$(sha256sum "$candidate" | cut -d' ' -f1)
echo 'Checks passed. Requesting permission to install one plugin file…'
pkexec /usr/bin/bash "$repair_dir/repair.sh" --install "$candidate" "$expected" "$version"
echo 'Repair completed. No effect toggle, desktop restart, or source download was performed.'
echo 'If KWin cached the previous plugin, save work and log out/in normally.'
