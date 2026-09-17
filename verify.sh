#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

python3 "${project_root}/tests/verify-docs.py"
bash "${project_root}/tests/verify-source.sh"
bash "${project_root}/tests/verify-package.sh"
bash "${project_root}/tests/verify-control.sh"

echo "Kadunce verification passed."
