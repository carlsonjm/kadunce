#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
native_dir="${project_dir}/native"
effect_cpp="${native_dir}/src/Effect.cpp"
effect_header="${native_dir}/src/Effect.h"
card_cpp="${native_dir}/src/CardStageController.cpp"
card_header="${native_dir}/src/CardStageController.h"
router_cpp="${native_dir}/src/WorkspaceInputRouter.cpp"
router_header="${native_dir}/src/WorkspaceInputRouter.h"
desktop_cpp="${native_dir}/src/DesktopStageController.cpp"
desktop_header="${native_dir}/src/DesktopStageController.h"
metadata_file="${native_dir}/src/metadata.json"
install_script="${project_dir}/install.sh"

python3 -m json.tool "${metadata_file}" >/dev/null
bash -n "${install_script}"

retired_brand='web''os|pa''lm|ghostie''post|chrome''os'
if rg -ni "${retired_brand}" "${project_dir}" \
        --glob '!.git/**'; then
    echo "Kadunce contains retired product or development-machine branding" >&2
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
rg -q 'systemctl --user enable kadunce-control\.service' \
    "${install_script}"
rg -q 'systemctl --user start kadunce-control\.service' \
    "${install_script}"

rg -q '"Id": "kwin4_effect_kadunce"' "${metadata_file}"
rg -q 'kwin4_effect_kadunce' "${native_dir}/CMakeLists.txt"
rg -q '0\.1\.0-kadunce-baseline' "${effect_cpp}" "${card_cpp}"
rg -q 'windowStepUserMovedResized' "${effect_cpp}" "${card_cpp}"
rg -q 'armed Bento drop across output seam' "${desktop_cpp}"
rg -q '!m_pointerPressed && !m_pointerPassthrough' "${router_cpp}"
rg -q 'm_cardLine\.stackSizeForId\(selectedId\) > 1' "${effect_cpp}" "${card_cpp}"
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
rg -q 'return 2;' "${effect_cpp}"
rg -q 'QDBusServiceWatcher::WatchForUnregistration' "${effect_cpp}"
rg -q 'm_cardStage->beginLauncherGuest' "${effect_cpp}"
rg -q 'm_cardStage->launcherGuestTarget' "${effect_cpp}" "${card_cpp}"
rg -q 'launcherGuestTargetForSlot' "${effect_cpp}" "${card_cpp}" "${card_header}"
rg -q 'work\.width\(\) \* 0\.03' "${card_cpp}"
rg -q 'm_launcherGuestLaunchPending' "${effect_cpp}" "${effect_header}"
rg -q 'QStringLiteral\("completeGuestLaunch"\)' "${effect_cpp}"
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
rg -q 'm_cardLine\.appendCard' "${effect_cpp}" "${card_cpp}"
rg -q 'const double originX' "${effect_cpp}" "${card_cpp}"
rg -q 'const double originY' "${effect_cpp}" "${card_cpp}"
rg -q 'QRegion remains only the hard output fence' "${effect_cpp}" "${card_cpp}"
rg -q 'Ctrl\+S' "${effect_cpp}" "${card_cpp}"
rg -q 'Ctrl\+Esc' "${effect_cpp}" "${card_cpp}"
rg -q 'Ctrl\+Left' "${effect_cpp}" "${card_cpp}"
rg -q 'Ctrl\+Right' "${effect_cpp}" "${card_cpp}"
rg -q 'paintScreen' "${effect_cpp}" "${card_cpp}"
rg -q 'm_paintingOutput != tablet' "${effect_cpp}" "${card_cpp}"
rg -q 'window->screen\(\) != tablet' "${effect_cpp}" "${card_cpp}"
rg -q 'blocksDirectScanout' "${effect_header}" "${card_header}"
rg -q 'QList<QPointer<KWin::EffectWindow>> m_liveCards' "${effect_header}" "${card_header}"
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
rg -q 'texcoord0 \* paintSize - apertureOrigin' "${effect_cpp}" "${card_cpp}"
rg -q 'OffscreenEffect snapshots expandedGeometry' "${effect_cpp}" "${card_cpp}"
rg -q 'mapToDeviceCoordinatesAligned\(paintRegion\)' "${effect_cpp}" "${card_cpp}"
rg -q 'deviceTarget\.x\(\) - devicePaint\.x\(\)' "${effect_cpp}" "${card_cpp}"
rg -q 'stackPaintOrderForId' "${native_dir}/src/CardLineModel.cpp" \
    "${effect_cpp}" "${card_cpp}"
rg -q 'A large fan did not expose a deterministic back-to-front deck' \
    "${native_dir}/tests/CardLineModelTest.cpp"
rg -q 'restoreOriginalStackingOrder' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'mapToDeviceCoordinatesAligned\(target\)' "${effect_cpp}" "${card_cpp}"
rg -q 'deviceRegion & roundedClip' "${effect_cpp}" "${card_cpp}"
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
if rg -q 'setCardLineInputActive|CardLineInputFilter' \
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
rg -q 'const CardLineModel &cardLine = m_cardStage->model\(\)' "${effect_cpp}"
rg -q 'm_host->admitCardToDesktopStage' "${card_cpp}"
rg -q 'm_cardStage->handleWindowAdded' "${effect_cpp}"
if rg -q '\bm_cardLine\b|\bm_liveCards\b|\bm_originalCardStackingOrder\b|\bm_activeRestore\b|\bm_presentation\b|\bm_cardGrabOffset\b|\bm_cardStackPreviewTarget\b|\bm_cardStackInsertionIndex\b' \
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
rg -q 'cancelInteractiveMoveResize' "${effect_cpp}" "${card_cpp}"
rg -q 'constexpr double CardHoldMotion = 12\.0' "${router_cpp}"
rg -q 'constexpr int CardHoldDelay = 300' "${router_cpp}"
rg -q 'm_holdTimer\.setSingleShot\(true\)' "${router_cpp}"
rg -q 'beginCardGrab' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'updateCardGrab' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'finishCardGrab' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'setElevatedWindow\(selectedWindow\(\), true\)' "${effect_cpp}" "${card_cpp}"
rg -q 'm_cardStage->cardGrabOffset\(\)' "${effect_cpp}"
rg -q 'target\.adjust\(insetX, insetY, -insetX, -insetY\)' "${effect_cpp}" "${card_cpp}"
rg -q 'm_cardLine\.moveSelected\(movement\)' "${effect_cpp}" "${card_cpp}"
rg -q 'void CardLineModel::moveSelected' \
    "${native_dir}/src/CardLineModel.cpp"
rg -q 'CardLineModel::detachedNeighborhood' \
    "${native_dir}/src/CardLineModel.cpp"
rg -q 'm_cardLine\.detachedNeighborhood\(m_cardGrabPageOffset\)' \
    "${effect_cpp}" "${card_cpp}"
rg -q 'Three-card edge page did not remain deterministic' \
    "${native_dir}/tests/CardLineModelTest.cpp"
rg -q 'constexpr double CardEdgeZoneFraction = 0\.08' "${router_cpp}"
rg -q 'constexpr double CardEdgeZoneMinimum = 72\.0' "${router_cpp}"
rg -q 'constexpr int CardEdgeDwellDelay = 300' "${router_cpp}"
rg -q 'constexpr int CardEdgeRepeatDelay = 350' "${router_cpp}"
rg -q 'm_edgePageTimer\.start\(CardEdgeDwellDelay\)' "${router_cpp}"
rg -q 'm_edgePageTimer\.start\(CardEdgeRepeatDelay\)' "${router_cpp}"
rg -q 'pageCardGrab' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'constexpr int CardStackDwellDelay = 350' "${router_cpp}"
rg -q 'constexpr int CardStackTransitionDuration = 350' "${effect_cpp}" "${card_cpp}"
rg -q 'QEasingCurve::InQuart' "${effect_cpp}" "${card_cpp}"
rg -q 'CardLineModel::stackSelectedWith' \
    "${native_dir}/src/CardLineModel.cpp"
rg -q 'makeOpenStackPose' "${native_dir}/src/CardLineLayout.cpp" "${effect_cpp}" "${card_cpp}"
rg -q 'makeClosedStackPose' "${native_dir}/src/CardLineLayout.cpp" "${effect_cpp}" "${card_cpp}"
rg -q 'StackGroupingDistance = 0\.24' \
    "${native_dir}/src/CardLineLayout.cpp"
rg -q 'StackClosedStep = 7\.0' \
    "${native_dir}/src/CardLineLayout.cpp"
rg -q 'StackRotationFactor = 120\.0' \
    "${native_dir}/src/CardLineLayout.cpp"
rg -q 'StackFaceRotation = 0\.6' \
    "${native_dir}/src/CardLineLayout.cpp"
rg -q 'VisualRotations\{0\.2, -0\.2, -0\.4\}' \
    "${native_dir}/src/CardLineLayout.cpp"
rg -q 'The bottom reference layout shoulder did not retain its slight upward tilt' \
    "${native_dir}/tests/CardLineLayoutTest.cpp"
rg -q 'setRotationAngle\(paintPose\.rotation\)' "${effect_cpp}" "${card_cpp}"
rg -q 'makeOpenStackEnvelope' \
    "${native_dir}/src/CardLineLayout.cpp" "${effect_cpp}" "${card_cpp}"
rg -q 'makeReservedCardTarget' \
    "${native_dir}/src/CardLineLayout.cpp" "${effect_cpp}" "${card_cpp}"
rg -q 'bottomRightRotationEnvelope' \
    "${native_dir}/src/CardLineLayout.cpp"
rg -q 'const double groupCentering' \
    "${native_dir}/src/CardLineLayout.cpp"
rg -q 'Left neighbor did not preserve the gutter around the stack' \
    "${native_dir}/tests/CardLineLayoutTest.cpp"
rg -q 'A large stack hid cards from vertical member paging' \
    "${native_dir}/tests/CardLineModelTest.cpp"
rg -q 'void CardLineModel::pageStack' \
    "${native_dir}/src/CardLineModel.cpp"
rg -q 'Horizontal paging did not treat a stack as one group' \
    "${native_dir}/tests/CardLineModelTest.cpp"
rg -q 'QKeySequence\(QStringLiteral\("Ctrl\+Up"\)\)' "${effect_cpp}" "${card_cpp}"
rg -q 'QKeySequence\(QStringLiteral\("Ctrl\+Down"\)\)' "${effect_cpp}" "${card_cpp}"
rg -q 'selectedStackContains' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'classifyStackGesture' \
    "${native_dir}/src/CardLineLayout.cpp" "${router_cpp}"
rg -q 'Vertical motion outside a stack changed its member' \
    "${native_dir}/tests/CardLineLayoutTest.cpp"
rg -q 'A large stack did not expose exactly one face and three shoulders' \
    "${native_dir}/tests/CardLineLayoutTest.cpp"
rg -q 'Cycling a two-card stack moved its fixed reference layout fan' \
    "${native_dir}/tests/CardLineLayoutTest.cpp"
rg -q 'Vertical stack cycling changed the horizontal group envelope' \
    "${native_dir}/tests/CardLineLayoutTest.cpp"
rg -q 'const bool activeStack = wasActive' "${effect_cpp}" "${card_cpp}"
rg -q 'm_cardLine\.pageStack\(delta\)' "${effect_cpp}" "${card_cpp}"
rg -q 'CardLineModel::detachSelectedMember' \
    "${native_dir}/src/CardLineModel.cpp"
rg -q 'CardLineModel::restoreDetachedMember' \
    "${native_dir}/src/CardLineModel.cpp"
rg -q 'makeInsertionStackPose' \
    "${native_dir}/src/CardLineLayout.cpp" "${effect_cpp}" "${card_cpp}"
rg -q 'pageCardStackInsertion' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"
rg -q 'An explicit insertion seam did not place the carried card in order' \
    "${native_dir}/tests/CardLineModelTest.cpp"
rg -q 'The source stack did not remain centered beneath its lifted member' \
    "${native_dir}/tests/CardLineModelTest.cpp"
rg -q 'if \(!pose\.visible\)' "${native_dir}/src/CardLineLayout.cpp"
rg -q 'if \(!paintPose\.visible\)' "${effect_cpp}" "${card_cpp}"
rg -q 'Stack commit duplicated or lost a live card' \
    "${native_dir}/tests/CardLineModelTest.cpp"
rg -q 'Card Line imposed a four- or five-card stack limit' \
    "${native_dir}/tests/CardLineModelTest.cpp"
rg -q 'classifyCardEdge' "${native_dir}/src/CardLineLayout.cpp" \
    "${router_cpp}"
rg -q 'A destination-card hover incorrectly requested paging' \
    "${native_dir}/tests/CardLineLayoutTest.cpp"
rg -q 'one extra outward dwell at the deck.s first or last seam' \
    "${router_cpp}"
rg -q 'm_edgePageTimer\.start\(CardEdgeRepeatDelay\)' "${router_cpp}"
rg -q 'm_cardLine\.removeCard' "${effect_cpp}" "${card_cpp}"
rg -q 'deviceRegion & KWin::Region\(fanBaseline\)' "${effect_cpp}" "${card_cpp}"
rg -q 'class Effect final : public KWin::OffscreenEffect' "${effect_header}" "${card_header}"
rg -q 'generateCustomShader' "${effect_cpp}" "${card_cpp}"
rg -q 'fwidth\(distanceToEdge\)' "${effect_cpp}" "${card_cpp}"
rg -q 'tex \*= coverage' "${effect_cpp}" "${card_cpp}"
rg -q 'window == m_fanApertureWindow' "${effect_cpp}" "${card_cpp}"
rg -q 'unredirect\(window\)' "${effect_cpp}" "${card_cpp}"
rg -q 'm_fanApertureShader \? "enabled" : "r20 fallback"' "${effect_cpp}" "${card_cpp}"
test "$(sha256sum "${native_dir}/src/CardLineLayout.cpp" | cut -d' ' -f1)" = \
    "8bce37048d28e7d79d03732ec774a190a46fd7f7e248aeb443a3da1cb360a461"
test "$(sha256sum "${native_dir}/src/CardLineLayout.h" | cut -d' ' -f1)" = \
    "66bb94d8c1c671672d5cc5cf839e6f5e36bf8bb85ca96df8c228ba114b828452"
test "$(sha256sum "${native_dir}/src/CardLineModel.cpp" | cut -d' ' -f1)" = \
    "de45d07717b0185013a22ecc6cc29878aa24c00feea86170751ccb907cac233e"
test "$(sha256sum "${native_dir}/src/CardLineModel.h" | cut -d' ' -f1)" = \
    "60bb357e1e3a3f5823a117176802a5176c228097e8094f662f9d01bae971a43a"
if rg -q 'm_cardGrabDirection|finalSlot - slot|travel \* 0\.78' \
    "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"; then
    echo "Ordinary carried-card travel must not move the destination row" >&2
    exit 1
fi
rg -q 'Wrapped reorder did not cross the Card Line seam cleanly' \
    "${native_dir}/tests/CardLineModelTest.cpp"
rg -q 'classifyCardLineGesture' "${router_cpp}"
rg -q 'event->deltaV120' "${router_cpp}"
rg -q 'activateSelectedFromInput' "${router_cpp}" "${effect_cpp}" "${card_cpp}"
rg -q 'm_ownedTouchIds' "${router_cpp}"
rg -q 'makeCoverPaintRect' "${native_dir}/src/CardLineLayout.cpp"
rg -q 'workHeight \* 0\.54' "${native_dir}/src/CardLineLayout.cpp"
rg -q 'workWidth \* 0\.056' "${native_dir}/src/CardLineLayout.cpp"
rg -q 'restoreActiveSnapshot' "${effect_cpp}" "${card_cpp}"
rg -q 'setQuickTileMode\(KWin::QuickTileMode\{\}' "${effect_cpp}" "${card_cpp}"
rg -q 'activateWindow' "${effect_cpp}" "${card_cpp}"

# The accepted Card Line paint path remains compositor-only. Physical writes
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
rg -q 'constexpr double gutter = 10\.0' \
    "${native_dir}/src/CardLineLayout.cpp"
if rg -q 'renderSyntheticLine|SYNTHETIC CARD LINE|GLTexture' "${effect_cpp}" "${card_cpp}" "${effect_header}" "${card_header}"; then
    echo "Synthetic cards must never appear in the live effect" >&2
    exit 1
fi
rg -q 'visibleNeighborhood' "${native_dir}/src/CardLineModel.cpp"
rg -q '10000' "${native_dir}/tests/CardLineModelTest.cpp"
rg -q 'makeCardLineLayout' "${effect_cpp}" "${card_cpp}"
rg -q 'Left neighbor is not a partial edge card' \
    "${native_dir}/tests/CardLineLayoutTest.cpp"
rg -q 'Right neighbor is not a partial edge card' \
    "${native_dir}/tests/CardLineLayoutTest.cpp"
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
rg -q 'std::none_of' "${effect_cpp}" "${card_cpp}"
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
if rg -q 'Effect \*|m_effect|friend class DesktopStageController' \
        "${desktop_cpp}" "${desktop_header}" "${effect_header}" "${card_header}"; then
    echo "DesktopStageController must depend only on its typed host" >&2
    exit 1
fi
if rg -q '\bmoveWindow\(|windowToScreen\(|frameGeometry\s*=' "${native_dir}"; then
    echo "Bento must use the current native KWin transaction API only" >&2
    exit 1
fi

echo "Kadunce source checks passed; controller boundaries and frozen r21 card core are intact"
