#!/usr/bin/env bash
set -euo pipefail
# Only explicit installation updates this snapshot, never the repair action.
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
destination=${1:-${XDG_DATA_HOME:-$HOME/.local/share}/kadunce/repair}
mkdir -p "$destination"
tar -cf "$destination/source.tar" --exclude=build --exclude=.git -C "$project_dir" native
(cd "$destination" && sha256sum source.tar > source.sha256)
install -m 755 "$project_dir/control/repair.sh" "$destination/repair.sh"
echo 'Saved approved native source snapshot for offline repair.'
