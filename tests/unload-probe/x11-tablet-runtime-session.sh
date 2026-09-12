#!/usr/bin/env bash
set -euo pipefail
export QT_QPA_PLATFORM=xcb KADUNCE_TEST_HAS_XDG=false KADUNCE_TEST_FRAMELESS=1
exec bash "$(dirname "$0")/tablet-runtime-session.sh"
