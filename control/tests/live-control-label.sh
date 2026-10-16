#!/usr/bin/env bash
# The live tray gate finds the switch in the session's language as well as in
# English, against a stubbed session bus.
set -euo pipefail
project_dir=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
scratch=$(mktemp -d /tmp/kadunce-live-label.XXXXXX)
trap 'rm -rf -- "$scratch"' EXIT
mkdir "$scratch/bin"
cat >"$scratch/bin/qdbus6" <<'STUB'
#!/usr/bin/env bash
case "${*: -1}" in
    *RegisteredStatusNotifierItems) echo ':1.5/StatusNotifierItem' ;;
    *.Id) echo 'kadunce-control' ;;
    *.Status) echo 'Active' ;;
esac
STUB
cat >"$scratch/bin/gdbus" <<'STUB'
#!/usr/bin/env bash
case "$*" in
    *Properties.Get*) echo "(<objectpath '/MenuBar'>,)" ;;
    *GetLayout*) echo "(1, (0, {}, [<(1, {'label': <'${STUB_LABEL}'>, 'toggle-type': <'checkmark'>}, [])>]))" ;;
esac
STUB
cat >"$scratch/bin/gettext" <<'STUB'
#!/usr/bin/env bash
echo "${STUB_TRANSLATION:-${*: -1}}"
STUB
chmod +x "$scratch/bin/"*
gate() {
    PATH="$scratch/bin:$PATH" KADUNCE_LIVE_CONTROL_ATTEMPTS=1 \
        bash "$project_dir/tests/verify-live-control.sh" >/dev/null 2>&1
}
STUB_LABEL='Cards on' gate || { echo 'The English switch was not found' >&2; exit 1; }
STUB_LABEL='Karten an' STUB_TRANSLATION='Karten an' gate \
    || { echo 'A translated switch was not found' >&2; exit 1; }
if STUB_LABEL='Karten an' gate; then
    echo 'A switch with no translation behind it passed' >&2; exit 1
fi
if STUB_LABEL='Cards' gate; then
    echo 'Another label passed for the switch' >&2; exit 1
fi
echo 'Live tray gate checks passed'
