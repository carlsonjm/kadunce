#!/usr/bin/env bash
# fill-return with X11 applications: KWin takes a hidden X11 window off the
# task list, so a card held aside there leaves the dock unless Kadunce keeps it.
set -euo pipefail
export QT_QPA_PLATFORM=xcb
exec bash "$(dirname "$0")/fill-return-runtime-session.sh"
