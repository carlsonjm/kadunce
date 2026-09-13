#!/usr/bin/env bash
# Source-level regression guard, not a GPU/runtime performance test.
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_file="$root/native/src/Effect.cpp"
pre=$(sed -n '/^void Effect::prePaintScreen(/,/^void Effect::postPaintScreen(/p' "$source_file")
post=$(sed -n '/^void Effect::postPaintScreen(/,/^void Effect::prePaintWindow(/p' "$source_file")
[[ -n $post ]] || { echo 'FAIL: no post-paint continuation'; exit 1; }
! rg -q 'addRepaintFull' <<<"$pre"
rg -q 'm_continueRepaint = false;' <<<"$pre"
rg -q 'm_continueRepaint = true;' <<<"$pre"
rg -q 'const bool continueRepaint = m_continueRepaint;' <<<"$post"
rg -q 'm_continueRepaint = false;' <<<"$post"
rg -q 'effects->postPaintScreen\(\)' <<<"$post"
rg -q 'if \(continueRepaint\)' <<<"$post"
rg -q 'effects->addRepaintFull\(\)' <<<"$post"
echo 'Post-paint continuation source guard passed (runtime acceptance separate).'
