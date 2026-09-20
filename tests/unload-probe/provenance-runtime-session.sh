#!/usr/bin/env bash
# The effect reports which plugin image its compositor has open, because an
# installer cannot read KWin's maps under a restricted-ptrace kernel. Prove the
# report names the file this KWin actually loaded, in a real compositor.
set -euo pipefail
trap 'echo "FAIL: plugin provenance line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
plugin="${KADUNCE_RUNTIME_BUILD:?}/bin/kwin/effects/plugins/kwin4_effect_kadunce.so"
test -f "$plugin"
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .6
reported=$(qdbus6 org.kde.KWin /Kadunce loadedPluginProvenance)
test -n "$reported"
read -r inode state <<<"$reported"
# A live, still-linked mapping of exactly the file this session was told to load.
test "$state" = present
test "$inode" = "$(stat -c %i "$plugin")"
echo 'PASS: the effect names the plugin image its compositor has open'
