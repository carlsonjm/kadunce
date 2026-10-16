#!/usr/bin/env bash
# Read-only release gate: the safety control must exist in the real tray.
set -euo pipefail
# The switch's label as the tray shows it: translated in the session's
# language where a translation is installed, else in English.
labels=('Cards on')
if command -v gettext >/dev/null 2>&1; then
    for locale_dir in "${XDG_DATA_HOME:-$HOME/.local/share}/locale" /usr/share/locale; do
        translated=$(TEXTDOMAINDIR=$locale_dir gettext -d kadunce-control 'Cards on' 2>/dev/null || true)
        [[ -n $translated ]] && labels+=("$translated")
    done
fi
for _ in $(seq 1 "${KADUNCE_LIVE_CONTROL_ATTEMPTS:-20}"); do
    items=$(qdbus6 org.kde.StatusNotifierWatcher /StatusNotifierWatcher \
        org.kde.StatusNotifierWatcher.RegisteredStatusNotifierItems 2>/dev/null || true)
    while IFS= read -r item; do
        [[ $item == */* ]] || continue
        owner=${item%%/*}
        path=/${item#*/}
        id=$(qdbus6 "$owner" "$path" org.kde.StatusNotifierItem.Id 2>/dev/null || true)
        # KStatusNotifierItem may prefix the id it was given with the
        # application's name, as kadunce-control_kadunce-control.
        [[ $id == kadunce-control || $id == *_kadunce-control ]] || continue
        status=$(qdbus6 "$owner" "$path" org.kde.StatusNotifierItem.Status 2>/dev/null || true)
        [[ $status == Active ]] || continue
        menu=$(gdbus call --session --dest "$owner" --object-path "$path" \
            --method org.freedesktop.DBus.Properties.Get org.kde.StatusNotifierItem Menu 2>/dev/null || true)
        menu_path=$(sed -n "s/.*objectpath '\([^']*\)'.*/\1/p" <<<"$menu")
        [[ -n $menu_path ]] || continue
        layout=$(gdbus call --session --dest "$owner" --object-path "$menu_path" \
            --method com.canonical.dbusmenu.GetLayout -- 0 -1 '[]' 2>/dev/null || true)
        labelled=false
        for label in "${labels[@]}"; do
            [[ $layout == *"'${label}'"* || $layout == *"\"${label}\""* ]] && labelled=true
        done
        if [[ $labelled == true && $layout == *"'toggle-type': <'checkmark'>"* ]]; then
            echo 'Kadunce safety control is registered, Active, and exposes its kill switch.'
            exit 0
        fi
    done <<<"$items"
    sleep 0.2
done
echo 'STOP: Kadunce kill switch is missing from the live tray. This build is not ready.' >&2
exit 1
