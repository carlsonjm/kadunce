#!/usr/bin/env bash
# disable.sh against stand-ins for KWin and kwinrc: switched off only when KWin
# confirms the unload, and on again in kwinrc whenever KWin keeps the effect,
# so kwinrc never says off while cards are still held.
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d /tmp/kadunce-disable-test.XXXXXX)"
trap 'find "${scratch}" -depth -delete' EXIT

mkdir -p "${scratch}/bin"
# kwinrc is one line per key here: key=value.
cat > "${scratch}/bin/kwriteconfig6" <<'STUB'
#!/usr/bin/env bash
while (($#)); do
    case "$1" in --key) key="$2"; shift 2 ;; --file|--group) shift 2 ;; *) value="$1"; shift ;; esac
done
grep -v "^${key}=" "${KWINRC}" > "${KWINRC}.new" 2>/dev/null || true
echo "${key}=${value}" >> "${KWINRC}.new"
mv "${KWINRC}.new" "${KWINRC}"
STUB
# UNLOAD says how KWin answers unloadEffect: ok, false, or fail.
cat > "${scratch}/bin/qdbus6" <<'STUB'
#!/usr/bin/env bash
case "$3" in
    org.kde.kwin.Effects.unloadEffect)
        case "${UNLOAD}" in ok) echo true ;; false) echo false ;; *) exit 1 ;; esac ;;
esac
STUB
chmod +x "${scratch}/bin/kwriteconfig6" "${scratch}/bin/qdbus6"

run() {
    local answer="$1" home="${scratch}/home-$1"
    mkdir -p "${home}"
    export KWINRC="${scratch}/kwinrc-$1"
    echo "kwin4_effect_kadunceEnabled=true" > "${KWINRC}"
    HOME="${home}" UNLOAD="${answer}" PATH="${scratch}/bin:${PATH}" \
        bash "${project_dir}/disable.sh" >/dev/null 2>&1
}

fail() { echo "verify-disable: $1" >&2; exit 1; }

run ok || fail "a confirmed unload did not succeed"
grep -qx 'kwin4_effect_kadunceEnabled=false' "${scratch}/kwinrc-ok" \
    || fail "a confirmed unload did not leave kwinrc off"
[[ -e "${scratch}/home-ok/.local/state/kadunce/disabled" ]] \
    || fail "a confirmed unload did not mark Cards switched off"

for answer in false fail; do
    if run "${answer}"; then fail "an unload KWin did not confirm (${answer}) reported success"; fi
    grep -qx 'kwin4_effect_kadunceEnabled=true' "${scratch}/kwinrc-${answer}" \
        || fail "kwinrc says off while KWin kept the effect (${answer})"
    [[ ! -e "${scratch}/home-${answer}/.local/state/kadunce/disabled" ]] \
        || fail "Cards were marked off while KWin kept the effect (${answer})"
done

echo "disable.sh: switched off only when KWin confirms; kwinrc follows KWin otherwise"
