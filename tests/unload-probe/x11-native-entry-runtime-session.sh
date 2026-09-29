#!/usr/bin/env bash
export KADUNCE_ENTRY_PLATFORM=xcb
exec "$(dirname -- "$0")/native-entry-runtime-session.sh"
