#!/usr/bin/env bash

set -euo pipefail

# Guest-centered Spread must compact its formerly selected neighbor.
controller="$(dirname "$0")/../native/src/CardStageController.cpp"
sed -n '/CardStackPose CardStageController::stackPoseForWindow/,/KWin::Rect CardStageController::launcherGuestTarget/p' "$controller" | grep -F '&& (!m_launcherGuestActive || m_launcherGuestArrival)' > /dev/null
rg -Fq 'closed.visible = closedDepth <= 3;' "$controller"

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# ripgrep anchors a --glob containing a slash to the working directory rather
# than to the path being searched, so the exclusions below only held when this
# script happened to be run from the project root. Run from anywhere else --- an
# installer is normally launched from the user's home --- every one of them
# silently stopped matching and the guard failed on the documents it exists to
# permit. Standing in the project makes the globs mean what they say, whoever
# calls this and from wherever.
cd "${project_dir}"
native_dir="${project_dir}/native"
effect_cpp="${native_dir}/src/Effect.cpp"
effect_header="${native_dir}/src/Effect.h"
card_cpp="${native_dir}/src/CardStageController.cpp"
card_header="${native_dir}/src/CardStageController.h"
# Ordering of ownership publication, native placement and projection retirement
# is carried by OwnershipHandoff.h and covered behaviorally by the
# ownership-handoff test. The checks below assert only which calls a section may
# make, never where in the file a call sits.
python3 - "$card_cpp" <<'PY'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
def section(start, end):
    return source.split(start, 1)[1].split(end, 1)[0]
entry = section('void CardStageController::rebuildLiveCards()',
                'void CardStageController::retainManagedOwnership(')
ownership = section('void CardStageController::retainManagedOwnership(',
                    'bool CardStageController::enterActive()')
for mutation in ('enterActive(', 'moveResize(', 'maximize(', 'setFullScreen('):
    assert mutation not in entry + ownership, 'Entry ownership must not activate/resize cards'
transfer = section('bool CardStageController::admitTransferredWindowToTablet(',
                   'void CardStageController::startArrivalTimer(')
assert 'if (sameOutput && m_active && !managedRestore(arrival))' in transfer
PY
# A2 projection transfers retained ownership before native visibility changes.
python3 - "$card_cpp" "$effect_cpp" <<'PY_A2'
import pathlib, sys
card, effect = (pathlib.Path(p).read_text() for p in sys.argv[1:])
projection = card.split('bool CardStageController::admitBentoStack(', 1)[1].split('void CardStageController::release()', 1)[0]
assert 'enterActive(' not in projection and 'moveResize(' not in projection
assert 'setMinimized(false)' not in projection, "Projection must not change a member's minimization"
resume = pathlib.Path(sys.argv[1]).read_text().split('bool CardStageController::resumeSelectedBentoProjection()', 1)[1].split('void CardStageController::release()', 1)[0]
assert all(call not in resume for call in ('moveResize(', 'setMinimized(', 'enterActive('))
retire = effect.split('void Effect::retireBentoProjectionForCardStage(', 1)[1].split('bool Effect::admitCardToDesktopStage(', 1)[0]
for required in ('m_fanApertureWindow = nullptr', 'm_previewSourceBounds.remove(window)',
                 'unredirect(window)', 'addRepaintFull()'):
    assert required in retire, f'projection retirement must include {required}'
route = effect.split('void Effect::toggle()', 1)[1].split('void Effect::release()', 1)[0]
assert 'transferTabletSessionToSpread' in route and 'admitBentoStack' in route
assert 'toggleOnOutput' not in route, 'Spread entry must not discard Bento ownership'
PY_A2
# CARD-LIFECYCLE.md §5: a window a layout cannot show becomes an AWAKE card.
# Publishing a layout is therefore the one place that must never minimize, which
# is what the deleted overflow container made it do.
python3 - "${native_dir}/src/DesktopStageController.cpp" <<'PY_PUBLISH'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
publish = source.split('bool DesktopStageController::applySession(', 1)[1].split(
    'void DesktopStageController::scheduleSettle()', 1)[0]
assert 'setMinimized(' not in publish, 'Publishing a layout must not minimize a window'
assert 'make_unique<RestoredMinimization>' not in publish, 'Publishing a layout must not defer a minimize'
shed = source.split('bool DesktopStageController::shedUnshowable(', 1)[1].split(
    'bool DesktopStageController::applySession(', 1)[0]
assert 'evictToTablet(' in shed, 'Shedding must hand the window to card ownership'
assert 'm_sessions.constFind(' in shed, 'Shedding must re-find its session between evictions'
PY_PUBLISH
# CARD-LIFECYCLE.md §13 keeps the native desktop for release and disable, and §5
# sends a window the layout cannot show to card ownership. A placement that does
# not settle is therefore answered by shedding the panes that would not take
# their rects, never by returning the session to Plasma.
python3 - "${native_dir}/src/DesktopStageController.cpp" <<'PY_SETTLE'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
settle = source.split('void DesktopStageController::settleSessions(', 1)[1].split(
    'void DesktopStageController::shedUnsettledPanes(', 1)[0]
assert 'restoreSession(' not in settle, 'A settle must not return a session to Plasma'
assert 'shedUnsettledPanes(' in settle, 'A settle must answer an unplaced pane by shedding it'
assert 'm_settleRetries' in settle, 'A placement must be asked for once more before it is judged'
shed = source.split('void DesktopStageController::shedUnsettledPanes(', 1)[1].split(
    'bool DesktopStageController::sessionGeometryMatches(', 1)[0]
assert 'extractPaneToCards(' in shed, 'An unplaced pane must leave for card ownership'
assert 'restoreSession(' not in shed, 'Shedding must not return the session to Plasma'
assert 'isMinimized()' in shed, 'A sleeping pane belongs to §7, not to the settle'
PY_SETTLE
python3 - "${native_dir}/src/DesktopStageController.cpp" <<'PY_RESUME'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
resume = source.split('bool DesktopStageController::resumeProjectedSession(', 1)[1].split('void DesktopStageController::restoreAllSessions()', 1)[0]
# CARD-LIFECYCLE.md §6 resumes the group whatever its panes did meanwhile, so a
# pane whose client changed its own frame is placed back on the stored rect
# rather than refused. What resume must still never do is solve a new layout,
# put a window to sleep, or write geometry of its own: its only placement is the
# stored one applySession publishes, and only for a settled session.
for forbidden in ('reflowSession(', 'moveResize(', 'setMinimized('):
    assert forbidden not in resume, f'exact projection resume must not call {forbidden}'
assert '!resumed->participationDirty' in resume, \
    'resume must place only a session with no participation change owing'
PY_RESUME
stack_browse=$(sed -n '/^void CardStageController::pageStack(int delta)/,/^void CardStageController::rebuildLiveCards()/p' "$card_cpp")
printf '%s\n' "$stack_browse" | perl -0777 -ne 'exit(!/captureCardTransition\(false, true\);.*?m_stackBrowseOutgoing = selectedWindow\(\);.*?m_workspace.pageStack\(delta\);.*?m_stackBrowseDirection =.*?syncSelectedElevation\(\)/s)'
rg -Fq 'm_stackBrowseOutgoing.clear();' "$card_cpp"
rg -Fq 'm_stackBrowseDirection ? StackBrowseDuration' "$card_cpp"
rg -Fq 'm_cardGrabActive ? stackBrowseTarget() : m_workspace.selectedId()' "$card_cpp"
pickup=$(sed -n '/^void CardStageController::beginCardGrab(/,/^void CardStageController::updateCardGrab(/p' "$card_cpp")
printf '%s\n' "$pickup" | perl -0777 -ne 'exit(!/setElevatedWindow\(selectedWindow\(\), true\);\s*syncSelectedStackingOrder\(\)/s)'
held_order=$(sed -n '/^void CardStageController::pageCardGrab(/,/^void CardStageController::finishCardGrab(/p' "$card_cpp")
printf '%s\n' "$held_order" | perl -0777 -ne 'exit(!/m_cardGrabPageOffset =.*?syncSelectedStackingOrder\(\)/s)'
router_cpp="${native_dir}/src/WorkspaceInputRouter.cpp"
router_header="${native_dir}/src/WorkspaceInputRouter.h"
desktop_cpp="${native_dir}/src/DesktopStageController.cpp"
desktop_header="${native_dir}/src/DesktopStageController.h"
metadata_file="${native_dir}/src/metadata.json"
install_script="${project_dir}/install.sh"

# Check the shared entry point, not just shortcut callers: arrivals and external
# activation also enter Active directly and must not retain stack elevation.
active_entry=$(sed -n '/^bool CardStageController::enterActive()/,/^void CardStageController::restoreActiveSnapshot()/p' "$card_cpp")
printf '%s\n' "$active_entry" | perl -0777 -ne 'exit(!/m_presentation = CardPresentation::Active;.*?syncSelectedElevation\(\);.*?activateWindow/s)'

# Paging captures presentation before changing selection, with no animation gate.
row_entry=$(sed -n '/^void CardStageController::pageHorizontal(/,/^bool CardStageController::beginLauncherGuest()/p' "$card_cpp")
printf '%s\n' "$row_entry" | perl -0777 -ne 'exit(!/captureCardTransition\(false, true\);.*?m_rowPageTransition = true;.*?m_workspace.page\(delta\)/s)'
held_page=$(sed -n '/^void CardStageController::pageCardGrab(/,/^void CardStageController::finishCardGrab(/p' "$card_cpp")
printf '%s\n' "$held_page" | perl -0777 -ne 'exit(!/captureCardTransition\(false, true\);.*?m_cardGrabPageOffset =/s)'
rg -Fq '((!m_rowPageTransition && !m_pickupTransition) || window == selectedWindow())' "$card_cpp"
rg -Fq 'm_pickupTransition = m_poseTransition;' "$card_cpp"
rg -Fq 'if (!commit) clearCardTransition();' "$card_cpp"
rg -Fq 'if (m_rowPageTransition) clearCardTransition();' "$card_cpp"
rg -Fq 'm_cardStage->paintSlot(window)' "$effect_cpp"
rg -Fq 'm_workspace.idAtOffset(side * 2)' "$card_cpp"
rg -Fq 'm_workspace.count() == 0' "$card_cpp"
rg -Fq 'm_preparationNeighbors.at(' "$effect_cpp"
rg -Fq 'KWin::Region(), preparation' "$effect_cpp"
rg -Fq 'size.width()*size.height()*scale*scale*4 <= 32*1024*1024' "$effect_cpp"
if rg -q 'paintSlot\(' "$router_cpp"; then
    echo 'Paint-only departing cards must not become input targets' >&2; exit 1
fi

python3 -m json.tool "${metadata_file}" >/dev/null
bash -n "${install_script}"

retired_brand='web''os|pa''lm|ghostie''post|chrome''os'
# TERMINOLOGY.md is the document that defines these as retired, so it must name
# them, exactly as this guard names them without matching itself.
if rg -ni "${retired_brand}" "${project_dir}" \
        --glob '!.git/**' --glob '!docs/TERMINOLOGY.md'; then
    echo "Kadunce contains retired product or development-machine branding" >&2
    exit 1
fi

# Retired vocabulary must not return. Layer 3 keeps three installed identities
# that a vocabulary pass may not touch, so they are named here as the only
# permitted occurrences; Block 10b retires them with a versioned migration.
# `docs/archive/` preserves the language of its own candidate and is exempt.
retired_vocabulary='card''line|card line'
# Matched case-sensitively, so only these exact spellings are permitted.
frozen_identity='showCardLine|cardLine|Kadunce Card Line'
if rg -ni "${retired_vocabulary}" "${project_dir}" \
        --glob '!.git/**' --glob '!docs/archive/**' \
        --glob '!docs/TERMINOLOGY.md' --glob '!tests/verify-source.sh' \
        --glob '!build-native/**' \
        | rg -v "${frozen_identity}"; then
    echo "Kadunce contains retired workspace vocabulary; Spread is the approved term" >&2
    exit 1
fi

# `cardLine` is a frozen wire value, not vocabulary, so the guard above cannot
# protect it: that one only watches for the retired word coming back, and a
# vocabulary pass moves in the opposite direction. Block 1b's mechanical pass
# rewrote the value to `spread` inside three probe assertions, which then
# asserted a presentation the effect never reports. All three failed silently
# from that day and one sat inside the integrated carry gate. Check the wire in
# both directions instead: the effect must still report the frozen value, and
# every presentation a test asserts must be one the effect can report.
rg -q 'QStringLiteral\("cardLine"\)' "${project_dir}/native/src/Effect.cpp" || {
    echo "Effect no longer reports the frozen cardLine presentation" >&2
    exit 1
}
# Keep in step with the presentations Effect::workspaceContext reports.
reported_presentations='inactive|cardLine|bento|active'
if rg -o --no-filename 'presentation"?\s*(?:==|:)\s*"([A-Za-z]+)"' \
        "${project_dir}/tests" -r '$1' \
        | sort -u | rg -v "^(${reported_presentations})$"; then
    echo "A test asserts a cardStage presentation the effect never reports" >&2
    exit 1
fi

# Installation is transactional: validation/build and the privileged,
# byte-verified copy must finish before the accepted live effect is disabled.
native_build_line="$(rg -n 'cmake --build "\$\{native_build_dir\}"' \
    "${install_script}" | cut -d: -f1)"
native_copy_line="$(rg -n '^pkexec /usr/bin/install' \
    "${install_script}" | cut -d: -f1)"
native_disable_line="$(rg -n -- '--key "\$\{native_effect_id\}Enabled" false' \
    "${install_script}" | cut -d: -f1)"
test "${native_build_line}" -lt "${native_copy_line}"
test "${native_copy_line}" -lt "${native_disable_line}"
rg -q 'The previous effect configuration was restored' "${install_script}"
rg -q 'registration_started=true' "${install_script}"
rg -q 'All six installation steps completed' "${install_script}"
rg -q 'tests/verify-control\.sh' "${install_script}"
rg -q 'kadunce-control\.service' "${install_script}"
rg -q 'systemctl --user reenable kadunce-control\.service' \
    "${install_script}"
rg -q 'systemctl --user start kadunce-control\.service' \
    "${install_script}"

rg -q '"Id": "kwin4_effect_kadunce"' "${metadata_file}"
rg -q 'kwin4_effect_kadunce' "${native_dir}/CMakeLists.txt"
rg -q '0\.1\.0-kadunce-baseline' "${effect_cpp}" "${card_cpp}"
rg -q 'windowStepUserMovedResized' "${effect_cpp}" "${card_cpp}"
rg -q 'armed Bento drop across output seam' "${desktop_cpp}"
rg -q 'm_forwardedPointerButtons.remove\(event->button\)' "${router_cpp}"
rg -q 'Foreign desktop release activated a tablet card' "${native_dir}/tests/PanelInputTest.cpp"
rg -q 'm_workspace\.stackSizeForId\(selectedId\) > 1' "${effect_cpp}" "${card_cpp}"
rg -q 'ExportScriptableSlots' "${effect_cpp}" "${card_cpp}"
rg -q 'Q_SCRIPTABLE QString workspaceContext\(\) const' "${effect_header}"
rg -q 'Q_SCRIPTABLE bool activateApplicationWindow' "${effect_header}"
rg -q 'bool Effect::activateApplicationWindow' "${effect_cpp}"
rg -q 'Q_SCRIPTABLE int launcherGuestProtocolVersion' "${effect_header}"
rg -q 'Q_SCRIPTABLE QString beginLauncherGuest' "${effect_header}"
rg -q 'Q_SCRIPTABLE void updateLauncherGuest' "${effect_header}"
rg -q 'Q_SCRIPTABLE bool finishLauncherGuest' "${effect_header}"
rg -q 'Q_SCRIPTABLE bool prepareLauncherGuestLaunch' "${effect_header}"
rg -q 'Q_SCRIPTABLE void cancelLauncherGuestLaunch' "${effect_header}"
rg -q 'Q_SCRIPTABLE void endLauncherGuest' "${effect_header}"
rg -q 'int Effect::launcherGuestProtocolVersion.*const' "${effect_cpp}"
rg -q 'return 3;' "${effect_cpp}"
rg -q 'readyForPaintingChanged' "${effect_cpp}"
rg -q 'desktopFileNameChanged' "${effect_cpp}"
rg -q 'completeLauncherGuestForWindow\(candidate\)' "${effect_cpp}"
rg -Fq 'finishNewArrival(window, animateArrival, previousSelection)' "${card_cpp}"
admission=$(sed -n '/^void CardStageController::finishNewArrival(/,/^}/p' "${card_cpp}")
guest_guard=$(printf '%s\n' "$admission" | rg -n 'if \(m_launcherGuestActive\)' | cut -d: -f1)
promote=$(printf '%s\n' "$admission" | rg -n 'if \(!enterActive\(\)\)' | cut -d: -f1)
test -n "$guest_guard" && test "$guest_guard" -lt "$promote"
printf '%s\n' "$admission" | rg -q 'm_workspace.selectIndex\(previousSelection\)'
rg -q 'QDBusServiceWatcher::WatchForUnregistration' "${effect_cpp}"
rg -q 'm_cardStage->beginLauncherGuest' "${effect_cpp}"
rg -q 'm_cardStage->launcherGuestTarget' "${effect_cpp}" "${card_cpp}"
rg -q 'launcherGuestTargetForSlot' "${effect_cpp}" "${card_cpp}" "${card_header}"
rg -q 'makeLauncherGuestLayout' "${card_cpp}"
rg -q 'm_launcherGuestGroupCount' "${card_cpp}"
rg -q 'm_launcherGuestLaunchPending' "${effect_cpp}" "${effect_header}"
rg -q 'QStringLiteral\("completeGuestLaunch"\)' "${effect_cpp}"
rg -q 'launcherGuestTransitionProgress' "${effect_cpp}" "${card_cpp}" \
    "${card_header}"
rg -q 'LauncherGuestTransitionDuration = 220' "${card_cpp}"
rg -q 'LauncherGuestDragPreview = 0\.18' "${card_cpp}"
rg -q -- '-16\.0 \* liftBlend' "${effect_cpp}"
rg -q 'slot < 0 \? -0\.8 : 0\.8' "${effect_cpp}"
rg -q 'm_cardStage->endLauncherGuest' "${effect_cpp}"
rg -q 'launcherGuestContainsForInput' "${router_cpp}" "${router_header}"
rg -q 'navigateLauncherGuestFromInput' "${router_cpp}" "${router_header}" \
    "${effect_cpp}" "${effect_header}"
rg -q 'm_launcherGuestTouchIds' "${router_cpp}" "${router_header}"
rg -q 'm_launcherGuestNavigationTouchIds' "${router_cpp}" "${router_header}"
if rg -q 'appendCard.*launcher|launcher.*appendCard' "${effect_cpp}" "${card_cpp}"; then
    echo "A launcher guest must never enter the persistent card model" >&2
    exit 1
fi
rg -q 'studio\.warbler\.kadunce\.workspace-context' "${effect_cpp}"
rg -q 'io\.github\.carlsonjm\.Tettegouche' "${effect_cpp}"
rg -q 'identity\.compare\(QStringLiteral\("tettegouche"\)' \
    "${effect_cpp}"
rg -q 'QJsonDocument\(root\)\.toJson\(QJsonDocument::Compact\)' \
    "${effect_cpp}"
rg -q 'window->internalId\(\)\.toString\(QUuid::WithoutBraces\)' \
    "${effect_cpp}"
rg -q 'desktopFileName\(\)' "${effect_cpp}"
rg -q 'resourceClass\(\).*trimmed\(\)\.toLower\(\)' "${effect_cpp}"
rg -q 'resourceName\(\).*trimmed\(\)\.toLower\(\)' "${effect_cpp}"
rg -q 'm_desktopStage->hasSessionOnOutput' "${effect_cpp}"
rg -q 'TETTEGOUCHE-CONTEXT\.md' "${native_dir}/../docs/ARCHITECTURE.md"
rg -q 'EffectsHandler::windowAdded' "${effect_cpp}" "${card_cpp}"
rg -q 'm_workspace\.append\(' "${card_cpp}"
rg -q 'const double originX' "${effect_cpp}" "${card_cpp}"
rg -q 'const double originY' "${effect_cpp}" "${card_cpp}"
rg -q 'QRegion remains only the hard output fence' "${effect_cpp}" "${card_cpp}"
rg -q 'Ctrl\+S' "${effect_cpp}" "${card_cpp}"
rg -q 'Ctrl\+Esc' "${effect_cpp}" "${card_cpp}"
rg -q 'Ctrl\+Left' "${effect_cpp}" "${card_cpp}"
rg -q 'Ctrl\+Right' "${effect_cpp}" "${card_cpp}"
rg -q 'paintScreen' "${effect_cpp}" "${card_cpp}"
rg -q 'cardPaintRoute\(m_paintingOutput == tablet' "${effect_cpp}"
rg -q 'CardPaintRoute::Hidden' "${effect_cpp}"
rg -q 'deviceRegion & KWin::Region\(outputClip\)' "${effect_cpp}"
rg -q 'window->screen\(\) != tablet' "${effect_cpp}" "${card_cpp}"
rg -q 'blocksDirectScanout' "${effect_header}" "${card_header}"
rg -q 'CardWorkspaceState<QPointer<KWin::EffectWindow>> m_workspace' "${effect_header}" "${card_header}"
rg -q 'rebuildLiveCards' "${effect_cpp}" "${card_cpp}"
rg -q 'setPositionTransformations' "${effect_cpp}" "${card_cpp}"
rg -q 'Presentation::Active' "${effect_cpp}" "${card_cpp}"
rg -q 'Qt::KeepAspectRatioByExpanding' "${effect_cpp}" "${card_cpp}"
rg -q 'rotatedFanCard.*!qFuzzyIsNull' "${effect_cpp}" "${card_cpp}"
if rg -q 'Qt::IgnoreAspectRatio|const Qt::AspectRatioMode aspectMode' \
        "${effect_cpp}" "${card_cpp}"; then
    echo "Rotated fan snapshots must never be stretched into their slots" >&2
    exit 1
fi
rg -q 'position.xy \* paintSize - apertureOrigin' "${effect_cpp}"
rg -q 'Geometry coordinates are independent of texture flips' "${effect_cpp}"
rg -q 'in vec2 cardPoint' "${effect_cpp}"
rg -q 'target.x\(\) - logicalRegion.x\(\)' "${effect_cpp}"
rg -q 'stackPaintOrderForId' "${native_dir}/src/SpreadModel.cpp" \
    "${effect_cpp}" "${card_cpp}"
rg -q 'A large fan did not expose a deterministic back-to-front deck' \
    "${native_dir}/tests/SpreadModelTest.cpp"
rg -q 'restoreOriginalStackingOrder' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'mapToDeviceCoordinatesAligned\(target\)' "${effect_cpp}" "${card_cpp}"
rg -q 'roundedClip(deviceTarget, CardCornerRadius' -F "${effect_cpp}"
rg -q 'const bool useFanAperture = m_fanApertureShader' -F "${effect_cpp}"
rg -q 'useFanAperture ? KWin::Region(deviceAperture)' -F "${effect_cpp}"
rg -q 'constexpr double CardCornerRadius = 10\.0' "${effect_cpp}" "${card_cpp}"
rg -q 'path\.addRoundedRect' "${effect_cpp}" "${card_cpp}"
rg -q 'CardCornerRadius \* viewport\.scale\(\)' "${effect_cpp}" "${card_cpp}"
rg -q 'class WorkspaceInputRouter final : public KWin::InputEventFilter' \
    "${router_header}"
rg -q 'class WorkspaceInputTarget' "${router_header}"
rg -q 'InputFilterOrder::Effects' "${router_cpp}"
rg -q 'tablet->geometry\(\)\.contains\(position\.toPoint\(\)\)' "${effect_cpp}" "${card_cpp}"
rg -q 'std::make_unique<WorkspaceInputRouter>' "${effect_cpp}" "${card_cpp}"
test "$(rg -c 'installInputEventFilter' "${effect_cpp}")" -eq 1
rg -q 'z13TabletKitAvailable' "${effect_cpp}"
rg -q 'registerTouchBorder' "${effect_cpp}"
rg -q 'unregisterTouchBorder' "${effect_cpp}"
rg -q 'KWin::ElectricBottom' "${effect_cpp}"
rg -q 'KWin::ElectricTop' "${effect_cpp}"
rg -q 'm_usesDirectSystemEdges' "${effect_cpp}" "${effect_header}"
rg -q 'm_ownsSystemEdges' "${router_cpp}" "${router_header}"
rg -q 'TouchMode::BottomEdge : TouchMode::None' "${router_cpp}"
rg -q 'TouchMode::TopEdge : TouchMode::None' "${router_cpp}"
if rg -q 'setSpreadInputActive|SpreadInputFilter' \
        "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}" "${router_cpp}" "${router_header}"; then
    echo "Four-edge input must have one persistent state-aware filter" >&2
    exit 1
fi
if rg -q 'Effect \*|m_effect|friend class WorkspaceInputRouter' \
        "${router_cpp}" "${router_header}" "${effect_header}" "${card_header}"; then
    echo "WorkspaceInputRouter must depend only on its semantic target" >&2
    exit 1
fi

# Card Stage is now the sole owner of mutable tablet-card state and card
# transactions. Effect retains lifecycle/context/shortcut orchestration and
# consumes only the controller's read-only render view.
rg -q 'class CardStageController final' "${card_header}"
rg -q 'class CardStageHost' "${card_header}"
rg -q 'std::make_unique<CardStageController>' "${effect_cpp}"
rg -q 'm_cardStage->stackPoseForWindow' "${effect_cpp}"
rg -q 'const auto &spread = m_workspace.model\(\)' "${card_cpp}"
rg -q 'm_host->admitCardToDesktopStage' "${card_cpp}"
rg -q 'm_cardStage->handleWindowAdded' "${effect_cpp}"
if rg -q '\bm_workspace\b|\bm_liveCards\b|\bm_originalCardStackingOrder\b|\bm_activeRestore\b|\bm_presentation\b|\bm_cardGrabOffset\b|\bm_cardStackPreviewTarget\b|\bm_cardStackInsertionIndex\b' \
        "${effect_cpp}" "${effect_header}"; then
    echo "Mutable Card Stage state must not leak back into Effect" >&2
    exit 1
fi
if rg -q 'Effect \*|m_effect|friend class CardStageController' \
        "${card_cpp}" "${card_header}" "${effect_header}"; then
    echo "CardStageController must depend only on its typed host" >&2
    exit 1
fi
rg -q 'presentationForInput' "${router_cpp}" "${router_header}" "${effect_cpp}" "${card_cpp}"
rg -q 'geometryForInput' "${router_cpp}" "${router_header}" "${effect_cpp}" "${card_cpp}"
rg -q 'constexpr double SystemEdgeWidth = 36\.0' "${router_cpp}"
rg -q 'TouchMode::ActiveLeft' "${router_cpp}"
rg -q 'TouchMode::ActiveRight' "${router_cpp}"
rg -q 'TouchMode::BottomEdge' "${router_cpp}"
rg -q 'TouchMode::TopEdge' "${router_cpp}"
rg -q 'delta\.x\(\) > 30\.0' "${router_cpp}"
rg -q 'delta\.x\(\) < -30\.0' "${router_cpp}"
rg -q 'delta\.y\(\) < -40\.0' "${router_cpp}"
rg -q 'delta\.y\(\) > 40\.0' "${router_cpp}"
if rg -q 'activeResizeGuardContains|ResizeGuardWidth|ActiveGuard' \
    "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"; then
    echo "Active must not reserve an input strip inside the application" >&2
    exit 1
fi
if rg -q 'cancelInteractiveMoveResize' "${card_cpp}"; then
    echo "Active must release manual window changes, not cancel them" >&2
    exit 1
fi
rg -q 'QScopedValueRollback<bool> applying' "${card_cpp}"
rg -q 'setGeometryRestore\(snapshot.floatingGeometry\)' "${native_dir}/src/WindowStateRestore.h"
rg -q 'setFullscreenGeometryRestore' "${native_dir}/src/WindowStateRestore.h"
rg -q 'restoreWindowState' "${card_cpp}"
rg -q 'handleManualWindowChange' "${effect_cpp}"
rg -q 'm_activeSettleRemaining = 2' "${card_cpp}"
rg -q '\-\-m_activeSettleRemaining' "${card_cpp}"
rg -q 'm_activeSettleTimer.stop' "${card_cpp}"
release_body=$(sed -n '/^void CardStageController::release()/,/^}/p' "${card_cpp}")
release_inactive=$(printf '%s\n' "$release_body" | rg -n 'm_active = false' | cut -d: -f1)
release_unredirect=$(printf '%s\n' "$release_body" | rg -n 'unredirectForCardStage' | cut -d: -f1)
release_restore=$(printf '%s\n' "$release_body" | rg -n 'restoreActiveSnapshot' | cut -d: -f1)
test "$release_inactive" -lt "$release_unredirect"
test "$release_unredirect" -lt "$release_restore"
rg -q 'Kadunce fullscreen release: direct scene, restored focus' "${card_cpp}"
rg -q 'constexpr double CardHoldMotion = 12\.0' "${router_cpp}"
rg -q 'constexpr int CardHoldDelay = 300' "${router_cpp}"
rg -q 'm_holdTimer\.setSingleShot\(true\)' "${router_cpp}"
rg -q 'beginCardGrab' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'updateCardGrab' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'finishCardGrab' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'setElevatedWindow\(selectedWindow\(\), true\)' "${effect_cpp}" "${card_cpp}"
rg -q 'm_cardStage->cardGrabOffset\(\)' "${effect_cpp}"
rg -q 'target = m_cardStage->cardGrabTarget\(\)' "${effect_cpp}"
rg -q 'm_cardGrabOffset = position - m_cardGrabStart' "${card_cpp}"
rg -q 'beginCardGrab\(holdCurrent\(\)\)' "${router_cpp}"
rg -q 'm_workspace\.moveSelected\(movement\)' "${effect_cpp}" "${card_cpp}"
rg -q 'void SpreadModel::moveSelected' \
    "${native_dir}/src/SpreadModel.cpp"
rg -q 'SpreadModel::detachedNeighborhood' \
    "${native_dir}/src/SpreadModel.cpp"
rg -q 'm_workspace\.detachedNeighborhood\(m_cardGrabPageOffset\)' \
    "${effect_cpp}" "${card_cpp}"
rg -q 'Three-card edge page did not remain deterministic' \
    "${native_dir}/tests/SpreadModelTest.cpp"
rg -q 'constexpr double CardEdgeZoneFraction = 0\.08' "${router_cpp}"
rg -q 'constexpr double CardEdgeZoneMinimum = 72\.0' "${router_cpp}"
rg -q 'constexpr int CardEdgeDwellDelay = 300' "${router_cpp}"
rg -q 'constexpr int CardEdgeRepeatDelay = 350' "${router_cpp}"
rg -q 'm_edgePageTimer\.start\(fast \? CardEdgeDwellDelay : delay\)' "${router_cpp}"
rg -q 'm_edgePageTimer\.start\(m_edgePageDelay\)' "${router_cpp}"
rg -q 'pageCardGrab' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
# A placement must read the keyboard, never infer it from the work area. The
# Keyboard asks the bottom panels to yield as it raises, so the area grows at
# the moment the space stops being free, and a card that trusts the area grows
# down into the keys. CURRENT_STATE.md records the measurement.
rg -Fq 'KWin::effects->inputPanel()' "${effect_cpp}"
active_target=$(sed -n '/^KWin::Rect CardStageController::activeTarget(/,/^void CardStageController::refreshActivePlacement()/p' "$card_cpp")
printf '%s\n' "$active_target" | rg -Fq 'inputPanelTopForCardStage(output)'
printf '%s\n' "$active_target" | rg -Fq 'std::max(clearance, work.bottom() - *panelTop)'
rg -Fq 'EffectsHandler::inputPanelChanged' "${effect_cpp}"
# A pane the compositor lifted for the keyboard is not a client refusing its
# rect, so re-asserting the stored layout must never reach §5's shed.
python3 - "${desktop_cpp}" <<'PY_PANEL'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
reassert = source.split('void DesktopStageController::reassertPlacementsForInputPanel()', 1)[1].split(
    'void DesktopStageController::settleSessions()', 1)[0]
assert 'inputPanelTopForDesktopStage(output)' in reassert, \
    're-asserting must not run while the keyboard is still up'
assert 'applySession(' in reassert, 're-asserting must place the stored layout'
for forbidden in ('shedUnsettledPanes(', 'scheduleSettle(', 'm_settleRetries', 'reflowSession('):
    assert forbidden not in reassert, f'keyboard re-assert must not call {forbidden}'
PY_PANEL

rg -q 'constexpr int CardStackDwellDelay = 350' "${router_cpp}"
rg -q 'constexpr int CardStackTransitionDuration = 350' "${effect_cpp}" "${card_cpp}"
rg -q 'QEasingCurve::InQuart' "${effect_cpp}" "${card_cpp}"
rg -q 'SpreadModel::stackSelectedWith' \
    "${native_dir}/src/SpreadModel.cpp"
rg -q 'makeOpenStackPose' "${native_dir}/src/SpreadLayout.cpp" "${effect_cpp}" "${card_cpp}"
rg -q 'makeClosedStackPose' "${native_dir}/src/SpreadLayout.cpp" "${effect_cpp}" "${card_cpp}"
rg -q 'slot \* width \* 0\.24 / 3\.0' \
    "${native_dir}/src/SpreadLayout.cpp"
rg -q 'StackClosedStep = 7\.0' \
    "${native_dir}/src/SpreadLayout.cpp"
rg -q 'return fanPose\(relative, cardWidth\)' \
    "${native_dir}/src/SpreadLayout.cpp"
rg -q 'StackFaceRotation = 0\.6' \
    "${native_dir}/src/SpreadLayout.cpp"
rg -q 'angles\{0\.6, 0\.2, -0\.2, -0\.4, -0\.6\}' \
    "${native_dir}/src/SpreadLayout.cpp"
rg -q 'The bottom reference layout shoulder did not retain its slight upward tilt' \
    "${native_dir}/tests/SpreadLayoutTest.cpp"
rg -q 'setRotationAngle\(paintPose\.rotation\)' "${effect_cpp}" "${card_cpp}"
rg -q 'makeOpenStackEnvelope' \
    "${native_dir}/src/SpreadLayout.cpp" "${effect_cpp}" "${card_cpp}"
rg -q 'makeReservedCardTarget' \
    "${native_dir}/src/SpreadLayout.cpp" "${effect_cpp}" "${card_cpp}"
rg -q 'bottomRightRotationEnvelope' \
    "${native_dir}/src/SpreadLayout.cpp"
rg -q 'const double groupCentering' \
    "${native_dir}/src/SpreadLayout.cpp"
rg -q 'Left neighbor did not preserve the gutter around the stack' \
    "${native_dir}/tests/SpreadLayoutTest.cpp"
rg -q 'A large stack hid cards from vertical member paging' \
    "${native_dir}/tests/SpreadModelTest.cpp"
rg -q 'void SpreadModel::pageStack' \
    "${native_dir}/src/SpreadModel.cpp"
rg -q 'Horizontal paging did not treat a stack as one group' \
    "${native_dir}/tests/SpreadModelTest.cpp"
rg -q 'QKeySequence\(QStringLiteral\("Ctrl\+Up"\)\)' "${effect_cpp}" "${card_cpp}"
rg -q 'QKeySequence\(QStringLiteral\("Ctrl\+Down"\)\)' "${effect_cpp}" "${card_cpp}"
rg -q 'selectedStackContains' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'classifyStackGesture' \
    "${native_dir}/src/SpreadLayout.cpp" "${router_cpp}"
rg -q 'Vertical motion outside a stack changed its member' \
    "${native_dir}/tests/SpreadLayoutTest.cpp"
rg -q 'A large stack did not expose exactly one face and three shoulders' \
    "${native_dir}/tests/SpreadLayoutTest.cpp"
rg -q 'Cycling a two-card stack moved its fixed reference layout fan' \
    "${native_dir}/tests/SpreadLayoutTest.cpp"
rg -q 'Vertical stack cycling changed the horizontal group envelope' \
    "${native_dir}/tests/SpreadLayoutTest.cpp"
rg -q 'const bool activeStack = wasActive' "${effect_cpp}" "${card_cpp}"
rg -q 'm_workspace\.pageStack\(delta\)' "${effect_cpp}" "${card_cpp}"
rg -q 'SpreadModel::detachSelectedMember' \
    "${native_dir}/src/SpreadModel.cpp"
rg -q 'SpreadModel::restoreDetachedMember' \
    "${native_dir}/src/SpreadModel.cpp"
rg -q 'makeInsertionStackPose' \
    "${native_dir}/src/SpreadLayout.cpp" "${effect_cpp}" "${card_cpp}"
rg -q 'pageCardStackInsertion' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'An explicit insertion seam did not place the carried card in order' \
    "${native_dir}/tests/SpreadModelTest.cpp"
rg -q 'The source stack did not remain centered beneath its lifted member' \
    "${native_dir}/tests/SpreadModelTest.cpp"
rg -q 'if \(!pose\.visible\)' "${native_dir}/src/SpreadLayout.cpp"
rg -q 'if \(!paintPose\.visible\)' "${effect_cpp}" "${card_cpp}"
rg -q 'Stack commit duplicated or lost a live card' \
    "${native_dir}/tests/SpreadModelTest.cpp"
rg -q 'Spread imposed a four- or five-card stack limit' \
    "${native_dir}/tests/SpreadModelTest.cpp"
rg -q 'classifyCardEdge' "${native_dir}/src/SpreadLayout.cpp" \
    "${router_cpp}"
rg -q 'A destination-card hover incorrectly requested paging' \
    "${native_dir}/tests/SpreadLayoutTest.cpp"
rg -q 'An end seam is not a' \
    "${router_cpp}"
rg -q 'm_edgePageTimer\.start\(m_edgePageDelay\)' "${router_cpp}"
rg -q 'm_workspace\.removeAt' "${card_cpp}"
# A tilted card is bounded by its rotated shader aperture plus the output
# fence, never an unrotated per-card bottom cutoff.
if rg -q 'fanBaseline.setBottom' "${effect_cpp}"; then
    echo 'Tilted card has an axis-aligned bottom cutoff' >&2; exit 1
fi
rg -q 'Qt::KeepAspectRatio\);' "${effect_cpp}"
rg -q 'paintCardSurface' "${effect_cpp}"
rg -q 'class Effect final : public KWin::OffscreenEffect' "${effect_header}" "${card_header}"
rg -q 'generateCustomShader' "${effect_cpp}" "${card_cpp}"
rg -q 'fwidth\(distanceToEdge\)' "${effect_cpp}" "${card_cpp}"
rg -q 'tex \*= coverage' "${effect_cpp}" "${card_cpp}"
rg -q 'window == m_fanApertureWindow' "${effect_cpp}" "${card_cpp}"
rg -q 'unredirect\(window\)' "${effect_cpp}" "${card_cpp}"
rg -q 'm_fanApertureShader \? "enabled" : "r20 fallback"' "${effect_cpp}" "${card_cpp}"
# Fixed hashes protect output-local dock-clearance geometry; visual alignment
# must not change this behavior.
test "$(sha256sum "${native_dir}/src/SpreadLayout.cpp" | cut -d' ' -f1)" = \
    "618f54ba01bc41e1da8e1f091f85212a861feac0e9b84123080fe8b4f6e8181d"
test "$(sha256sum "${native_dir}/src/SpreadLayout.h" | cut -d' ' -f1)" = \
    "bebcd0302985c6dd4092bc5c207a0d7cfb465765aac79dc6f71a6a48ce7a83d4"
# Explicit insertion-selection policy is covered across all slots/active members
# in SpreadModelTest; fixed aperture/layout hashes above remain unchanged.
test "$(sha256sum "${native_dir}/src/SpreadModel.cpp" | cut -d' ' -f1)" = \
    "9cc0d5d4463c00265fb947337a35f7fcdf70b3ae0d08283b565cc9294143c15b"
test "$(sha256sum "${native_dir}/src/SpreadModel.h" | cut -d' ' -f1)" = \
    "e1a6700a718bbb90083874cd7811b139f8419f2ab175d11e89f5ec23c172d75b"
rg -q 'appendCenteredCard' "${native_dir}/src/CardWorkspaceState.h"
rg -q 'window == m_arrivalWindow' "${card_cpp}"
rg -q 'ArrivalExpandDuration = 220' "${card_cpp}"
rg -q 'm_arrivalTimer.stop\(\)' "${card_cpp}"
rg -q 'm_launcherGuestPrimaryWindow' "${card_cpp}"
rg -q 'm_launcherGuestSecondaryWindow' "${card_cpp}"
rg -q 'captureCardTransition\(replacesGuest\)' "${card_cpp}"
rg -q 'setPairNeighborSide\(m_launcherGuestPrimarySide\)' "${card_cpp}"
completion=$(sed -n '/^bool Effect::completeLauncherGuestForWindow(/,/^}/p' "${effect_cpp}")
arrival_line=$(printf '%s\n' "$completion" | rg -n 'stageWindowArrival\(window\)' | cut -d: -f1)
ready_line=$(printf '%s\n' "$completion" | rg -n 'QDBusMessage ready' | cut -d: -f1)
test -n "$arrival_line" && test "$arrival_line" -lt "$ready_line"
rg -q 'makeFocusedPairLayout' "${card_cpp}"
rg -q 'm_workspace.count\(\) == 2' "${card_cpp}"
rg -q 'm_cardStage->previewTargetForWindow' "${effect_cpp}"
rg -q 'PreviewTransitionDuration = 280' "${card_cpp}"
rg -q 'isPanelPoint' "${router_cpp}" "${effect_cpp}"
sed -n '/^bool Effect::isPanelPoint(/,/^}/p' "${effect_cpp}" | rg -q 'window->isDock\(\) \|\| window->isAppletPopup\(\)'
rg -q 'Held touch blocked tray popup release' "${native_dir}/tests/PanelInputTest.cpp"
rg -q 'window->window\(\)->hitTest\(position\)' "${effect_cpp}"
rg -q 'm_panelPointerButtons' "${router_cpp}"
rg -q 'add_test\(NAME panel-input' "${native_dir}/CMakeLists.txt"
if rg -q 'm_cardGrabDirection|finalSlot - slot|travel \* 0\.78' \
    "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"; then
    echo "Ordinary carried-card travel must not move the destination row" >&2
    exit 1
fi
rg -q 'Wrapped reorder did not cross the Spread seam cleanly' \
    "${native_dir}/tests/SpreadModelTest.cpp"
rg -q 'classifySpreadGesture' "${router_cpp}"
rg -q 'event->deltaV120' "${router_cpp}"
rg -q 'activateSelectedFromInput' "${router_cpp}" "${effect_cpp}" "${card_cpp}"
rg -q 'm_ownedTouchIds' "${router_cpp}"
rg -q 'makeCoverPaintRect' "${native_dir}/src/SpreadLayout.cpp"
rg -q 'makeBentoCompositeGeometry' "${native_dir}/src/BentoCompositeGeometry.h" "${effect_cpp}"
rg -q 'BentoWorkspaceTintOpacity = 0\.22F' "${effect_cpp}"
rg -q 'bentoProjectionWorkspace' "${effect_cpp}" "${card_cpp}" "${card_header}"
rg -q 'bentoProjectionRect' "${effect_cpp}" "${card_cpp}" "${card_header}"
rg -q 'makeBentoProjectedPaneGeometry' \
    "${native_dir}/src/BentoCompositeGeometry.h" "${effect_cpp}"
rg -Fq 'mapBentoCompositeRect(composite, frame)' \
    "${native_dir}/src/BentoCompositeGeometry.h"
rg -q 'scaleBentoCompositeRadius' \
    "${native_dir}/src/BentoCompositeGeometry.h" "${effect_cpp}" \
    "${native_dir}/tests/BentoCompositeGeometryTest.cpp"
rg -Fq '? projectionPaneClip : visualTarget' "${effect_cpp}"
rg -Fq 'mapToDeviceCoordinatesAligned(apertureTarget)' "${effect_cpp}"
rg -Fq 'roundedClip(deviceAperture, apertureRadius)' "${effect_cpp}"
rg -Fq 'm_fanApertureSize = useFanAperture' "${effect_cpp}"
rg -Fq '? QSizeF(deviceAperture.size())' "${effect_cpp}"
rg -q 'retireBentoProjectionForCardStage' \
    "${effect_cpp}" "${effect_header}" "${card_cpp}" "${card_header}"
rg -Fq 'workspaceArea = workspaceArea(output)' "${desktop_cpp}"
rg -q 'const bool refreshableFirstEdge' "${desktop_cpp}"
rg -q 'drop.intent == CardDropIntent::ActivateBento' "${desktop_cpp}"
rg -q 'usesBentoProjectionAperture' "${card_cpp}" "${effect_cpp}"
rg -q 'workHeight \* 0\.54' "${native_dir}/src/SpreadLayout.cpp"
rg -q 'workWidth \* 0\.056' "${native_dir}/src/SpreadLayout.cpp"
rg -q 'restoreActiveSnapshot' "${effect_cpp}" "${card_cpp}"
rg -q 'setQuickTileMode\(KWin::QuickTileMode\{\}' "${effect_cpp}" "${card_cpp}"
rg -q 'activateWindow' "${effect_cpp}" "${card_cpp}"

# The accepted Spread paint path remains compositor-only. Physical writes
# are isolated to Active, monitor Bento, cross-output handoff, and their
# matching restore paths.
rg -q 'handleActiveGeometryChanged' "${effect_cpp}" "${card_cpp}"
rg -q 'handleSessionStateChanged' "${effect_cpp}" "${card_cpp}"
rg -q 'workspace\(\)->raiseWindow' "${effect_cpp}" "${card_cpp}"
rg -q 'workspace\(\)->activateWindow' "${effect_cpp}" "${card_cpp}"
rg -q 'EffectsHandler::windowActivated' "${effect_cpp}"
rg -q 'handleWindowActivated' "${effect_cpp}" "${effect_header}" \
    "${card_cpp}" "${card_header}"
rg -q 'promoted externally activated card' "${card_cpp}"
rg -Fq 'std::clamp(requestedGutter, 6.0, 48.0)' \
    "${native_dir}/src/SpreadLayout.cpp"
if rg -q 'renderSyntheticLine|SYNTHETIC SPREAD|GLTexture' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"; then
    echo "Synthetic cards must never appear in the live effect" >&2
    exit 1
fi
rg -q 'visibleNeighborhood' "${native_dir}/src/SpreadModel.cpp"
rg -q '10000' "${native_dir}/tests/SpreadModelTest.cpp"
rg -q 'makeSpreadLayout' "${effect_cpp}" "${card_cpp}"
rg -q 'Left neighbor is not a partial edge card' \
    "${native_dir}/tests/SpreadLayoutTest.cpp"
rg -q 'Right neighbor is not a partial edge card' \
    "${native_dir}/tests/SpreadLayoutTest.cpp"
rg -q 'native_plugin_system_target="/usr/lib/qt6/plugins/kwin/effects/plugins/kwin4_effect_kadunce\.so"' \
    "${project_dir}/install.sh"
rg -q 'pkexec /usr/bin/install -Dm755' "${project_dir}/install.sh"

# Output-local Bento/handoff is an explicit, reversible geometry transaction.
# Keep that ownership and restore boundary auditable while the frozen hashes
# above prove the tablet renderer did not change.
rg -q 'void Effect::toggleBento' "${effect_cpp}" "${card_cpp}"
rg -q 'bool Effect::finishCardGrabOnOutput' "${effect_cpp}" "${card_cpp}"
rg -q 'class DesktopStageController final' "${desktop_header}"
rg -q 'class DesktopStageHost' "${desktop_header}"
rg -q 'void DesktopStageController::restoreSession' "${desktop_cpp}"
rg -q 'replacementOutput' "${desktop_cpp}"
rg -q 'void Effect::handleScreenRemoved' "${effect_cpp}" "${card_cpp}"
rg -q 'void DesktopStageController::adjustRail' "${desktop_cpp}"
rg -q 'outputStageState' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'toggleBentoOnOutput' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'handoffBentoLeadToOutput' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'std::make_unique<DesktopStageController>' "${effect_cpp}" "${card_cpp}"
rg -q 'allowsDesktopStageOnOutput' \
    "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}" "${desktop_cpp}" "${desktop_header}"
rg -q 'activatePreparedTabletDrop' "${effect_cpp}" "${desktop_cpp}"
rg -q 'prepareOutputForDesktopStage' \
    "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}" "${desktop_cpp}" "${desktop_header}"
rg -q 'hasSessionOnOutput\(tablet->name\(\)\)' "${effect_cpp}" "${card_cpp}"
rg -q 'makePixelBentoLayout' "${native_dir}/src/BentoLayout.cpp" "${desktop_cpp}"
rg -q 'chooseBentoAdmission' "${native_dir}/src/BentoLayout.cpp" "${desktop_cpp}"
rg -q 'Bento layout/admission checks passed' \
    "${native_dir}/tests/BentoLayoutTest.cpp"
if rg -q 'BentoSession|BentoRestoreSnapshot|m_bentoSessions|m_bentoSettle' \
        "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"; then
    echo "Desktop Stage session state must not leak back into Effect" >&2
    exit 1
fi
if rg -q 'Effect \*|m_effect' \
        "${desktop_cpp}" "${desktop_header}" "${effect_header}" "${card_header}"; then
    echo "DesktopStageController must depend only on its typed host" >&2
    exit 1
fi
if rg -q 'friend class DesktopStageController' "${effect_header}" "${card_header}"; then
    echo "DesktopStageController must not access another owner's internals" >&2
    exit 1
fi
if rg -q '\bmoveWindow\(|windowToScreen\(|frameGeometry\s*=' "${native_dir}"; then
    echo "Bento must use the current native KWin transaction API only" >&2
    exit 1
fi

echo "Kadunce source checks passed; controller boundaries, standard layout and focused-pair guards are intact"
