#!/usr/bin/env bash
# Puts the tablet at a panel's own scale, as Plasma does at sign-in, then runs
# the scene named in KADUNCE_SCALED_SESSION. KWin's --scale on a virtual output
# multiplies its size rather than scaling what it shows.
set -euo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
kscreen-doctor "output.Virtual-0.scale.${KADUNCE_TEST_SCALE:?}" >/dev/null 2>&1
sleep 1
echo "scaled tablet: $(kscreen-doctor -o 2>/dev/null | sed 's/\x1b\[[0-9;]*m//g' | grep -E 'Output|Geometry|Scale' | tr -s ' \n' ' ')"
exec "${KADUNCE_SCALED_SESSION:?}"
