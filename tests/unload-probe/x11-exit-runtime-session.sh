#!/usr/bin/env bash
set -euo pipefail
export KADUNCE_EXIT_PLATFORM=xcb
exec "$(dirname "$0")/exit-runtime-session.sh"
