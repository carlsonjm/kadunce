#!/usr/bin/env bash
# Read-only release gate: the safety control must exist in the real tray.
set -euo pipefail
for attempt in {1..20}; do
    items=$(qdbus6 org.kde.StatusNotifierWatcher /StatusNotifierWatcher \
        org.kde.StatusNotifierWatcher.RegisteredStatusNotifierItems 2>/dev/null || true)
    while IFS= read -r item; do
        [[ $item == */* ]] || continue
        owner=${item%%/*}
        path=/${item#*/}
        title=$(qdbus6 "$owner" "$path" org.kde.StatusNotifierItem.Title 2>/dev/null || true)
        [[ $title == Kadunce ]] || continue
        status=$(qdbus6 "$owner" "$path" org.kde.StatusNotifierItem.Status 2>/dev/null || true)
        [[ $status == Active ]] || continue
        menu=$(gdbus call --session --dest "$owner" --object-path "$path" \
            --method org.freedesktop.DBus.Properties.Get org.kde.StatusNotifierItem Menu 2>/dev/null || true)
        menu_path=$(sed -n "s/.*objectpath '\([^']*\)'.*/\1/p" <<<"$menu")
        [[ -n $menu_path ]] || continue
        layout=$(gdbus call --session --dest "$owner" --object-path "$menu_path" \
            --method com.canonical.dbusmenu.GetLayout -- 0 -1 '[]' 2>/dev/null || true)
        if [[ $layout == *"'Kadunce enabled'"* && $layout == *"'toggle-type': <'checkmark'>"* ]]; then
            echo 'Kadunce safety control is registered, Active, and exposes its kill switch.'
            exit 0
        fi
    done <<<"$items"
    sleep 0.2
done
echo 'STOP: Kadunce kill switch is missing from the live tray. This build is not ready.' >&2
exit 1
