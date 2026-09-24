/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CardStageController.h"
#include "KeyboardReveal.h"
#include "HeldCardGeometry.h"
#include "OwnershipHandoff.h"
#include "NeighborStackPose.h"
#include "RowPageMotion.h"
#include "StackBrowseMotion.h"
#include "SpreadLayout.h"
#include "FocusedPairLayout.h"
#include "WindowStateRestore.h"

#include <core/output.h>
#include <effect/effecthandler.h>
#include <window.h>
#include <workspace.h>

#include <QDebug>
#include <QEasingCurve>
#include <QScopedValueRollback>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace Kadunce
{

namespace
{
constexpr auto Revision = "0.1.0-kadunce-baseline";
constexpr int CardStackTransitionDuration = 350;
constexpr int PreviewTransitionDuration = 280;
constexpr int ArrivalExpandDuration = 220;
constexpr double LauncherGuestCommitDistance = 58.0;
constexpr int LauncherGuestTransitionDuration = 220;
constexpr double LauncherGuestDragPreview = 0.18;
// CARD-LIFECYCLE.md §9: how far a stacked card rises before it has left the
// stack. Held-card height rather than a pixel count, so the gesture is the same
// proportion of the card on every display, and far enough above the 48px slop a
// browse gesture carries that a card cannot leave a stack by being nudged.
constexpr double StackReleaseRise = 0.30;
}

CardStageController::CardStageController(CardStageHost *host)
    : m_host(host)
{
    m_arrivalTimer.setSingleShot(true);
    QObject::connect(&m_arrivalTimer, &QTimer::timeout, &m_arrivalTimer, [this]() {
        const auto window = m_arrivalWindow;
        if (!m_active || m_presentation != CardPresentation::Spread
            || !window || window->isDeleted() || selectedWindow() != window
            || !window->window() || m_launcherGuestActive || m_cardGrabActive
            || !m_host->tabletOutputForCardStage()
            || window->screen() != m_host->tabletOutputForCardStage()
            || window->window()->isInteractiveMove()
            || window->window()->isInteractiveResize()) {
            clearCardTransition();
            return;
        }
        if (!window->window()->readyForPainting() || !window->window()->isShown()) {
            if (m_arrivalWait.elapsed() < 10000) m_arrivalTimer.start(50);
            else clearCardTransition(); // Keep the overview; never fall back to the old app.
            return;
        }
        if (!m_arrivalExpanding) {
            captureCardTransition();
            m_arrivalExpanding = true;
            m_arrivalTimer.start(ArrivalExpandDuration);
        } else {
            clearCardTransition();
            (void)enterActive();
        }
        KWin::effects->addRepaintFull();
    });
    // Keyboard, focus and cursor notifications arrive in bursts and from
    // inside the compositor's own handling, so the reveal runs once, after.
    m_keyboardRevealTimer.setSingleShot(true);
    m_keyboardRevealTimer.setInterval(0);
    QObject::connect(&m_keyboardRevealTimer, &QTimer::timeout,
                     &m_keyboardRevealTimer, [this]() { updateKeyboardReveal(); });
    // The bottom panels return a moment after the keyboard leaves. Until then
    // the work area still includes their room, so the card keeps the
    // placement it had before the keyboard came.
    m_keyboardRevealRelease.setSingleShot(true);
    m_keyboardRevealRelease.setInterval(1000);
    QObject::connect(&m_keyboardRevealRelease, &QTimer::timeout,
                     &m_keyboardRevealRelease, [this]() { m_keyboardReveal.reset(); });
    m_activeSettleTimer.setSingleShot(true);
    m_activeSettleTimer.setInterval(0);
    QObject::connect(&m_activeSettleTimer, &QTimer::timeout, &m_activeSettleTimer, [this]() {
        const auto window = m_activeRestore.window;
        auto *tablet = m_host->tabletOutputForCardStage();
        if (!m_active || m_presentation != CardPresentation::Active
            || !m_activeRestore.valid || !window || window->isDeleted()
            || !window->window() || !tablet || m_activeSettleRemaining <= 0) return;
        auto *client = window->window();
        if (client->isInteractiveMove() || client->isInteractiveResize()
            || client->isFullScreen() || client->maximizeMode() != KWin::MaximizeRestore
            || client->quickTileMode() != KWin::QuickTileMode{}) return;
        const auto target = activePlacement(tablet);
        if (window->frameGeometry().toRect() == target) return;
        --m_activeSettleRemaining;
        QScopedValueRollback<bool> applying(m_applyingWindowState, true);
        client->moveResize(KWin::RectF(target));
        KWin::effects->addRepaintFull();
        qInfo() << "Kadunce Active source settle" << window->caption()
                << "target" << target << "remaining" << m_activeSettleRemaining;
    });
}

bool CardStageController::isActive() const
{
    return m_active;
}

CardPresentation CardStageController::presentation() const
{
    return m_presentation;
}

KWin::EffectWindow *CardStageController::activeCardIdentity() const
{
    // Only an individual card this stage still owns can be the Active card.
    return m_active && m_presentedActive && !m_presentedActive->isDeleted()
        && liveCardIndex(m_presentedActive) >= 0 ? m_presentedActive.data() : nullptr;
}

bool CardStageController::canOwnCards(const KWin::LogicalOutput *output) const
{
    // One workspace exists and it is bound to one output. PRODUCT-CONTRACT.md
    // gives an external output ordinary windows or per-output Bento, never
    // cards, so this is the product's shape rather than a temporary limit.
    return output && m_host->isTabletOutputForCardStage(output);
}

bool CardStageController::ownsDisplay(const KWin::LogicalOutput *output) const
{
    // CARD-LIFECYCLE.md §3: Kadunce owns a display from its first Card or Bento
    // action, whether what it holds is individual cards, stacks or a group.
    return m_active && canOwnCards(output) && !m_workspace.windows().isEmpty();
}

bool CardStageController::selectCardEntry(KWin::EffectWindow *window)
{
    // Selection is an entry index; membership is a card index. They coincide
    // only while every entry is standalone, so a stack or a Bento group makes
    // selecting by card index land on a different entry.
    const int cardId = liveCardIndex(window) + 1;
    if (cardId <= 0) return false;
    const int offset = spreadEntryOffset(m_workspace.model(), cardId);
    if (offset < 0) return false;
    m_workspace.page(offset);
    for (int step = 0; step < m_workspace.stackSizeForId(cardId)
         && m_workspace.selectedId() != cardId; ++step) {
        m_workspace.pageStack(1);
    }
    return m_workspace.selectedId() == cardId;
}

bool CardStageController::isEligiblePartner(const KWin::EffectWindow *window) const
{
    if (!m_active || !window || window->isDeleted() || !window->window()) return false;
    auto *output = m_host->tabletOutputForCardStage();
    // On the current display and current virtual desktop, owned as an
    // individual card, and awake. isManagedWindowForCardStage carries the
    // desktop and activity test and excludes a minimized window, so §7 keeps a
    // sleeping card from silently returning to Bento.
    if (!output || window->screen() != output
        || !m_host->isManagedWindowForCardStage(window)
        || liveCardIndex(window) < 0) return false;
    // The Bento group is never a partner, and neither is a live pane.
    return !usesBentoProjectionAperture(window) && !isBentoProjectionPane(window);
}

KWin::EffectWindow *CardStageController::partnerForSideSnap(
    const KWin::EffectWindow *carried, bool leftEdge) const
{
    if (!m_active || !carried || carried->isDeleted()) return nullptr;
    auto *active = activeCardIdentity();
    // §3: when the carried window is not the Active card, the Active card is
    // the partner. It qualifies whether it stands alone or is a stack's
    // selected member, because the user named it Active.
    if (active != carried) return isEligiblePartner(active) ? active : nullptr;
    // §3: the Active card was carried, so the partner is the nearest eligible
    // card on the contacted side of it. The walk begins at the entry holding
    // that card and is cyclic, stopping where it started.
    const int carriedId = liveCardIndex(carried) + 1;
    if (carriedId <= 0) return nullptr;
    const int direction = leftEdge ? -1 : 1;
    for (int step = 1; step < m_workspace.count(); ++step) {
        const int face = m_workspace.faceAtStepsFrom(carriedId, direction * step);
        if (face <= 0 || face == carriedId) break;
        // §4: a search passes over every ordinary stack, so no walk breaks a
        // composed group. A stacked card reaches Bento only as the Active card.
        if (m_workspace.stackSizeForId(face) > 1) continue;
        auto *candidate = m_workspace.windowForId(face).data();
        if (candidate && candidate != carried && isEligiblePartner(candidate))
            return candidate;
    }
    return nullptr;
}

const SpreadModel &CardStageController::model() const
{
    return m_workspace.model();
}

CardWorkspaceSnapshot CardStageController::workspaceSnapshot() const
{
    QStringList identities;
    identities.reserve(m_workspace.windows().size());
    for (const auto &window : m_workspace.windows())
        identities.append(window ? window->internalId().toString(QUuid::WithoutBraces) : QString());
    return makeCardWorkspaceSnapshot(m_workspace.model(), identities, m_active);
}

const QList<QPointer<KWin::EffectWindow>> &CardStageController::liveCards() const
{
    return m_workspace.windows();
}

KWin::EffectWindow *CardStageController::selectedWindow() const
{
    if (m_workspace.windows().isEmpty()) {
        return nullptr;
    }
    const int index = m_workspace.selectedId() - 1;
    return index >= 0 && index < m_workspace.windows().size()
        ? m_workspace.windows().at(index).data() : nullptr;
}

int CardStageController::liveCardIndex(const KWin::EffectWindow *window) const
{
    for (int index = 0; index < m_workspace.windows().size(); ++index) {
        if (m_workspace.windows().at(index) == window) {
            return index;
        }
    }
    return -1;
}

bool CardStageController::usesBentoProjectionAperture(
    const KWin::EffectWindow *window) const
{
    return window && std::any_of(
        m_bentoProjectionWindows.cbegin(), m_bentoProjectionWindows.cend(),
        [window](const auto &projected) { return projected == window; });
}

bool CardStageController::isBentoProjectionPane(
    const KWin::EffectWindow *window) const
{
    return window && std::any_of(
        m_bentoProjectionPaneWindows.cbegin(),
        m_bentoProjectionPaneWindows.cend(),
        [window](const auto &pane) { return pane == window; });
}

bool CardStageController::selectedIsBentoProjection() const
{
    return m_bentoProjectionSession && selectedWindow()
        && usesBentoProjectionAperture(selectedWindow());
}

// Whether the user has the Bento group selected, which is not the same question
// as whether the selected window is itself a painted pane. The group is admitted
// as one stack holding its panes and the windows retained beside them, so paging
// inside that stack, or a card inserted into it, moves the selection off a pane
// without moving it off the group. Resume asks this; the callers that suppress
// per-card behavior keep asking the narrower question.
bool CardStageController::selectedIsBentoGroup() const
{
    if (!m_bentoProjectionSession || !selectedWindow()) return false;
    if (usesBentoProjectionAperture(selectedWindow())) return true;
    const int selected = m_workspace.selectedId();
    for (const auto &pane : m_bentoProjectionPaneWindows) {
        if (!pane) continue;
        const int id = m_workspace.indexOf(pane) + 1;
        if (id > 0 && m_workspace.sameStack(id, selected)) return true;
    }
    return false;
}

QList<QPointer<KWin::EffectWindow>> CardStageController::bentoProjectionPanes() const
{
    return m_bentoProjectionPaneWindows;
}

KWin::Rect CardStageController::bentoProjectionWorkspace() const
{
    return m_bentoProjectionSession ? m_bentoProjectionSession->workspaceArea
                                    : KWin::Rect();
}

std::optional<BentoRect> CardStageController::bentoProjectionRect(
    const KWin::EffectWindow *window) const
{
    if (!window || !m_bentoProjectionSession) return std::nullopt;
    const auto member = std::find_if(m_bentoProjectionSession->panes.cbegin(),
        m_bentoProjectionSession->panes.cend(), [window](const auto &member) {
            return member.window == window;
        });
    if (member == m_bentoProjectionSession->panes.cend()) return std::nullopt;
    const auto index = std::distance(m_bentoProjectionSession->panes.cbegin(), member);
    if (index < 0 || std::size_t(index) >= m_bentoProjectionSession->rects.size())
        return std::nullopt;
    return m_bentoProjectionSession->rects[std::size_t(index)];
}

QList<QPointer<KWin::EffectWindow>> CardStageController::preparationNeighbors() const
{
    QList<QPointer<KWin::EffectWindow>> result;
    if (!m_active || m_workspace.count() == 0
        || m_presentation != CardPresentation::Spread || m_launcherGuestActive) return result;
    for (int side : {-1, 1}) {
        const int id = m_cardGrabActive
            ? m_workspace.detachedNeighborhood(m_cardGrabPageOffset + side)[side < 0 ? 0 : 2]
            : m_workspace.idAtOffset(side * 2);
        const auto window = m_workspace.windows().value(id - 1);
        if (window && !window->isDeleted() && visibleSlot(window) == 99 && !result.contains(window))
            result.append(window);
    }
    return result;
}

int CardStageController::visibleSlot(const KWin::EffectWindow *window) const
{
    const int index = liveCardIndex(window);
    if (index < 0 || m_workspace.windows().isEmpty()) {
        return 99;
    }
    const int cardId = index + 1;
    if (m_launcherGuestActive && !m_launcherGuestArrival
        && m_presentation == CardPresentation::Spread) {
        if (m_launcherGuestPrimaryWindow && m_workspace.sameStack(cardId,
                liveCardIndex(m_launcherGuestPrimaryWindow) + 1)) {
            return m_launcherGuestPrimarySide;
        }
        if (m_launcherGuestSecondaryWindow && m_workspace.sameStack(cardId,
                liveCardIndex(m_launcherGuestSecondaryWindow) + 1)) {
            return -m_launcherGuestPrimarySide;
        }
        return 99;
    }
    if (m_presentation == CardPresentation::Bento) {
        // The panes are the display. Individual cards stay owned and hidden.
        return 99;
    }
    if (m_presentation == CardPresentation::Active) {
        return cardId == m_workspace.selectedId() ? 0 : 99;
    }
    if (m_workspace.sameStack(cardId, m_workspace.selectedId())) {
        return 0;
    }
    if (m_cardGrabActive) {
        const std::array<int, 3> destinations =
            m_workspace.detachedNeighborhood(m_cardGrabPageOffset);
        for (int slot = -1; slot <= 1; ++slot) {
            const int destination =
                destinations[static_cast<std::size_t>(slot + 1)];
            if (destination != 0
                && m_workspace.sameStack(cardId, destination)) {
                return slot;
            }
        }
        return 99;
    }
    if (m_workspace.count() == 2) {
        return m_workspace.pairNeighborSide();
    }
    if (m_workspace.count() >= 3) {
        const std::array<int, 3> neighborhood =
            m_workspace.visibleNeighborhood();
        if (m_workspace.sameStack(cardId, neighborhood[0])) {
            return -1;
        }
        if (m_workspace.sameStack(cardId, neighborhood[2])) {
            return 1;
        }
    }
    return 99;
}

bool CardStageController::cardGrabActive() const
{
    return m_cardGrabActive;
}

QPointF CardStageController::cardGrabOffset() const
{
    return m_cardGrabOffset;
}

int CardStageController::cardGrabPageOffset() const
{
    return m_cardGrabPageOffset;
}

KWin::Rect CardStageController::cardGrabTarget() const
{
    if (!m_cardGrabActive || !m_cardGrabScaleTimer.isValid()) return m_cardGrabTarget;
    const double t = heldPickupProgress(m_cardGrabScaleTimer.elapsed());
    const auto rect = anchoredStackCarry(
        {double(m_cardGrabTarget.x()), double(m_cardGrabTarget.y()),
         double(m_cardGrabTarget.width()), double(m_cardGrabTarget.height())},
        m_cardGrabStart.x(), m_cardGrabStart.y(),
        m_cardGrabDestinationSize.width(), m_cardGrabDestinationSize.height(), t);
    return KWin::Rect(qRound(rect.x), qRound(rect.y), qRound(rect.width), qRound(rect.height));
}

int CardStageController::stackPreviewTarget() const
{
    return m_cardStackPreviewTarget;
}

bool CardStageController::stackPreviewArmed() const
{
    return m_cardStackPreviewArmed;
}

bool CardStageController::stackInsertionPreviewValid() const
{
    return m_cardGrabActive && m_cardStackPreviewArmed && m_stackInsertion
        && m_cardStackPreviewRevision == m_workspace.revision()
        && m_cardStackPreviewTarget != 0
        && m_cardStackPreviewTarget == cardStackCandidate();
}

int CardStageController::stackInsertionIndex() const
{
    return m_cardStackInsertionIndex;
}

KWin::Rect CardStageController::stackPlaceholderTarget() const
{
    auto *tablet = m_host->tabletOutputForCardStage();
    if (!tablet || !stackInsertionPreviewValid()) return {};
    // The insertion seam is the zero-offset pose in the opened fan. Keep this
    // virtual space fixed to the deck, never magnetize the real held window.
    return cardTargetForSlot(tablet, 0);
}

int CardStageController::previousStackInsertionIndex() const
{
    return m_cardStackPreviousInsertionIndex;
}

int CardStageController::stackBrowseTarget() const
{
    if (!m_cardGrabActive || m_workspace.count() < 2) {
        return 0;
    }
    const int destination = m_workspace.detachedNeighborhood(m_cardGrabPageOffset)[1];
    const auto destinationWindow = m_workspace.windows().value(destination - 1);
    return usesBentoProjectionAperture(destinationWindow) ? 0 : destination;
}

double CardStageController::stackInsertionBlend() const
{
    if (!m_cardStackInsertionTimer.isValid()) {
        return 1.0;
    }
    const double progress = std::clamp(
        static_cast<double>(m_cardStackInsertionTimer.elapsed())
            / CardStackTransitionDuration,
        0.0, 1.0);
    // Respond immediately to an accepted slot request rather than spending
    // half the transition almost stationary (InQuart is only6.25% at halfway).
    return QEasingCurve(QEasingCurve::OutCubic).valueForProgress(progress);
}

double CardStageController::stackPreviewBlend() const
{
    if (!m_cardStackPreviewTimer.isValid()) {
        return m_cardStackPreviewTo;
    }
    const double progress = std::clamp(
        static_cast<double>(m_cardStackPreviewTimer.elapsed())
            / CardStackTransitionDuration,
        0.0, 1.0);
    const double eased = QEasingCurve(QEasingCurve::InQuart)
        .valueForProgress(progress);
    return m_cardStackPreviewFrom
        + (m_cardStackPreviewTo - m_cardStackPreviewFrom) * eased;
}

bool CardStageController::animationsRunning() const
{
    return (m_cardGrabActive && m_cardGrabScaleTimer.isValid()
            && m_cardGrabScaleTimer.elapsed() < HeldPickupDuration)
        || (m_previewTransition.isValid()
            && m_previewTransition.elapsed() < (m_pickupTransition ? HeldPickupDuration : m_rowPageTransition ? RowPageDuration
                : m_stackBrowseDirection ? StackBrowseDuration : PreviewTransitionDuration))
        || (m_cardStackPreviewTimer.isValid()
            && m_cardStackPreviewTimer.elapsed()
                < CardStackTransitionDuration)
        || (m_cardStackInsertionTimer.isValid()
            && m_cardStackInsertionTimer.elapsed()
                < CardStackTransitionDuration)
        || (m_launcherGuestTransitionTimer.isValid()
            && m_launcherGuestTransitionTimer.elapsed()
                < LauncherGuestTransitionDuration);
}

bool CardStageController::launcherGuestActive() const
{
    return m_launcherGuestActive;
}

double CardStageController::launcherGuestOffset() const
{
    return m_launcherGuestOffset;
}

double CardStageController::launcherGuestTransitionProgress() const
{
    const double dragProgress = std::clamp(
        std::abs(m_launcherGuestOffset) / LauncherGuestCommitDistance,
        0.0, 1.0) * LauncherGuestDragPreview;
    if (!m_launcherGuestTransitionTimer.isValid()) {
        return dragProgress;
    }
    const double elapsed = std::clamp(
        static_cast<double>(m_launcherGuestTransitionTimer.elapsed())
            / LauncherGuestTransitionDuration,
        0.0, 1.0);
    const double eased = QEasingCurve(QEasingCurve::OutCubic)
        .valueForProgress(elapsed);
    return m_launcherGuestTransitionFrom
        + (1.0 - m_launcherGuestTransitionFrom) * eased;
}

KWin::Rect CardStageController::cardTargetForSlot(
    KWin::LogicalOutput *output, int slot) const
{
    const KWin::RectF work = KWin::effects->clientArea(
        KWin::MaximizeArea, output);
    const bool focusedPair = m_workspace.count() == 2
        && (!m_launcherGuestActive || m_launcherGuestArrival) && !m_cardGrabActive;
    const SpreadLayout layout = focusedPair
        ? makeFocusedPairLayout(work.x(), work.y(), work.width(), work.height())
        : makeSpreadLayout(work.x(), work.y(), work.width(), work.height());
    CardStackEnvelope envelope{0.0, 0.0};
    if (m_presentation == CardPresentation::Spread) {
        if (!m_cardGrabActive) {
            const int selectedId = m_workspace.selectedId();
            const int memberCount = m_workspace.stackSizeForId(selectedId);
            if (memberCount > 1 && !selectedIsBentoProjection()) {
                envelope = makeOpenStackEnvelope(
                    memberCount,
                    m_workspace.stackActivePositionForId(selectedId),
                    layout.cards[1].width, layout.cards[1].height);
            }
        } else {
            const int destinationId = stackBrowseTarget();
            const int destinationSize =
                m_workspace.stackSizeForId(destinationId);
            if (m_cardStackPreviewTarget != 0) {
                const auto opened = makeInsertionStackEnvelope(
                    destinationSize + 1,
                    layout.cards[1].width, layout.cards[1].height);
                const auto browsed = makeOpenStackEnvelope(destinationSize,
                    m_workspace.stackActivePositionForId(destinationId),
                    layout.cards[1].width, layout.cards[1].height);
                const double blend = stackPreviewBlend();
                envelope = {browsed.left + (opened.left - browsed.left) * blend,
                    browsed.right + (opened.right - browsed.right) * blend};
            } else if (destinationSize > 1) {
                envelope = makeOpenStackEnvelope(
                    destinationSize,
                    m_workspace.stackActivePositionForId(destinationId),
                    layout.cards[1].width, layout.cards[1].height);
            }
        }
    }
    const CardRect target = focusedPair
        ? makeReservedFocusedPairTarget(layout, slot, envelope)
        : makeReservedCardTarget(layout, slot, envelope);
    return KWin::Rect(qRound(target.x), qRound(target.y),
                      qRound(target.width), qRound(target.height));
}

void CardStageController::clearCardTransition()
{
    m_arrivalTimer.stop();
    m_arrivalWindow.clear();
    m_arrivalExpanding = false;
    m_previewTransition.invalidate();
    m_previewOrigins.clear();
    m_poseTransition = false;
    m_pickupTransition = false;
    m_rowPageTransition = false;
    m_stackBrowseDirection = 0;
    m_stackBrowseOutgoing.clear();
}

void CardStageController::anchorRowTransition()
{
    auto *output = m_host->tabletOutputForCardStage();
    if (!m_rowPageTransition || !output) return;
    const auto work = KWin::effects->clientArea(KWin::MaximizeArea, output);
    if (work.width() <= 0) return;
    const int centerId = m_cardGrabActive
        ? m_workspace.detachedNeighborhood(m_cardGrabPageOffset)[1] : m_workspace.selectedId();
    const auto center = m_workspace.windows().value(centerId - 1);
    const auto target = cardTargetForSlot(output, 0);
    const auto pose = stackPoseForWindow(center, target.width());
    m_rowDisplacement = 0;
    for (const auto &origin : m_previewOrigins) {
        if (origin.window == center) {
            m_rowDisplacement = origin.normalized.x()
                - (target.x() + pose.x - work.x()) / work.width();
            break;
        }
    }
}

int CardStageController::paintSlot(const KWin::EffectWindow *window) const
{
    const int slot = visibleSlot(window);
    if (slot != 99 || !m_rowPageTransition || !m_previewTransition.isValid()
        || m_previewTransition.elapsed() >= RowPageDuration) return slot;
    for (const auto &origin : m_previewOrigins)
        if (origin.window == window && origin.visible) return origin.slot;
    return 99;
}

void CardStageController::captureCardTransition(bool includeGuest, bool includeGrab)
{
    auto *output = m_host->tabletOutputForCardStage();
    if (!output || m_presentation != CardPresentation::Spread
        || (m_launcherGuestActive && !includeGuest) || (m_cardGrabActive && !includeGrab)) {
        clearCardTransition();
        return;
    }
    const auto work = KWin::effects->clientArea(KWin::MaximizeArea, output);
    if (work.width() <= 0 || work.height() <= 0) return;
    const bool fullPose = includeGrab || (m_poseTransition && !m_launcherGuestActive);
    QList<PreviewOrigin> origins;
    for (const auto &window : std::as_const(m_workspace.windows())) {
        if (!window || window->isDeleted() || paintSlot(window) == 99) continue;
        auto rect = previewTargetForWindow(output, window);
        auto pose = stackPoseForWindow(window, rect.width());
        double opacity = 1.0;
        if (includeGrab && m_cardGrabActive && window == selectedWindow()) {
            rect = cardGrabTarget();
            rect.translate(qRound(m_cardGrabOffset.x()), qRound(m_cardGrabOffset.y()));
        } else if (fullPose) {
            rect.translate(qRound(pose.x), qRound(pose.y));
            opacity = applyPoseTransition(window, rect, pose);
        }
        origins.append({window, QRectF((rect.x() - work.x()) / work.width(),
            (rect.y() - work.y()) / work.height(), rect.width() / work.width(),
            rect.height() / work.height()), pose.rotation, pose.visible, opacity, paintSlot(window)});
    }
    m_previewOrigins = origins;
    m_poseTransition = fullPose;
    m_pickupTransition = false;
    m_rowPageTransition = false;
    m_stackBrowseDirection = 0;
    m_stackBrowseOutgoing.clear();
    m_previewTransition.start();
}

KWin::Rect CardStageController::previewTargetForWindow(
    KWin::LogicalOutput *output, const KWin::EffectWindow *window) const
{
    const int slot = paintSlot(window);
    if (!output || slot == 99) return {};
    auto target = m_launcherGuestActive && !m_launcherGuestArrival ? launcherGuestTargetForSlot(output, slot)
                                       : cardTargetForSlot(output, slot);
    if (m_rowPageTransition && visibleSlot(window) == 99) {
        const auto work = KWin::effects->clientArea(KWin::MaximizeArea, output);
        target.translate(slot < 0 ? work.x() - target.right() - 32
                                 : work.right() - target.x() + 32, 0);
    }
    if (m_arrivalExpanding) {
        if (window == m_arrivalWindow) target = activeTarget(output);
        else target.translate(slot * output->geometry().width() / 2, 0);
    }
    const int duration = m_arrivalExpanding ? ArrivalExpandDuration : PreviewTransitionDuration;
    if (!m_previewTransition.isValid() || m_cardGrabActive || m_poseTransition
        || m_presentation != CardPresentation::Spread
        || m_previewTransition.elapsed() >= duration) return target;
    const auto work = KWin::effects->clientArea(KWin::MaximizeArea, output);
    QRectF from(target.x() + slot * work.width() * 0.2, target.y(),
                target.width(), target.height());
    for (const auto &origin : m_previewOrigins) {
        if (origin.window == window) {
            from = QRectF(work.x() + origin.normalized.x() * work.width(),
                work.y() + origin.normalized.y() * work.height(),
                origin.normalized.width() * work.width(),
                origin.normalized.height() * work.height());
            break;
        }
    }
    const double t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
        std::clamp(double(m_previewTransition.elapsed()) / duration, 0.0, 1.0));
    const auto blend = [t](double a, double b) { return qRound(a + (b - a) * t); };
    return KWin::Rect(blend(from.x(), target.x()), blend(from.y(), target.y()),
        blend(from.width(), target.width()), blend(from.height(), target.height()));
}

KWin::Rect CardStageController::posedTargetForWindow(KWin::LogicalOutput *output, const KWin::EffectWindow *window) const
{
    auto rect = previewTargetForWindow(output, window);
    auto pose = stackPoseForWindow(window, rect.width());
    rect.translate(qRound(pose.x), qRound(pose.y));
    (void)applyPoseTransition(window, rect, pose);
    return rect;
}

double CardStageController::applyPoseTransition(const KWin::EffectWindow *window,
                                             KWin::Rect &rect, CardStackPose &pose) const
{
    auto *output = m_host->tabletOutputForCardStage();
    const int duration = m_pickupTransition ? HeldPickupDuration : m_rowPageTransition ? RowPageDuration
        : m_stackBrowseDirection ? StackBrowseDuration
        : m_arrivalExpanding ? ArrivalExpandDuration : PreviewTransitionDuration;
    if (!output || !m_poseTransition
        || (m_cardGrabActive && ((!m_rowPageTransition && !m_pickupTransition) || window == selectedWindow()))
        || !m_previewTransition.isValid()
        || m_presentation != CardPresentation::Spread
        || m_previewTransition.elapsed() >= duration) return 1.0;
    const auto work = KWin::effects->clientArea(KWin::MaximizeArea, output);
    if (m_rowPageTransition) {
        const QRectF area(work.x(), work.y(), work.width(), work.height());
        RowPageFrame to{QRectF(rect.x(), rect.y(), rect.width(), rect.height()),
            pose.rotation, visibleSlot(window) != 99 && pose.visible ? 1.0 : 0.0};
        RowPageFrame from = rowNeighborOrigin(to, area, paintSlot(window));
        int oldSlot = 99;
        for (const auto &origin : m_previewOrigins) {
            if (origin.window != window) continue;
            from = {{area.x() + origin.normalized.x()*area.width(),
                     area.y() + origin.normalized.y()*area.height(),
                     origin.normalized.width()*area.width(), origin.normalized.height()*area.height()},
                    origin.rotation, origin.visible ? origin.opacity : 0.0};
            oldSlot = origin.slot;
            break;
        }
        const int newSlot = visibleSlot(window);
        if (newSlot == 99) to.opacity = from.opacity; // leave through clipping, not a fade
        const bool wraps = oldSlot != 99 && newSlot != 99 && oldSlot * newSlot < 0
            && (from.rect.center().x()-area.center().x()) * (to.rect.center().x()-area.center().x()) < 0;
        const double displacement = m_rowDisplacement * area.width();
        if (oldSlot == 99) {
            from = to;
            from.rect.translate(displacement, 0);
        }
        if (newSlot == 99) to.rect.moveLeft(from.rect.x() - displacement);
        const auto frame = sharedRowPageFrame(from, to, area, wraps, displacement,
            double(m_previewTransition.elapsed()) / duration);
        rect = KWin::Rect(qRound(frame.rect.x()), qRound(frame.rect.y()),
                          qRound(frame.rect.width()), qRound(frame.rect.height()));
        pose.rotation = frame.rotation;
        pose.visible = frame.opacity > 0;
        return frame.opacity;
    }
    const double t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
        std::clamp(double(m_previewTransition.elapsed()) / duration, 0.0, 1.0));
    for (const auto &origin : m_previewOrigins) {
        if (origin.window != window) continue;
        const auto blend = [t](double a, double b) { return qRound(a + (b - a) * t); };
        const double opacity = (origin.visible ? origin.opacity : 0.0) * (1.0 - t) + (pose.visible ? 1.0 : 0.0) * t;
        const bool destinationVisible = pose.visible;
        pose.visible = opacity > 0.0;
        if (!origin.visible) return opacity; // No visible source pose to travel from.
        if (!destinationVisible) {
            rect = KWin::Rect(qRound(work.x() + origin.normalized.x() * work.width()),
                qRound(work.y() + origin.normalized.y() * work.height()),
                qRound(origin.normalized.width() * work.width()), qRound(origin.normalized.height() * work.height()));
            pose.rotation = origin.rotation;
            return opacity;
        }
        rect = KWin::Rect(blend(work.x() + origin.normalized.x() * work.width(), rect.x()),
            blend(work.y() + origin.normalized.y() * work.height(), rect.y()),
            blend(origin.normalized.width() * work.width(), rect.width()),
            blend(origin.normalized.height() * work.height(), rect.height()));
        pose.rotation = origin.rotation + (pose.rotation - origin.rotation) * t;
        if (m_stackBrowseDirection) {
            const int role = window == selectedWindow() ? 1
                : window == m_stackBrowseOutgoing ? -1 : 0;
            const auto accent = stackBrowseAccent(
                double(m_previewTransition.elapsed()) / duration,
                rect.height(), m_stackBrowseDirection, role);
            rect.translate(0, qRound(accent.y));
            pose.rotation += accent.rotation;
        }
        return opacity;
    }
    return t; // Newly visible group: fade in rather than pop into the row.
}

CardStackPose CardStageController::stackPoseForWindow(const KWin::EffectWindow *window, double width) const
{
    if (usesBentoProjectionAperture(window)) {
        return {0.0, 0.0, 0.0, isBentoProjectionPane(window)};
    }
    auto *tablet = m_host->tabletOutputForCardStage();
    if (!tablet) return {0.0, 0.0, 0.0, true};
    if (m_cardGrabActive && window == selectedWindow()) {
        const double t = m_cardGrabScaleTimer.isValid()
            ? heldPickupProgress(m_cardGrabScaleTimer.elapsed()) : 1.0;
        return {0.0, 0.0, m_cardGrabRotation * (1.0 - t), true};
    }
    const auto &spread = m_workspace.model();
    const int cardId = m_workspace.windows().indexOf(const_cast<KWin::EffectWindow *>(window)) + 1;
    const double previewBlend = stackPreviewBlend();
    const int memberCount = spread.stackSizeForId(cardId);
    const int memberIndex = spread.stackPositionForId(cardId);
    const int activeIndex = spread.stackActivePositionForId(cardId);
    CardStackPose pose{0.0, 0.0, 0.0, true};
    const bool previewDestination = cardGrabActive()
        && stackPreviewTarget() != 0
        && spread.sameStack(
            cardId, stackPreviewTarget());
    const int browseTarget = stackBrowseTarget();
    const bool browseDestination = cardGrabActive()
        && browseTarget != 0
        && spread.sameStack(cardId, browseTarget);
    if (memberCount > 1 || previewDestination || browseDestination) {
        // Compact neighbors around their selected face, not storage order.
        // Keep the same visible identities as the open fan (three shoulders).
        const int closedDepth = (activeIndex - memberIndex + memberCount) % memberCount;
        CardStackPose closed = makeClosedStackPose(
            memberCount - 1 - closedDepth, memberCount,
            tablet->geometry().width());
        closed.visible = closedDepth <= 3;
        if (previewDestination) {
            const int insertion = std::clamp(
                stackInsertionIndex(), 0, memberCount);
            const int previousInsertion = std::clamp(
                previousStackInsertionIndex(),
                0, memberCount);
            const int depth = (activeIndex - memberIndex + memberCount) % memberCount;
            const int previewMemberIndex = memberCount
                - (depth >= insertion ? depth + 1 : depth);
            const int previousMemberIndex = memberCount
                - (depth >= previousInsertion ? depth + 1 : depth);
            const CardStackPose previous = makeInsertionStackPose(
                previousMemberIndex, memberCount + 1,
                memberCount - previousInsertion, width);
            const CardStackPose next = makeInsertionStackPose(
                previewMemberIndex, memberCount + 1,
                memberCount - insertion, width);
            const double insertionBlend = stackInsertionBlend();
            const CardStackPose opened{
                previous.x + (next.x - previous.x) * insertionBlend,
                previous.y + (next.y - previous.y) * insertionBlend,
                previous.rotation
                    + (next.rotation - previous.rotation)
                        * insertionBlend,
                previous.visible || next.visible,
            };
            // Entry/exit starts from the already visible browse fan, not a
            // different closed deck. Keep the same blend as the base envelope.
            const auto browsed = makeOpenStackPose(memberIndex, memberCount,
                activeIndex, width);
            pose = {
                browsed.x + (opened.x - browsed.x) * previewBlend,
                browsed.y + (opened.y - browsed.y) * previewBlend,
                browsed.rotation
                    + (opened.rotation - browsed.rotation) * previewBlend,
                opened.visible,
            };
        } else if (browseDestination) {
            pose = makeOpenStackPose(
                memberIndex, memberCount,
                activeIndex, width);
        } else if (!cardGrabActive()
                   && (!m_launcherGuestActive || m_launcherGuestArrival)
                   && spread.sameStack(
                       cardId, spread.selectedId())) {
            pose = makeOpenStackPose(
                memberIndex, memberCount,
                activeIndex, width);
        } else {
            pose = closed;
            const int side = visibleSlot(window);
            if (side == -1 || side == 1) {
                const auto work = KWin::effects->clientArea(KWin::MaximizeArea, tablet);
                const auto base = m_launcherGuestActive && !m_launcherGuestArrival
                    ? launcherGuestTargetForSlot(tablet, side)
                    : cardTargetForSlot(tablet, side);
                const double extent = 7.0 * tablet->geometry().width() / 1024.0
                    * std::min(memberCount - 1, 3);
                const double available = side > 0
                    ? work.right() - (base.x() - extent)
                    : base.right() - work.x();
                pose = neighborStackPose(closedDepth, memberCount, side,
                    available, extent);
            }
        }
    }
    return pose;
}

KWin::Rect CardStageController::launcherGuestTarget(
    KWin::LogicalOutput *output) const
{
    return launcherGuestTargetForSlot(output, 0);
}

KWin::Rect CardStageController::launcherGuestTargetForSlot(
    KWin::LogicalOutput *output, int slot) const
{
    if (!output) {
        return {};
    }
    const KWin::RectF work = KWin::effects->clientArea(
        KWin::MaximizeArea, output);
    const SpreadLayout layout = makeLauncherGuestLayout(work.x(), work.y(),
        work.width(), work.height(), m_launcherGuestGroupCount);
    const auto target = layout.cards[slot < 0 ? 0 : slot > 0 ? 2 : 1];
    return KWin::Rect(qRound(target.x), qRound(target.y),
                      qRound(target.width), qRound(target.height));
}

KWin::Rect CardStageController::activeTarget(KWin::LogicalOutput *output) const
{
    const KWin::RectF work = KWin::effects->clientArea(
        KWin::MaximizeArea, output);
    // The gutter is the same on every edge, a reserving panel included: the
    // work area already stops at the panel. The keyboard overlays the card and
    // never changes its size; a covered text cursor is revealed by lifting
    // the card, in updateKeyboardReveal.
    const CardRect target = makeActiveTarget(
        work.x(), work.y(), work.width(), work.height(), m_settings.gutter());
    return KWin::Rect(qRound(target.x), qRound(target.y),
                      qRound(target.width), qRound(target.height));
}

KWin::Rect CardStageController::activePlacement(
    KWin::LogicalOutput *output) const
{
    if (!m_keyboardReveal || !m_keyboardReveal->window
        || m_keyboardReveal->window != m_activeRestore.window) {
        return activeTarget(output);
    }
    const bool raised = m_host->inputPanelTopForCardStage(output).has_value();
    return m_keyboardReveal->base
        .translated(0.0, raised ? -m_keyboardReveal->lift : 0.0).toRect();
}

void CardStageController::refreshKeyboardReveal()
{
    m_keyboardRevealTimer.start();
}

std::optional<KWin::RectF> CardStageController::keyboardRevealFrame(
    const KWin::EffectWindow *window) const
{
    if (!m_keyboardReveal || m_keyboardReveal->lift <= 0.0 || !window
        || m_keyboardReveal->window != window
        || m_activeRestore.window != window
        || m_presentation != CardPresentation::Active) {
        return std::nullopt;
    }
    return m_keyboardReveal->base;
}

void CardStageController::putBackKeyboardReveal()
{
    m_keyboardRevealTimer.stop();
    m_keyboardRevealRelease.stop();
    const auto reveal = std::exchange(m_keyboardReveal, std::nullopt);
    if (!reveal || reveal->lift <= 0.0 || !reveal->window
        || reveal->window->isDeleted() || !reveal->window->window()) {
        return;
    }
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    const KWin::RectF frame = reveal->window->frameGeometry();
    reveal->window->window()->moveResize(
        KWin::RectF(reveal->base.x(), reveal->base.y(), frame.width(), frame.height()));
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce keyboard reveal returned" << reveal->window->caption()
            << "to" << reveal->base.toRect();
}

void CardStageController::updateKeyboardReveal()
{
    const auto window = m_activeRestore.window;
    if (m_keyboardReveal && m_keyboardReveal->window != window) {
        // Whatever took the card out of Active placed it; the lift left with it.
        m_keyboardReveal.reset();
        m_keyboardRevealRelease.stop();
    }
    auto *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || m_presentation != CardPresentation::Active
        || !m_activeRestore.valid || !window || window->isDeleted()
        || !window->window() || !tablet) {
        return;
    }
    auto *client = window->window();
    if (client->isInteractiveMove() || client->isInteractiveResize()
        || client->isFullScreen() || client->maximizeMode() != KWin::MaximizeRestore
        || client->quickTileMode() != KWin::QuickTileMode{}) {
        return;
    }
    const auto keyboardTop = m_host->inputPanelTopForCardStage(tablet);
    if (!keyboardTop) {
        if (!m_keyboardReveal) return;
        const KWin::RectF base = m_keyboardReveal->base;
        if (m_keyboardReveal->lift > 0.0) {
            m_keyboardReveal->lift = 0.0;
            QScopedValueRollback<bool> applying(m_applyingWindowState, true);
            client->moveResize(KWin::RectF(base.x(), base.y(),
                window->frameGeometry().width(), window->frameGeometry().height()));
            KWin::effects->addRepaintFull();
            qInfo() << "Kadunce keyboard reveal returned" << window->caption()
                    << "to" << base.toRect();
        }
        m_keyboardRevealRelease.start();
        return;
    }
    m_keyboardRevealRelease.stop();
    if (!m_keyboardReveal) {
        // The placement asked for rather than the frame, which still shows the
        // old size while a card that has just arrived acknowledges its new one.
        m_keyboardReveal = KeyboardReveal{window, client->moveResizeGeometry(), 0.0};
    }
    const KWin::RectF base = m_keyboardReveal->base;
    const KWin::RectF frame = window->frameGeometry();
    // Contents roll up only as far as the line being typed on needs, and
    // never back down while the keyboard is up: a line typed below the keys,
    // or taller keys, rolls them further; a cursor moving up, a shorter
    // keyboard or focus leaving the card moves nothing.
    double lift = m_keyboardReveal->lift;
    if (const auto cursor = m_host->textCursorForCardStage(window)) {
        // The cursor moves with the card, so measure it where the card rests.
        const KWin::RectF resting = cursor->translated(0.0, base.y() - frame.y());
        lift = std::max(lift, keyboardRevealLift(resting.top(), resting.bottom(),
            *keyboardTop, m_settings.gutter(), tablet->geometry().y()));
    }
    m_keyboardReveal->lift = lift;
    // Only the height the contents sit at is the reveal's to set. A client
    // that trims its own frame, as a terminal does to whole rows, keeps the
    // size it chose; the settle owns size.
    const int top = qRound(base.y() - lift);
    if (qRound(frame.y()) == top) return;
    const KWin::Rect target(qRound(frame.x()), top, qRound(frame.width()),
                            qRound(frame.height()));
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    client->moveResize(KWin::RectF(target));
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce keyboard reveal" << window->caption()
            << "lift" << lift << "target" << target;
}

bool CardStageController::selectedStackContains(const QPointF &position) const
{
    if (!m_active || m_presentation != CardPresentation::Spread) {
        return false;
    }
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    const int selectedId = m_workspace.selectedId();
    const int memberCount = m_workspace.stackSizeForId(selectedId);
    if (!tablet || memberCount <= 1 || selectedIsBentoProjection()) {
        return false;
    }
    const KWin::Rect center = m_launcherGuestActive ? cardTargetForSlot(tablet, 0)
        : posedTargetForWindow(tablet, selectedWindow());
    const CardStackEnvelope envelope = makeOpenStackEnvelope(
        memberCount, m_workspace.stackActivePositionForId(selectedId),
        center.width(), center.height());
    constexpr double VerticalSlop = 48.0;
    const KWin::RectF deck(
        center.x() + envelope.left,
        center.y() - VerticalSlop,
        center.width() + envelope.right - envelope.left,
        center.height() + VerticalSlop * 2.0);
    return deck.contains(position);
}

int CardStageController::activeSideForPoint(const QPointF &position) const
{
    if (!m_active || m_presentation != CardPresentation::Active) {
        return 0;
    }
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!tablet || !tablet->geometry().contains(position.toPoint())) {
        return 0;
    }
    const KWin::Rect active = activeTarget(tablet);
    if (position.x() < active.x()) {
        return -1;
    }
    if (position.x() >= active.right()) {
        return 1;
    }
    return 0;
}

QStringList CardStageController::hudState() const
{
    const bool available = m_active
        && m_presentation == CardPresentation::Spread;
    KWin::EffectWindow *window = available ? selectedWindow() : nullptr;
    const int selectedId = available ? m_workspace.selectedId() : 0;
    const int stackCount = available
        ? m_workspace.stackSizeForId(selectedId) : 0;
    const int stackPosition = stackCount > 0
        ? m_workspace.stackActivePositionForId(selectedId) + 1 : 0;
    return {
        available ? QStringLiteral("1") : QStringLiteral("0"),
        window ? window->caption() : QString(),
        QString::number(stackPosition),
        QString::number(stackCount),
    };
}

std::optional<PreparedCarrySource> CardStageController::prepareNativeCarrySource(KWin::EffectWindow *window) const
{
    auto *output = m_host->tabletOutputForCardStage();
    if (!m_active || m_applyingWindowState || m_cardGrabActive || m_launcherGuestActive
        || m_presentation != CardPresentation::Active || !m_activeRestore.valid
        || !window || window->isDeleted() || !window->window() || !output
        || selectedWindow() != window || m_activeRestore.window != window
        || window->screen() != output || window->isUserResize() || window->isMinimized()
        || !KWin::effects->screens().contains(output)) return std::nullopt;
    PreparedCarrySource source;
    source.m_owner = m_carrySourceIdentity;
    source.m_window = window;
    source.m_generation = m_transferGuard.generation();
    source.m_outputGeometry = output->geometry();
    source.m_origin = {window->window()->internalId().toString(), output->name(),
        m_restoreGeneration, m_workspace.revision()};
    // Use the authoritative pre-Active record, NOT the current gutter geometry.
    const auto &saved = m_activeRestore;
    source.m_restore = {window->window(), output, saved.geometry, saved.floatingGeometry,
        saved.fullscreenRestoreGeometry, saved.maximizeMode, saved.quickTileMode,
        saved.fullScreen, false};
    return source;
}

bool CardStageController::nativeCarrySourceValid(const PreparedCarrySource &source) const
{
    if (source.m_owner.lock() != m_carrySourceIdentity) return false;
    const auto current = prepareNativeCarrySource(source.m_window);
    return current && current->m_origin == source.m_origin
        && current->m_generation == source.m_generation
        && current->m_outputGeometry == source.m_outputGeometry
        && current->m_restore.output == source.m_restore.output;
}

bool CardStageController::transferNativeCarryToDesktop(const PreparedCarrySource &source,
    KWin::LogicalOutput *destination, const KWin::RectF &geometry)
{
    if (!nativeCarrySourceValid(source) || !destination
        || m_host->isTabletOutputForCardStage(destination)
        || !KWin::effects->screens().contains(destination) || !geometry.isValid()
        || source.m_window->isUserMove() || source.m_window->isUserResize()) return false;
    const auto removal = m_workspace.prepareRemoval(source.m_window);
    if (!removal) return false;
    const QPointer<KWin::EffectWindow> arrival = source.m_window;
    const QPointer<KWin::LogicalOutput> target = destination;
    const auto targetGeometry = destination->geometry();
    bool committed = false;
    (void)m_host->admitCardToDesktopStage(arrival, destination, geometry,
        [&] {
            if (committed || !target || target->geometry() != targetGeometry
                || !KWin::effects->screens().contains(target.data())
                || !nativeCarrySourceValid(source) || arrival->isUserMove() || arrival->isUserResize()
                || !m_workspace.commitRemoval(*removal)) return false;
            committed = true;
            // Retire source restoration BEFORE the receiver performs native work.
            // This is logical state only: never replay the departed window's old
            // geometry during cleanup, release, or synchronous activation signals.
            ++m_restoreGeneration;
            forgetManagedRestore(arrival);
            m_presentation = CardPresentation::Spread;
            m_active = !m_workspace.windows().isEmpty();
            m_originalCardStackingOrder.removeAll(arrival);
            return true;
        },
        [&] {
            if (!committed) return;
            m_activeSettleTimer.stop();
            m_activeSettleRemaining = 0;
            if (arrival && !arrival->isDeleted()) {
                KWin::effects->setElevatedWindow(arrival, false);
                m_host->unredirectForCardStage(arrival);
            }
            m_host->setPagingShortcutsForCardStage(m_active);
            syncSelectedElevation();
            KWin::effects->addRepaintFull();
        });
    // After publication, a receiver interruption is consumed, never a request
    // to replay source removal/restoration. The receiver follows this contract.
    return committed;
}

void CardStageController::beginCardGrab(const QPointF &position)
{
    if (!m_active || m_presentation != CardPresentation::Spread
        || m_cardGrabActive || !selectedWindow() || selectedIsBentoProjection()
        || !qIsFinite(position.x()) || !qIsFinite(position.y())) {
        return;
    }
    auto *tablet = m_host->tabletOutputForCardStage();
    if (!tablet) return;
    // Freeze the presentation at pickup. Paging/stack feedback may move the
    // destination, but never changes the contact anchor of the held card.
    auto pickup = previewTargetForWindow(tablet, selectedWindow());
    auto pickupPose = stackPoseForWindow(selectedWindow(), pickup.width());
    pickup.translate(qRound(pickupPose.x), qRound(pickupPose.y));
    (void)applyPoseTransition(selectedWindow(), pickup, pickupPose);
    if (pickup.isEmpty()) return;
    captureCardTransition(false, true);
    if (!m_workspace.selectedIsStandalone()
        && !m_workspace.detachSelectedMember()) {
        clearCardTransition();
        return;
    }
    m_cardGrabActive = true;
    m_pickupTransition = m_poseTransition;
    m_cardGrabOffset = {};
    m_cardGrabStart = position;
    m_cardGrabTarget = pickup;
    m_cardGrabRotation = pickupPose.rotation;
    const auto work = KWin::effects->clientArea(KWin::MaximizeArea, tablet);
    m_cardGrabDestinationSize = QSizeF(work.width() * HeldCardFraction,
                                     work.height() * HeldCardFraction);
    m_cardGrabScaleTimer.start();
    m_cardGrabPageOffset = 0;
    m_cardGrabMoved = false;
    m_cardStackPreviewTarget = 0;
    m_stackInsertion.reset();
    m_cardStackInsertionIndex = -1;
    m_cardStackPreviousInsertionIndex = -1;
    m_cardStackPreviewFrom = 0.0;
    m_cardStackPreviewTo = 0.0;
    m_cardStackPreviewArmed = false;
    m_cardStackPreviewTimer.invalidate();
    m_cardStackInsertionTimer.invalidate();
    m_cardGrabPointer = position;
    m_cardGrabDestinationOutput.clear();
    KWin::effects->setElevatedWindow(selectedWindow(), true);
    syncSelectedStackingOrder();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "lifted Spread card"
            << m_workspace.selectedId();
}

void CardStageController::updateCardGrab(const QPointF &position)
{
    if (!m_cardGrabActive || !qIsFinite(position.x()) || !qIsFinite(position.y())) {
        return;
    }
    m_cardGrabOffset = position - m_cardGrabStart;
    updateCardGrabDestination(position);
    if (std::hypot(m_cardGrabOffset.x(), m_cardGrabOffset.y()) >= 36.0) {
        m_cardGrabMoved = true;
    }
    KWin::effects->addRepaintFull();
}

void CardStageController::updateCardGrabDestination(const QPointF &position)
{
    if (!m_cardGrabActive) {
        return;
    }
    m_cardGrabPointer = position;
    KWin::LogicalOutput *output = KWin::effects->screenAt(position.toPoint());
    m_cardGrabDestinationOutput = output
        && !m_host->isTabletOutputForCardStage(output)
        ? output->name() : QString();
}

void CardStageController::pageCardGrab(int direction)
{
    if (!m_cardGrabActive || m_workspace.count() <= 2 || direction == 0) {
        return;
    }
    const int destinations = m_workspace.count() - 1;
    captureCardTransition(false, true);
    m_rowPageTransition = true;
    const int next = m_cardGrabPageOffset + (direction < 0 ? -1 : 1);
    const int remainder = next % destinations;
    m_cardGrabPageOffset = remainder < 0 ? remainder + destinations : remainder;
    anchorRowTransition();
    syncSelectedStackingOrder();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision
            << "edge-dwell paged detached row"
            << m_cardGrabPageOffset + 1 << "of" << destinations;
}

void CardStageController::finishCardGrab(bool commit)
{
    if (!m_cardGrabActive) {
        return;
    }
    KWin::EffectWindow *grabbed = selectedWindow();
    if (!commit) clearCardTransition();
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    // Release animation is presentation only. Capture before changing order;
    // the held surface remains directly attached to its contact until release.
    // Cancellation/disable must never acquire an animation lifetime.
    if (commit && !m_launcherGuestActive) {
        captureCardTransition(false, true);
    }
    int movement = 0;
    bool stacked = false;
    if (commit && m_cardStackPreviewArmed
        && m_cardStackPreviewTarget != 0
        && m_cardStackPreviewRevision == m_workspace.revision()
        && m_cardStackPreviewTarget == cardStackCandidate()) {
        stacked = m_stackInsertion && m_workspace.commitStackInsertion(*m_stackInsertion);
    } else if (commit && m_cardGrabMoved && tablet
               && std::abs(m_cardGrabOffset.y()) <= m_cardGrabTarget.height() * 0.48) {
        const KWin::RectF work = KWin::effects->clientArea(
            KWin::MaximizeArea, tablet);
        const SpreadLayout layout = makeSpreadLayout(
            work.x(), work.y(), work.width(), work.height());
        const double pitch = layout.cards[1].width + layout.gutter;
        if (m_cardGrabOffset.x() <= -pitch * 0.82) {
            movement = -1;
        } else if (m_cardGrabOffset.x() >= pitch * 0.82) {
            movement = 1;
        }
    }

    // CARD-LIFECYCLE.md §9: a stacked card pulled up out of the stack is
    // released into the Spread where it stood, and one that never rose out of
    // the stack rejoins it. Sideways travel keeps meaning reorder, so the two
    // answers cannot compete for the same release; §10's top edge is further
    // up the same gesture and takes the card out of Spread entirely.
    const bool pulledFromStack = commit && !stacked
        && m_workspace.hasDetachedMember()
        && m_cardGrabOffset.y() <= -m_cardGrabTarget.height() * StackReleaseRise;
    const bool restoreDetached = m_workspace.hasDetachedMember()
        && (!commit || (!stacked && movement == 0 && !pulledFromStack));
    if (restoreDetached) {
        m_workspace.restoreDetachedMember();
    } else {
        m_workspace.commitDetachedMember();
    }
    if (!restoreDetached && !stacked && movement != 0) {
        m_workspace.moveSelected(movement);
    }
    resetCardGrabState(grabbed, stacked);
    qInfo() << "Kadunce" << Revision << "released Spread card"
            << m_workspace.selectedId() << "movement" << movement
            << "stacked" << stacked;
}

void CardStageController::resetCardGrabState(
    KWin::EffectWindow *grabbed, bool stacked)
{
    // A release changes the detached neighborhood. Do not replay a row page
    // captured against its old membership over the committed stack/drop.
    if (m_rowPageTransition) clearCardTransition();
    if (grabbed && !grabbed->isDeleted() && !stacked) {
        KWin::effects->setElevatedWindow(grabbed, false);
    }
    m_cardGrabActive = false;
    m_cardGrabOffset = {};
    m_cardGrabStart = {};
    m_cardGrabTarget = {};
    m_cardGrabRotation = 0.0;
    m_cardGrabDestinationSize = {};
    m_cardGrabScaleTimer.invalidate();
    m_cardGrabPageOffset = 0;
    m_cardGrabMoved = false;
    m_cardStackPreviewTarget = 0;
    m_stackInsertion.reset();
    m_cardStackInsertionIndex = -1;
    m_cardStackPreviousInsertionIndex = -1;
    m_cardStackPreviewFrom = 0.0;
    m_cardStackPreviewTo = 0.0;
    m_cardStackPreviewArmed = false;
    m_cardStackPreviewTimer.invalidate();
    m_cardStackInsertionTimer.invalidate();
    m_cardGrabPointer = {};
    m_cardGrabDestinationOutput.clear();
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
}

bool CardStageController::finishCardGrabOnOutput(const QPointF &position)
{
    if (!m_cardGrabActive
        || m_presentation != CardPresentation::Spread) {
        return false;
    }
    KWin::LogicalOutput *destination = KWin::effects->screenAt(
        position.toPoint());
    if (!destination || !QRectF(destination->geometry()).contains(position)) {
        finishCardGrab(false);
        return true;
    }
    if (m_host->isTabletOutputForCardStage(destination)) {
        return false;
    }
    KWin::EffectWindow *grabbed = selectedWindow();
    const int cardIndex = liveCardIndex(grabbed);
    if (!grabbed || cardIndex < 0 || !grabbed->window()) {
        return false;
    }

    const auto removal = m_workspace.prepareRemoval(QPointer<KWin::EffectWindow>(grabbed));
    if (!removal) { finishCardGrab(false); return true; }
    QPointer<KWin::EffectWindow> arrival = grabbed;
    const bool accepted = m_host->admitCardToDesktopStage(
        grabbed, destination, KWin::RectF(activeTarget(destination)),
        [&] {
            if (!m_workspace.commitRemoval(*removal)) return false;
            forgetManagedRestore(arrival);
            return true;
        },
        [&] {
            // Both logical owners are published before native/visual cleanup.
            m_originalCardStackingOrder.removeAll(arrival);
            const bool empty = m_workspace.windows().isEmpty();
            if (empty) {
                m_active = false;
                m_presentation = CardPresentation::Spread;
                m_originalCardStackingOrder.clear();
            }
            resetCardGrabState(arrival, false);
            if (empty) m_host->setPagingShortcutsForCardStage(false);
            if (arrival && !arrival->isDeleted()) m_host->unredirectForCardStage(arrival);
        });
    if (!accepted) finishCardGrab(false);
    return true;
}

int CardStageController::cardStackCandidate() const
{
    if (!m_cardGrabActive || !m_cardGrabMoved
        || !m_workspace.selectedIsStandalone() || m_workspace.count() < 2) {
        return 0;
    }
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!tablet) {
        return 0;
    }
    const KWin::RectF work = KWin::effects->clientArea(
        KWin::MaximizeArea, tablet);
    const SpreadLayout layout = makeSpreadLayout(
        work.x(), work.y(), work.width(), work.height());
    if (std::abs(m_cardGrabOffset.x()) > layout.cards[1].width * 0.48
        || std::abs(m_cardGrabOffset.y()) > layout.cards[1].height * 0.48
        || !m_cardGrabDestinationOutput.isEmpty()) {
        return 0;
    }
    const int destination = m_workspace.detachedNeighborhood(m_cardGrabPageOffset)[1];
    const auto destinationWindow = m_workspace.windows().value(destination - 1);
    return usesBentoProjectionAperture(destinationWindow) ? 0 : destination;
}

void CardStageController::setCardStackPreview(int destinationId)
{
    if (destinationId == 0 || destinationId != cardStackCandidate()) {
        return;
    }
    auto *tablet = m_host->tabletOutputForCardStage();
    if (!tablet) return;
    const int slot = m_cardGrabTarget.center().x() + m_cardGrabOffset.x()
            < cardTargetForSlot(tablet, 0).center().x()
        ? m_workspace.stackSizeForId(destinationId) : 0;
    const auto insertion = m_workspace.prepareStackInsertionAtDepth(
        m_workspace.windows().value(destinationId - 1), slot);
    if (!insertion) return;
    m_stackInsertion = insertion;
    const double current = stackPreviewBlend();
    m_cardStackPreviewTarget = destinationId;
    m_cardStackPreviewRevision = m_workspace.revision();
    m_cardStackInsertionIndex = slot;
    m_cardStackPreviousInsertionIndex = m_cardStackInsertionIndex;
    m_cardStackInsertionTimer.invalidate();
    m_cardStackPreviewFrom = current;
    m_cardStackPreviewTo = 1.0;
    m_cardStackPreviewArmed = true;
    m_cardStackPreviewTimer.restart();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "opened TouchPad stack target"
            << destinationId;
}

bool CardStageController::pageCardStackInsertion(int direction)
{
    if (!m_cardGrabActive || !m_cardStackPreviewArmed
        || m_cardStackPreviewTarget == 0 || direction == 0
        || m_cardStackPreviewRevision != m_workspace.revision()
        || m_cardStackPreviewTarget != cardStackCandidate()) {
        return false;
    }
    const int destinationSize =
        m_workspace.stackSizeForId(m_cardStackPreviewTarget);
    const int next = std::clamp(
        m_cardStackInsertionIndex + (direction < 0 ? 1 : -1),
        0, destinationSize);
    if (next == m_cardStackInsertionIndex) {
        return false;
    }
    const auto insertion = m_workspace.prepareStackInsertionAtDepth(
        m_workspace.windows().value(m_cardStackPreviewTarget - 1), next);
    if (!insertion) return false;
    m_stackInsertion = insertion;
    m_cardStackPreviousInsertionIndex = m_cardStackInsertionIndex;
    m_cardStackInsertionIndex = next;
    m_cardStackInsertionTimer.restart();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision
            << "selected stack insertion seam"
            << m_cardStackInsertionIndex + 1 << "of" << destinationSize + 1;
    return true;
}

void CardStageController::clearCardStackPreview()
{
    m_stackInsertion.reset();
    if (m_cardStackPreviewTarget == 0) {
        return;
    }
    const double current = stackPreviewBlend();
    m_cardStackPreviewFrom = current;
    m_cardStackPreviewTo = 0.0;
    m_cardStackPreviewArmed = false;
    m_cardStackPreviewTimer.restart();
    KWin::effects->addRepaintFull();
}

void CardStageController::syncSelectedElevation()
{
    if (!m_active) {
        return;
    }
    const int selectedId = m_workspace.selectedId();
    // Tette owns the center for its entire guest lease, including drawer
    // collapse. Restore stack elevation only after the guest actually leaves.
    for (int index = 0; index < m_workspace.windows().size(); ++index) {
        KWin::EffectWindow *window = m_workspace.windows().at(index).data();
        if (window && !window->isDeleted()) {
            const bool selectedProjectionPane = selectedIsBentoProjection()
                && isBentoProjectionPane(window);
            KWin::effects->setElevatedWindow(window,
                m_presentation == CardPresentation::Spread
                    && !m_launcherGuestActive
                    && (selectedProjectionPane
                        || (index + 1 == selectedId
                            && m_workspace.stackSizeForId(selectedId) > 1)));
        }
    }
    syncSelectedStackingOrder();
}

void CardStageController::syncSelectedStackingOrder()
{
    if (!m_active || m_presentation != CardPresentation::Spread || m_launcherGuestActive) {
        return;
    }
    if (selectedIsBentoProjection()) return;
    const int faceId = m_cardGrabActive ? stackBrowseTarget() : m_workspace.selectedId();
    const std::vector<int> paintOrder =
        m_workspace.stackPaintOrderForId(faceId);
    if (paintOrder.size() <= 1) {
        return;
    }
    for (const int cardId : paintOrder) {
        const int index = cardId - 1;
        if (index < 0 || index >= m_workspace.windows().size()) {
            continue;
        }
        KWin::EffectWindow *window = m_workspace.windows().at(index).data();
        if (window && !window->isDeleted() && window->window()) {
            KWin::workspace()->raiseWindow(window->window());
        }
    }
}

void CardStageController::restoreOriginalStackingOrder()
{
    for (const QPointer<KWin::EffectWindow> &window :
         std::as_const(m_originalCardStackingOrder)) {
        if (window && !window->isDeleted() && window->window()) {
            KWin::workspace()->raiseWindow(window->window());
        }
    }
}

void CardStageController::toggle()
{
    m_host->cancelInputForCardStage();
    if (m_active) {
        finishCardGrab(false);
        if (m_presentation == CardPresentation::Spread) {
            if (!enterActive()) {
                return;
            }
        } else {
            parkActiveSnapshot();
            m_presentation = CardPresentation::Spread;
        }
        syncSelectedElevation();
        KWin::effects->addRepaintFull();
        qInfo() << "Kadunce" << Revision << "changed to"
                << (m_presentation == CardPresentation::Active
                        ? "Active" : "Spread");
        return;
    }

    m_restoredMinimizations.clear();
    rebuildLiveCards();
    if (m_workspace.windows().isEmpty()) {
        qWarning() << "Kadunce" << Revision
                   << "has no eligible live window on the tablet";
        return;
    }
    m_active = true;
    m_presentation = CardPresentation::Spread;
    m_host->setPagingShortcutsForCardStage(true);
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "activated with"
            << m_workspace.windows().size() << "live tablet cards; selected"
            << m_workspace.selectedIndex() + 1;
}

bool CardStageController::admitBentoStack(const BentoProjectionSession &projection,
    const std::function<bool()> &commitSource)
{
    auto *tablet = m_host->tabletOutputForCardStage();
    // §2: the group is one Spread entry beside whatever individual cards this
    // stage already owns, so an active stage is not a reason to refuse it.
    if (m_cardGrabActive || m_launcherGuestActive
        || !tablet || projection.output != tablet
        || projection.outputName != tablet->name()
        || !tablet->geometry().contains(projection.workspaceArea)
        || !validBentoProjectionShape(projectionShape(projection))) return false;
    QList<QPointer<KWin::EffectWindow>> windows;
    QList<ActiveRestoreSnapshot> snapshots;
    const auto append = [&](const BentoProjectionMember &member, bool visible) {
        const auto &saved = member.restore;
        auto *client = saved.window.data();
        if (!client || client->isDeleted() || client->output() != tablet
            || !client->effectWindow() || !saved.geometry.isValid()
            || client->effectWindow() != member.window
            || member.window->isMinimized() != member.minimized
            || (visible && member.minimized)
            || (!visible && !member.minimized)) return false;
        auto *window = client->effectWindow();
        windows.append(window);
        snapshots.append({.window = window, .geometry = saved.geometry,
            .floatingGeometry = saved.floatingGeometry,
            .fullscreenRestoreGeometry = saved.fullscreenRestoreGeometry,
            .quickTileMode = saved.quickTileMode, .maximizeMode = saved.maximizeMode,
            .fullScreen = saved.fullScreen, .minimized = saved.minimized, .valid = true});
        return true;
    };
    QList<QPointer<KWin::EffectWindow>> ordered;
    ordered.append(projection.lead);
    for (const auto &member : projection.panes)
        if (member.window != projection.lead) ordered.append(member.window);
    for (const auto &member : projection.sleeping) ordered.append(member.window);
    for (const auto &window : std::as_const(ordered)) {
        const auto pane = std::find_if(projection.panes.cbegin(), projection.panes.cend(),
            [&](const auto &member) { return member.window == window; });
        if (pane != projection.panes.cend()) {
            if (!append(*pane, true)) return false;
            continue;
        }
        const auto sleeping = std::find_if(projection.sleeping.cbegin(), projection.sleeping.cend(),
            [&](const auto &member) { return member.window == window; });
        if (sleeping == projection.sleeping.cend() || !append(*sleeping, false)) return false;
    }
    const auto admission = m_workspace.prepareStackAdmission(windows);
    if (!admission) return false;
    // Both membership owners publish before signals/native calls. Original
    // restore records cross directly; no frame acknowledgement is required.
    if (!publishOwnershipThenRecord(
            [&] { return m_workspace.commitAdmission(*admission, commitSource); },
            [&] {
                for (const auto &snapshot : std::as_const(snapshots)) {
                    m_parkedRestores.removeIf([&](const auto &existing) {
                        return !existing.window || existing.window == snapshot.window; });
                    m_parkedRestores.append(snapshot);
                }
                m_bentoProjectionWindows = windows;
                m_bentoProjectionPaneWindows.clear();
                for (const auto &member : projection.panes)
                    m_bentoProjectionPaneWindows.append(member.window);
                m_bentoProjectionSession = projection;
                ++m_restoreGeneration;
                m_active = true;
                // The stage can already present an individual Active card here:
                // §5 gives a displaced pane to card ownership, and the group is
                // admitted beside it. Discarding the record instead of parking
                // it would leave that window with the geometry Bento gave it,
                // so release could no longer return it where it began.
                parkActiveSnapshot();
                m_presentation = CardPresentation::Spread;
                for (const auto &window : std::as_const(projection.stackingOrder)) {
                    m_originalCardStackingOrder.removeAll(window);
                    m_originalCardStackingOrder.append(window);
                }
                m_restoredMinimizations.clear();
            })) {
        return false;
    }
    m_host->cancelInputForCardStage();
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    for (const auto &window : windows) {
        if (!m_active) return true;
        if (window && !window->isDeleted() && window->window()) {
            m_host->connectManagedWindowForCardStage(window);
            // Each member's own visibility is part of the transferred session.
            // Spread never wakes a sleeping member merely to present the group.
        }
    }
    if (!m_active) return true;
    m_host->setPagingShortcutsForCardStage(true);
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    return true;
}

bool CardStageController::resumeSelectedBentoProjection()
{
    if (!m_active || m_presentation != CardPresentation::Spread
        || !selectedIsBentoGroup() || !m_bentoProjectionSession
        || m_cardGrabActive || m_launcherGuestActive) return false;
    const BentoProjectionSession projection = *m_bentoProjectionSession;
    const auto allWindows = m_workspace.windows();
    const auto projectionWindows = m_bentoProjectionWindows;
    const auto originalStackingOrder = m_originalCardStackingOrder;
    // CARD-LIFECYCLE.md §6: selecting the group resumes the group. Only its own
    // members leave this stage; the cards around it keep their ownership and
    // their parked restore records, so none of them reaches the native desktop.
    const auto departure = m_workspace.prepareGroupRemoval(projectionWindows,
        OwnershipTransition::CardToBento);
    if (!departure) return false;
    bool committed = false;
    return m_host->resumeBentoProjectionForCardStage(projection,
        [this, projectionWindows, &departure, &committed] {
            committed = commitResumeHandback(
                [&] {
                    if (!m_active || !selectedIsBentoGroup()
                        || !m_workspace.commitGroupRemoval(*departure)) return false;
                    m_transferGuard.invalidate();
                    m_host->cancelInputForCardStage();
                    clearCardTransition();
                    m_activeSettleTimer.stop();
                    m_activeSettleRemaining = 0;
                    m_activeRestore = {};
                    for (const auto &window : projectionWindows) {
                        retireActiveIdentity(window);
                        m_parkedRestores.removeIf([&](const auto &saved) {
                            return !saved.window || saved.window == window; });
                        m_originalCardStackingOrder.removeAll(window);
                    }
                    m_bentoProjectionWindows.clear();
                    m_bentoProjectionPaneWindows.clear();
                    // §2: the display now presents Bento. Whatever this stage
                    // still owns stays owned and hidden behind it.
                    m_presentation = CardPresentation::Bento;
                    m_host->setPagingShortcutsForCardStage(false);
                    if (m_workspace.windows().isEmpty()) {
                        m_active = false;
                        m_presentation = CardPresentation::Spread;
                        m_originalCardStackingOrder.clear();
                    }
                    return true;
                },
                [&] {
                    m_bentoProjectionSession.reset();
                    for (const auto &window : projectionWindows) {
                        if (window && !window->isDeleted())
                            KWin::effects->setElevatedWindow(window, false);
                    }
                    m_host->retireBentoProjectionForCardStage(projectionWindows);
                },
                [] {});
            return committed;
        },
        [this, allWindows, projectionWindows, originalStackingOrder, &committed] {
            if (!committed) return;
            for (const auto &window : allWindows) {
                if (window && !window->isDeleted()) {
                    KWin::effects->setElevatedWindow(window, false);
                    m_host->unredirectForCardStage(window);
                }
            }
            QScopedValueRollback<bool> applying(m_applyingWindowState, true);
            m_restoredMinimizations.clear();
            // Raise only what resumed. A retained card is hidden rather than
            // restored, so leaving it above the panes would occlude them in
            // KWin's own stacking while Kadunce paints nothing for it.
            for (const auto &window : originalStackingOrder) {
                if (window && !window->isDeleted() && window->window()
                    && projectionWindows.contains(window))
                    KWin::workspace()->raiseWindow(window->window());
            }
            KWin::effects->addRepaintFull();
        });
}

void CardStageController::release()
{
    m_transferGuard.invalidate();
    m_host->cancelInputForCardStage();
    clearCardTransition();
    m_activeSettleTimer.stop();
    m_activeSettleRemaining = 0;
    if (!m_active) {
        return;
    }
    finishCardGrab(false);
    endLauncherGuest();
    const QPointer<KWin::EffectWindow> releasedWindow = selectedWindow();
    // Stop filtering the scene before fullscreen restoration changes layers,
    // activation or geometry. Those operations can synchronously reenter KWin.
    m_active = false;
    m_presentation = CardPresentation::Spread;
    for (const QPointer<KWin::EffectWindow> &window :
         std::as_const(m_workspace.windows())) {
        if (window && !window->isDeleted()) {
            m_host->unredirectForCardStage(window);
            KWin::effects->setElevatedWindow(window, false);
        }
    }
    restoreActiveSnapshot();
    restoreOriginalStackingOrder();
    // Replaying the old stack must not leave the released fullscreen client
    // underneath another application or without active-fullscreen treatment.
    if (releasedWindow && !releasedWindow->isDeleted()
        && releasedWindow->window() && releasedWindow->window()->isFullScreen()
        && KWin::effects->sessionState() == KWin::SessionState::Normal) {
        KWin::workspace()->raiseWindow(releasedWindow->window());
        KWin::workspace()->activateWindow(releasedWindow->window(), true);
        qInfo() << "Kadunce fullscreen release: direct scene, restored focus"
                << releasedWindow->caption() << releasedWindow->frameGeometry();
    }
    m_host->setPagingShortcutsForCardStage(false);
    m_workspace.clear();
    m_presentedActive = nullptr;
    m_bentoProjectionWindows.clear();
    m_bentoProjectionPaneWindows.clear();
    m_bentoProjectionSession.reset();
    m_originalCardStackingOrder.clear();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "released";
}

bool CardStageController::adoptDisplayWithActive(KWin::EffectWindow *carried,
    const std::function<bool()> &commitSource)
{
    auto *tablet = m_host->tabletOutputForCardStage();
    // §3 adopts a display Kadunce does not yet own. The plan tests the same
    // thing, so both must read ownership rather than the stage's active flag.
    if (ownsDisplay(tablet) || !commitSource || !tablet || !carried || carried->isDeleted()
        || !carried->window() || carried->screen() != tablet
        || carried->isUserResize() || carried->isMinimized()
        || !m_host->isManagedWindowForCardStage(carried)
        || !KWin::effects->screens().contains(tablet)) {
        qInfo() << "Kadunce refused display adoption:"
                << "owns" << ownsDisplay(tablet) << "tablet" << bool(tablet)
                << "window" << bool(carried && !carried->isDeleted() && carried->window())
                << "sameOutput" << (carried && carried->screen() == tablet)
                << "resizing" << (carried && carried->isUserResize())
                << "minimized" << (carried && carried->isMinimized())
                << "managed" << (carried && m_host->isManagedWindowForCardStage(carried));
        return false;
    }
    // CARD-LIFECYCLE.md §3: the first deliberate action adopts the display as
    // one batch. Nothing is published until the carry commits, so a refusal
    // leaves every window Native rather than half of them owned.
    if (!commitSource()) {
        qInfo() << "Kadunce refused display adoption: the carried source changed";
        return false;
    }
    m_restoredMinimizations.clear();
    rebuildLiveCards();
    const int carriedIndex = liveCardIndex(carried);
    if (carriedIndex < 0) {
        qInfo() << "Kadunce refused display adoption: the carried window is not a live card";
        m_workspace.clear();
        m_presentedActive = nullptr;
        m_parkedRestores.clear();
        m_originalCardStackingOrder.clear();
        return false;
    }
    m_active = true;
    m_presentation = CardPresentation::Spread;
    m_host->setPagingShortcutsForCardStage(true);
    (void)selectCardEntry(carried);
    // Adoption produces individual cards only. The carried window is Active and
    // every other eligible window is a nonselected card; no pane is filled.
    if (!enterActive()) m_presentation = CardPresentation::Spread;
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "adopted the display as"
            << m_workspace.windows().size() << "individual cards; Active is"
            << carried->caption();
    return true;
}

void CardStageController::leaveBentoPresentation()
{
    if (!m_active || m_presentation != CardPresentation::Bento) return;
    m_presentation = CardPresentation::Spread;
    m_host->setPagingShortcutsForCardStage(true);
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
}

bool CardStageController::promoteToActive(KWin::EffectWindow *window)
{
    if (!m_active || m_launcherGuestActive || !window || window->isDeleted()
        || !window->window() || m_presentation == CardPresentation::Bento) return false;
    finishCardGrab(false);
    const int index = liveCardIndex(window);
    if (index < 0) return false;
    if (m_presentation == CardPresentation::Active) {
        if (selectedWindow() == window) return true;
        parkActiveSnapshot();
    }
    if (!selectCardEntry(window)) return false;
    if (!enterActive()) {
        m_presentation = CardPresentation::Spread;
        return false;
    }
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    return true;
}

bool CardStageController::releasePairToBento(KWin::EffectWindow *carried,
                                             KWin::EffectWindow *partner)
{
    if (!m_active || m_launcherGuestActive || !carried || !partner
        || carried == partner || m_presentation == CardPresentation::Bento
        || liveCardIndex(partner) < 0) return false;
    finishCardGrab(false);
    QList<QPointer<KWin::EffectWindow>> pair{partner};
    // A window the user snapped in from the desktop is not a card yet, so it
    // has no individual ownership here to give up; the destination admits it.
    if (liveCardIndex(carried) >= 0) pair.append(carried);
    // §3: exactly these leave individual card ownership, in one step, so the
    // destination can publish two panes and nothing else.
    const auto departure = m_workspace.prepareGroupRemoval(pair,
        OwnershipTransition::CardToBento);
    if (!departure) return false;
    if (!publishOwnershipThenRecord(
            [&] { return m_workspace.commitGroupRemoval(*departure); },
            [&] {
                ++m_restoreGeneration;
                m_activeSettleTimer.stop();
                m_activeSettleRemaining = 0;
                for (const auto &window : pair) {
                    retireActiveIdentity(window);
                    if (m_activeRestore.window == window) m_activeRestore = {};
                    m_parkedRestores.removeIf([&](const auto &saved) {
                        return !saved.window || saved.window == window; });
                    m_originalCardStackingOrder.removeAll(window);
                }
                // §2: the display presents Bento; the remaining cards stay
                // owned and hidden behind it.
                m_presentation = CardPresentation::Bento;
                m_host->setPagingShortcutsForCardStage(false);
                if (m_workspace.windows().isEmpty()) {
                    m_active = false;
                    m_presentation = CardPresentation::Spread;
                    m_originalCardStackingOrder.clear();
                }
            })) return false;
    m_transferGuard.invalidate();
    m_host->cancelInputForCardStage();
    clearCardTransition();
    for (const auto &window : pair) {
        if (window && !window->isDeleted()) {
            KWin::effects->setElevatedWindow(window, false);
            m_host->unredirectForCardStage(window);
        }
    }
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "paired two cards into Bento;"
            << m_workspace.windows().size() << "individual cards remain";
    return true;
}

bool CardStageController::releaseCardToLiveBento(KWin::EffectWindow *card)
{
    if (!m_active || m_launcherGuestActive || !card || card->isDeleted()
        || !card->window() || m_presentation != CardPresentation::Bento
        || liveCardIndex(card) < 0) return false;
    finishCardGrab(false);
    // §8: exactly this card leaves individual ownership, in one step, so the
    // layout never publishes a pane this stage still names as a card.
    const QList<QPointer<KWin::EffectWindow>> leaving{card};
    const auto departure = m_workspace.prepareGroupRemoval(leaving,
        OwnershipTransition::CardToBento);
    if (!departure) return false;
    if (!publishOwnershipThenRecord(
            [&] { return m_workspace.commitGroupRemoval(*departure); },
            [&] {
                ++m_restoreGeneration;
                m_activeSettleTimer.stop();
                m_activeSettleRemaining = 0;
                retireActiveIdentity(card);
                if (m_activeRestore.window == card) m_activeRestore = {};
                m_parkedRestores.removeIf([&](const auto &saved) {
                    return !saved.window || saved.window == card; });
                m_originalCardStackingOrder.removeAll(card);
                // The display was already presenting Bento and still is; only
                // the membership behind it changed. An empty card stage still
                // has a live layout in front of it, so this stage stays active
                // rather than releasing the display it no longer draws.
            })) return false;
    m_transferGuard.invalidate();
    m_host->cancelInputForCardStage();
    clearCardTransition();
    KWin::effects->setElevatedWindow(card, false);
    m_host->unredirectForCardStage(card);
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "admitted a called card into live Bento;"
            << m_workspace.windows().size() << "individual cards remain";
    return true;
}

bool CardStageController::admitSleepingPaneAsCard(
    KWin::EffectWindow *window, const std::function<bool()> &commitSource,
    const NativeMoveSnapshot *restore)
{
    QPointer<KWin::LogicalOutput> tablet = m_host->tabletOutputForCardStage();
    if (m_cardGrabActive || m_launcherGuestActive || !tablet || !window
        || window->isDeleted() || !window->window() || !window->isNormalWindow()
        || !window->isMinimized()
        || !m_host->mayHoldWindowForCardStage(window)
        || window->isUserMove() || window->isUserResize()
        || liveCardIndex(window) >= 0) return false;
    QPointer<KWin::EffectWindow> arrival = window;
    QPointer<KWin::Window> client = window->window();
    const auto ticket = m_transferGuard.issue();
    // §5 keeps the record the window had before Bento placed it, so release
    // still returns it where it began. The caller supplies it because the
    // session that held it has already published a plan that no longer names
    // this window.
    const ActiveRestoreSnapshot incoming{
        .window = window,
        .geometry = restore ? restore->geometry : client->moveResizeGeometry(),
        .floatingGeometry = restore ? restore->floatingGeometry : client->geometryRestore(),
        .fullscreenRestoreGeometry = restore ? restore->fullscreenRestoreGeometry
                                             : client->fullscreenGeometryRestore(),
        .quickTileMode = restore ? restore->quickTileMode : client->quickTileMode(),
        .maximizeMode = restore ? restore->maximizeMode : client->maximizeMode(),
        .fullScreen = restore ? restore->fullScreen : client->isFullScreen(),
        .minimized = restore ? restore->minimized : client->isMinimized(),
        .valid = true,
    };
    const auto admission = m_workspace.prepareAdmission(arrival, false);
    if (!admission) return false;
    if (!m_workspace.commitAdmission(*admission, commitSource)) return false;
    m_originalCardStackingOrder.append(arrival);
    if (!m_active) {
        // A pair takes both cards, so the stage that gave them up owns nothing
        // and is not active. It has to own this one, and §2 gives it the only
        // presentation that fits: the display is showing the panes this window
        // just left, and a sleeping card sits behind them like every other
        // card this stage holds. Entering Spread would draw a stage whose one
        // member is asleep over a live layout.
        m_active = true;
        m_presentation = CardPresentation::Bento;
        m_host->setPagingShortcutsForCardStage(false);
    }
    const auto valid = [&] {
        return m_transferGuard.accepts(ticket)
            && arrival && !arrival->isDeleted() && client && arrival->window() == client
            && tablet && m_host->tabletOutputForCardStage() == tablet
            && KWin::effects->screens().contains(tablet.data());
    };
    // Membership is published. Everything below is presentation cleanup, so an
    // invalidation stops it rather than failing a transfer that already
    // happened.
    if (!valid()) return true;
    m_host->connectManagedWindowForCardStage(arrival);
    if (!valid()) return true;
    if (!managedRestore(arrival)) m_parkedRestores.append(incoming);
    // §7: sleeping and nonselected. It is given no geometry, is not raised,
    // and keeps the minimized state the user asked for.
    KWin::effects->setElevatedWindow(arrival, false);
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "took a minimized pane as a sleeping card;"
            << m_workspace.windows().size() << "individual cards remain";
    return true;
}

int CardStageController::returnCardsToDisplay()
{
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || !tablet || !KWin::effects->screens().contains(tablet)) return 0;
    // Where KWin intends a window to be, not where its last acknowledged frame
    // was: a Wayland client acknowledges a move later than KWin decides it.
    const KWin::EffectWindow *presented =
        m_presentation == CardPresentation::Active ? selectedWindow() : nullptr;
    int returned = 0;
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    for (const QPointer<KWin::EffectWindow> &window : std::as_const(m_workspace.windows())) {
        if (!window || window->isDeleted() || !window->window()) continue;
        KWin::Window *client = window->window();
        if (client->moveResizeOutput() == tablet
            || client->isInteractiveMove() || client->isInteractiveResize()) continue;
        // KWin may have given the window back a state it held on that layout.
        // A card holds none of them; its restore record keeps the originals.
        if (client->isFullScreen()) client->setFullScreen(false);
        if (client->maximizeMode() != KWin::MaximizeRestore)
            client->maximize(KWin::MaximizeRestore);
        if (client->quickTileMode() != KWin::QuickTileMode{})
            client->setQuickTileMode(KWin::QuickTileMode{}, window->frameGeometry().center());
        // Every card stands where the Active card does. Only the presented one
        // is drawn there; the rest are drawn in Spread's slots or not at all.
        const KWin::Rect target = window == presented
            ? activePlacement(tablet) : activeTarget(tablet);
        client->moveResize(KWin::RectF(target));
        ++returned;
        qInfo() << "Kadunce" << Revision << "returned card" << window->caption()
                << "to" << tablet->name() << "target" << target;
    }
    if (returned == 0) return 0;
    if (presented) m_activeSettleRemaining = 2;
    KWin::effects->addRepaintFull();
    return returned;
}

bool CardStageController::admitArrivalAsCard(KWin::EffectWindow *window)
{
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || m_cardGrabActive || m_launcherGuestActive || !tablet || !window
        || window->isDeleted() || !window->window() || !window->isNormalWindow()
        || !m_host->isManagedWindowForCardStage(window)
        || window->window()->moveResizeOutput() != tablet
        || window->isUserMove() || window->isUserResize()
        || liveCardIndex(window) >= 0) return false;
    // Admission appends the card after every other and selects it; the
    // selection this display had is put back until the caller chooses.
    const int previousSelection = m_workspace.selectedIndex();
    const auto admission = m_workspace.prepareAdmission(window, false);
    if (!admission || !m_workspace.commitAdmission(*admission, [] { return true; }))
        return false;
    m_workspace.selectIndex(previousSelection);
    // Published first, then described, as every other admission does. The
    // record is where KWin put it on this display, which is a place release
    // can always return it to.
    retainManagedOwnership(window);
    m_originalCardStackingOrder.append(window);
    m_host->connectManagedWindowForCardStage(window);
    KWin::effects->setElevatedWindow(window, false);
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "took" << window->caption()
            << "as a card after its display went away;"
            << m_workspace.windows().size() << "individual cards";
    return true;
}

bool CardStageController::admitDisplacedPaneAsHiddenCard(
    KWin::EffectWindow *window, const std::function<bool()> &commitSource,
    const NativeMoveSnapshot *restore)
{
    QPointer<KWin::LogicalOutput> tablet = m_host->tabletOutputForCardStage();
    if (!m_active || m_presentation != CardPresentation::Bento || m_cardGrabActive
        || m_launcherGuestActive || !tablet || !window || window->isDeleted()
        || !window->window() || !window->isNormalWindow()
        || !m_host->isManagedWindowForCardStage(window)
        || window->isUserMove() || window->isUserResize()
        || liveCardIndex(window) >= 0) return false;
    QPointer<KWin::EffectWindow> arrival = window;
    QPointer<KWin::Window> client = window->window();
    const auto ticket = m_transferGuard.issue();
    // §5 keeps the record the window had before Bento placed it, so release
    // still returns it where it began rather than to a pane rectangle. The
    // caller supplies it because the session that held it has already
    // published a plan that no longer names this window.
    const ActiveRestoreSnapshot incoming{
        .window = window,
        .geometry = restore ? restore->geometry : client->moveResizeGeometry(),
        .floatingGeometry = restore ? restore->floatingGeometry : client->geometryRestore(),
        .fullscreenRestoreGeometry = restore ? restore->fullscreenRestoreGeometry
                                             : client->fullscreenGeometryRestore(),
        .quickTileMode = restore ? restore->quickTileMode : client->quickTileMode(),
        .maximizeMode = restore ? restore->maximizeMode : client->maximizeMode(),
        .fullScreen = restore ? restore->fullScreen : client->isFullScreen(),
        .minimized = restore ? restore->minimized : client->isMinimized(),
        .valid = true,
    };
    const auto admission = m_workspace.prepareAdmission(arrival, false);
    if (!admission) return false;
    if (!m_workspace.commitAdmission(*admission, commitSource)) return false;
    m_originalCardStackingOrder.append(arrival);
    const auto valid = [&] {
        return m_transferGuard.accepts(ticket)
            && arrival && !arrival->isDeleted() && client && arrival->window() == client
            && tablet && m_host->tabletOutputForCardStage() == tablet
            && KWin::effects->screens().contains(tablet.data());
    };
    // Membership is published. Everything below is presentation cleanup, so an
    // invalidation stops it rather than failing a transfer that already
    // happened.
    if (!valid()) return true;
    m_host->connectManagedWindowForCardStage(arrival);
    if (!valid()) return true;
    if (!managedRestore(arrival)) m_parkedRestores.append(incoming);
    // §2: the panes are the display and this card is one of the ones behind
    // them. It is given no geometry of its own and does not become selected.
    KWin::effects->setElevatedWindow(arrival, false);
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "took a displaced pane as a hidden card;"
            << m_workspace.windows().size() << "individual cards remain";
    return true;
}

void CardStageController::pageHorizontal(int delta)
{
    if (!m_active || m_presentation == CardPresentation::Bento) {
        return;
    }
    m_host->cancelInputForCardStage();
    if (m_arrivalWindow) clearCardTransition();
    finishCardGrab(false);
    const bool wasActive = m_presentation == CardPresentation::Active;
    if (!wasActive && m_workspace.count() == 2 && !m_launcherGuestActive) {
        if (delta == 0 || (delta < 0 ? -1 : 1) != m_workspace.pairNeighborSide()) return;
    }
    if (delta == 0) return;
    if (!wasActive && !m_launcherGuestActive) {
        captureCardTransition(false, true);
        m_rowPageTransition = true;
    }
    const bool activeStack = wasActive
        && m_workspace.stackSizeForId(m_workspace.selectedId()) > 1;
    if (wasActive) {
        parkActiveSnapshot();
    }
    if (activeStack) {
        m_workspace.pageStack(delta);
    } else {
        m_workspace.page(delta);
    }
    if (wasActive && !enterActive()) {
        m_presentation = CardPresentation::Spread;
    }
    if (!wasActive) anchorRowTransition();
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce horizontal navigation selected"
            << m_workspace.selectedId() << "of" << m_workspace.count();
}

bool CardStageController::beginLauncherGuest()
{
    if (!m_active || m_presentation != CardPresentation::Spread
        || m_workspace.windows().isEmpty() || m_cardGrabActive) {
        return false;
    }
    m_host->cancelInputForCardStage();
    captureCardTransition();
    m_arrivalTimer.stop();
    m_arrivalWindow.clear();
    m_arrivalExpanding = false;
    m_launcherGuestGroupCount = m_workspace.count();
    m_launcherGuestArrival = false;
    m_launcherGuestPrimarySide = m_workspace.count() == 2
        ? -m_workspace.pairNeighborSide() : 1;
    m_launcherGuestPrimaryWindow = selectedWindow();
    m_launcherGuestSecondaryWindow = m_workspace.count() > 1
        ? m_workspace.windows().value(m_workspace.idAtOffset(-1) - 1) : nullptr;
    m_launcherGuestOffset = 0.0;
    m_launcherGuestTransitionFrom = 0.0;
    m_launcherGuestTransitionTimer.invalidate();
    m_launcherGuestPendingPage = 0;
    m_launcherGuestActive = true;
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    return true;
}

void CardStageController::updateLauncherGuest(double horizontalDelta)
{
    if (!m_launcherGuestActive) {
        return;
    }
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!tablet) {
        return;
    }
    const KWin::RectF work = KWin::effects->clientArea(
        KWin::MaximizeArea, tablet);
    const SpreadLayout layout = makeSpreadLayout(
        work.x(), work.y(), work.width(), work.height());
    const double pitch = layout.cards[1].width + layout.gutter;
    m_launcherGuestOffset = std::clamp(
        horizontalDelta, -pitch, pitch);
    KWin::effects->addRepaintFull();
}

bool CardStageController::finishLauncherGuest(double horizontalDelta)
{
    if (!m_launcherGuestActive) {
        return false;
    }
    if (std::abs(horizontalDelta) < LauncherGuestCommitDistance) {
        m_launcherGuestOffset = 0.0;
        m_launcherGuestTransitionFrom = 0.0;
        m_launcherGuestTransitionTimer.invalidate();
        KWin::effects->addRepaintFull();
        return false;
    }

    const double previewFrom = std::clamp(
        std::abs(m_launcherGuestOffset) / LauncherGuestCommitDistance,
        0.0, 1.0) * LauncherGuestDragPreview;
    m_launcherGuestOffset = horizontalDelta > 0.0
        ? LauncherGuestCommitDistance : -LauncherGuestCommitDistance;
    m_launcherGuestTransitionFrom = previewFrom;
    m_launcherGuestTransitionTimer.restart();
    const int incomingSide = horizontalDelta > 0.0 ? -1 : 1;
    m_launcherGuestPendingPage = m_workspace.count() > 1
        && incomingSide != m_launcherGuestPrimarySide ? -1 : 0;
    KWin::effects->addRepaintFull();
    return true;
}

void CardStageController::endLauncherGuest()
{
    if (!m_launcherGuestActive && qFuzzyIsNull(m_launcherGuestOffset)) {
        return;
    }
    m_host->cancelInputForCardStage();
    if (!m_launcherGuestArrival && m_launcherGuestPendingPage != 0) {
        m_workspace.page(m_launcherGuestPendingPage);
    }
    // Keep the guest's established landing intact, then ease that landing
    // into the larger pair. The shoulder stays on the side it actually used.
    if (!m_launcherGuestArrival && m_workspace.count() == 2 && m_launcherGuestTransitionTimer.isValid()) {
        m_workspace.setPairNeighborSide(m_launcherGuestOffset < 0.0 ? -1 : 1);
        if (auto *output = m_host->tabletOutputForCardStage()) {
            const auto work = KWin::effects->clientArea(KWin::MaximizeArea, output);
            if (work.width() > 0 && work.height() > 0) {
                m_previewOrigins.clear();
                for (const auto &window : std::as_const(m_workspace.windows())) {
                    if (!window || window->isDeleted()) continue;
                    const int slot = m_workspace.sameStack(liveCardIndex(window) + 1,
                        m_workspace.selectedId()) ? 0 : m_workspace.pairNeighborSide();
                    const auto rect = slot == 0 ? cardTargetForSlot(output, slot)
                        : launcherGuestTargetForSlot(output, slot);
                    m_previewOrigins.append({window, QRectF(
                        (rect.x() - work.x()) / work.width(),
                        (rect.y() - work.y()) / work.height(),
                        rect.width() / work.width(), rect.height() / work.height())});
                }
                m_previewTransition.start();
            }
        }
    }
    m_launcherGuestPendingPage = 0;
    m_launcherGuestActive = false;
    m_launcherGuestArrival = false;
    m_launcherGuestOffset = 0.0;
    m_launcherGuestPrimaryWindow.clear();
    m_launcherGuestSecondaryWindow.clear();
    m_launcherGuestTransitionFrom = 0.0;
    m_launcherGuestTransitionTimer.invalidate();
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
}

void CardStageController::pageStack(int delta)
{
    if (!m_active || m_presentation != CardPresentation::Spread
        || delta == 0 || selectedIsBentoProjection()
        || m_workspace.stackSizeForId(m_workspace.selectedId()) <= 1) {
        return;
    }
    m_host->cancelInputForCardStage();
    if (m_arrivalWindow) clearCardTransition(); // Explicit browsing wins over auto-expand.
    finishCardGrab(false);
    // Capture complete fan poses, including an interrupted browse transition.
    // All input routes already share this action; do not add device-specific motion.
    captureCardTransition(false, true);
    m_stackBrowseOutgoing = selectedWindow();
    m_workspace.pageStack(delta);
    m_stackBrowseDirection = delta < 0 ? -1 : 1;
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce stack selected member"
            << m_workspace.selectedId() << "position"
            << m_workspace.stackActivePositionForId(m_workspace.selectedId()) + 1
            << "of" << m_workspace.stackSizeForId(m_workspace.selectedId());
}

void CardStageController::rebuildLiveCards()
{
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    m_workspace.clear();
    m_presentedActive = nullptr;
    m_bentoProjectionWindows.clear();
    m_bentoProjectionPaneWindows.clear();
    m_bentoProjectionSession.reset();
    if (!tablet) {
        return;
    }
    KWin::EffectWindow *active = KWin::effects->activeWindow();
    int activeIndex = -1;
    QList<QPointer<KWin::EffectWindow>> admitted;
    const QList<KWin::EffectWindow *> windows = KWin::effects->stackingOrder();
    for (KWin::EffectWindow *window : windows) {
        if (!m_host->isManagedWindowForCardStage(window)
            || window->screen() != tablet) {
            continue;
        }
        if (window == active) {
            activeIndex = admitted.size();
        }
        m_host->connectManagedWindowForCardStage(window);
        admitted.append(window);
    }
    // Entry publishes the whole admitted set before any of it is described.
    // A restore record is defined only for a published card, so capture here
    // cannot precede the reset that publishes them.
    publishOwnershipThenRecord(
        [&] {
            m_workspace.reset(admitted,
                activeIndex >= 0 ? activeIndex : admitted.size() - 1);
            return true;
        },
        [&] {
            for (const auto &window : admitted) retainManagedOwnership(window);
            if (!admitted.isEmpty()) m_originalCardStackingOrder = admitted;
        });
}

void CardStageController::retainManagedOwnership(KWin::EffectWindow *window)
{
    if (!window || window->isDeleted() || !window->window()
        || liveCardIndex(window) < 0 || m_activeRestore.window == window) return;
    for (const auto &saved : std::as_const(m_parkedRestores))
        if (saved.window == window) return;
    auto *client = window->window();
    // Membership owns restoration, independently of the selected presentation.
    // Use KWin's accepted placement, which can precede a Wayland buffer/frame.
    m_parkedRestores.append(ActiveRestoreSnapshot{
        .window = window,
        .geometry = client->moveResizeGeometry(),
        .floatingGeometry = client->geometryRestore(),
        .fullscreenRestoreGeometry = client->fullscreenGeometryRestore(),
        .quickTileMode = client->quickTileMode(),
        .maximizeMode = client->maximizeMode(),
        .fullScreen = client->isFullScreen(),
        .minimized = client->isMinimized(),
        .valid = true,
    });
    ++m_restoreGeneration;
}

bool CardStageController::enterActive()
{
    ++m_restoreGeneration;
    m_host->cancelInputForCardStage();
    clearCardTransition();
    m_activeSettleTimer.stop();
    m_activeSettleRemaining = 2;
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    KWin::EffectWindow *effectWindow = selectedWindow();
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!m_host->isManagedWindowForCardStage(effectWindow) || !tablet
        || effectWindow->screen() != tablet || !effectWindow->window()) {
        qWarning() << "Kadunce" << Revision
                   << "cannot admit the selected card to Active";
        return false;
    }

    KWin::Window *client = effectWindow->window();
    for (const QPointer<KWin::EffectWindow> &window :
         std::as_const(m_workspace.windows())) {
        if (window && !window->isDeleted()) {
            m_host->unredirectForCardStage(window);
        }
    }
    parkActiveSnapshot();
    retainManagedOwnership(effectWindow);
    const auto retained = std::find_if(m_parkedRestores.begin(), m_parkedRestores.end(),
        [effectWindow](const auto &saved) { return saved.window == effectWindow; });
    if (retained != m_parkedRestores.end()) {
        m_activeRestore = *retained;
        m_parkedRestores.erase(retained);
    } else m_activeRestore = ActiveRestoreSnapshot{
        .window = effectWindow,
        .geometry = effectWindow->frameGeometry(),
        .floatingGeometry = client->geometryRestore(),
        .fullscreenRestoreGeometry = client->fullscreenGeometryRestore(),
        .quickTileMode = client->quickTileMode(),
        .maximizeMode = client->maximizeMode(),
        .fullScreen = client->isFullScreen(),
        .minimized = client->isMinimized(),
        .valid = true,
    };
    if (client->isFullScreen()) {
        client->setFullScreen(false);
    }
    if (client->maximizeMode() != KWin::MaximizeRestore) {
        client->maximize(KWin::MaximizeRestore);
    }
    if (client->quickTileMode() != KWin::QuickTileMode{}) {
        client->setQuickTileMode(KWin::QuickTileMode{},
                                 effectWindow->frameGeometry().center());
    }

    restoreOriginalStackingOrder();
    const KWin::Rect target = activeTarget(tablet);
    client->moveResize(KWin::RectF(target));
    // Publish Active before asking KWin to activate the client. The resulting
    // windowActivated signal is synchronous on some Plasma versions and must
    // not be mistaken for a second task-manager request.
    m_presentation = CardPresentation::Active;
    m_presentedActive = effectWindow;
    // All entry paths must retire Spread's temporary compositor elevation.
    // Active is a native window; panel popups must retain their normal layers.
    syncSelectedElevation();
    m_activeSettleTimer.start();
    // A keyboard may already be up over the card that just arrived.
    refreshKeyboardReveal();
    m_host->cardActivatedForCardStage(effectWindow);
    KWin::workspace()->raiseWindow(client);
    KWin::workspace()->activateWindow(client, true);
    qInfo() << "Kadunce" << Revision
            << "entered interactive Active with" << effectWindow->caption();
    return true;
}

void CardStageController::restoreActiveSnapshot()
{
    ++m_restoreGeneration;
    m_activeSettleTimer.stop();
    m_activeSettleRemaining = 0;
    m_keyboardRevealTimer.stop();
    m_keyboardRevealRelease.stop();
    m_keyboardReveal.reset();
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    m_restoredMinimizations.clear();
    parkActiveSnapshot();
    const auto snapshots = std::exchange(m_parkedRestores, {});
    for (const auto &snapshot : snapshots) {
        if (!snapshot.valid || !snapshot.window || snapshot.window->isDeleted()
            || !snapshot.window->window()) continue;
        auto *client = snapshot.window->window();
        restoreWindowState(client, snapshot, snapshot.geometry, true, true, false);
        if (snapshot.minimized && snapshot.window && !snapshot.window->isDeleted())
            m_restoredMinimizations.push_back(std::make_unique<RestoredMinimization>(client,
                RestoredMinimization::Target{client->moveResizeGeometry(), snapshot.maximizeMode,
                    snapshot.quickTileMode, snapshot.fullScreen}));
    }
}

void CardStageController::parkActiveSnapshot()
{
    m_activeSettleTimer.stop();
    // A card leaving Active stays a card, so it leaves from its own placement
    // rather than from wherever the keyboard lifted it.
    if (m_keyboardReveal && m_keyboardReveal->window == m_activeRestore.window) {
        putBackKeyboardReveal();
    }
    if (m_activeRestore.valid && m_activeRestore.window) {
        const auto window = m_activeRestore.window;
        m_parkedRestores.removeIf([&](const auto &s) { return !s.window || s.window == window; });
        m_parkedRestores.append(m_activeRestore);
    }
    m_activeRestore = {};
}

void CardStageController::retireActiveIdentity(const KWin::EffectWindow *window)
{
    if (m_presentedActive == window) m_presentedActive = nullptr;
}

void CardStageController::forgetManagedRestore(KWin::EffectWindow *window)
{
    retireActiveIdentity(window);
    if (m_activeRestore.window == window) m_activeRestore = {};
    m_parkedRestores.removeIf([window](const auto &s) { return !s.window || s.window == window; });
    m_bentoProjectionWindows.removeIf(
        [window](const auto &projected) { return !projected || projected == window; });
    m_bentoProjectionPaneWindows.removeIf(
        [window](const auto &pane) { return !pane || pane == window; });
    if (m_bentoProjectionSession) {
        auto &projection = *m_bentoProjectionSession;
        for (int index = projection.panes.size() - 1; index >= 0; --index) {
            if (!projection.panes[index].window
                || projection.panes[index].window == window) {
                projection.panes.removeAt(index);
                if (index < int(projection.rects.size()))
                    projection.rects.erase(projection.rects.begin() + index);
            }
        }
        projection.sleeping.removeIf(
            [window](const auto &member) { return !member.window || member.window == window; });
        projection.stackingOrder.removeIf(
            [window](const auto &member) { return !member || member == window; });
        if (projection.lead == window) {
            projection.lead = projection.panes.isEmpty()
                ? nullptr : projection.panes.first().window;
        }
        if (projection.sideWindow == window) {
            projection.sideWindow.clear();
            projection.side.reset();
        }
        if (projection.panes.isEmpty()) m_bentoProjectionSession.reset();
    }
    ++m_restoreGeneration;
}

std::optional<NativeMoveSnapshot> CardStageController::managedRestore(KWin::EffectWindow *window) const
{
    if (!m_active || !window || window->isDeleted() || !window->window()) return std::nullopt;
    const ActiveRestoreSnapshot *saved = m_activeRestore.window == window ? &m_activeRestore : nullptr;
    if (!saved) for (const auto &s : m_parkedRestores) {
        if (s.window == window) { saved = &s; break; }
    }
    if (!saved || !saved->valid) return std::nullopt;
    return NativeMoveSnapshot{window->window(), window->screen(), saved->geometry,
        saved->floatingGeometry, saved->fullscreenRestoreGeometry, saved->maximizeMode,
        saved->quickTileMode, saved->fullScreen, saved->minimized};
}

bool CardStageController::admitTransferredWindowToTablet(
    KWin::EffectWindow *window, const std::function<bool()> &commitSource,
    const QRectF &carriedOrigin, const NativeMoveSnapshot *restore)
{
    QPointer<KWin::LogicalOutput> tablet = m_host->tabletOutputForCardStage();
    if (!tablet || !window || window->isDeleted() || !window->window()
        || !window->isNormalWindow()
        || !m_host->isManagedWindowForCardStage(window)) {
        return false;
    }
    QPointer<KWin::EffectWindow> arrival = window;
    QPointer<KWin::Window> client = window->window();
    const ActiveRestoreSnapshot incoming{
        .window = window,
        .geometry = restore ? restore->geometry : window->frameGeometry(),
        .floatingGeometry = restore ? restore->floatingGeometry : client->geometryRestore(),
        .fullscreenRestoreGeometry = restore ? restore->fullscreenRestoreGeometry : client->fullscreenGeometryRestore(),
        .quickTileMode = restore ? restore->quickTileMode : client->quickTileMode(),
        .maximizeMode = restore ? restore->maximizeMode : client->maximizeMode(),
        .fullScreen = restore ? restore->fullScreen : client->isFullScreen(),
        .minimized = restore ? restore->minimized : client->isMinimized(),
        .valid = true,
    };
    const KWin::RectF target(activeTarget(tablet));
    const auto ticket = m_transferGuard.issue();
    if (!target.isValid() || !KWin::effects->screens().contains(tablet.data())
        || window->isUserMove() || window->isUserResize()) return false;
    const bool newMember = liveCardIndex(window) < 0;
    const bool animateArrival = m_active && m_presentation == CardPresentation::Spread
        && !m_launcherGuestActive;
    const int previousSelection = m_workspace.selectedIndex();
    if (newMember) {
        // A second arrival must not replace a held card's rollback transaction.
        if (m_cardGrabActive) return false;
        const auto admission = m_workspace.prepareAdmission(arrival,
            animateArrival || m_launcherGuestActive);
        if (!admission) return false;
        // Capture the old arrangement before publishing the new member.
        if (animateArrival) captureCardTransition();
        if (!m_workspace.commitAdmission(*admission, commitSource)) return false;
        m_originalCardStackingOrder.append(arrival);
        // A fresh receiver must own the card before applying Active geometry.
        // Otherwise the first upward swipe can start an ordinary native resize.
        if (!m_active) {
            m_active = true;
            m_host->setPagingShortcutsForCardStage(true);
        }
    } else if (!commitSource()) {
        return false;
    }
    const auto valid = [&] {
        return m_transferGuard.accepts(ticket)
            && arrival && !arrival->isDeleted() && client && arrival->window() == client
            && tablet && m_host->tabletOutputForCardStage() == tablet
            && KWin::effects->screens().contains(tablet.data())
            && !arrival->isUserMove() && !arrival->isUserResize();
    };
    if (!valid()) return true;
    if (newMember) {
        // Both memberships are committed before any native/presentation cleanup.
        m_host->connectManagedWindowForCardStage(arrival);
        m_host->cancelInputForCardStage();
        if (!valid()) return true;
        m_presentation = CardPresentation::Spread;
        parkActiveSnapshot();
        if (!valid()) return true;
    }
    // A source record belongs to cancellation until commit. Same-output
    // admission may retain it; cross-output adoption establishes a new receiver
    // origin from KWin's accepted tablet placement before Active sizing.
    const bool sameOutput = restore ? restore->output == tablet : window->screen() == tablet;
    if (sameOutput && m_active && !managedRestore(arrival)) m_parkedRestores.append(incoming);
    if (!adoptPublishedCard(client.data(), tablet.data(),
            [&] { return liveCardIndex(arrival) >= 0; },
            [&] { retainManagedOwnership(arrival); }, target, valid)) {
        return true;
    }
    if (!m_active) {
        KWin::workspace()->raiseWindow(client);
        if (!valid()) return true;
        KWin::workspace()->activateWindow(client, true);
        return true;
    }
    // Reuse the same centered insertion and settling sequence as app launches.
    // Already-admitted windows use the existing selection/arrival path.
    if (newMember) {
        finishNewArrival(arrival, animateArrival, previousSelection);
    } else {
        stageWindowArrival(arrival);
    }
    // The receiver owns presentation. Seed its existing center/expand sequence
    // from the released face, not an invented off-screen Spread slot. Do
    // this after admission cleanup, which can cancel the source carry.
    if (valid() && animateArrival && m_presentation == CardPresentation::Spread
        && m_arrivalWindow == arrival && carriedOrigin.isValid()) {
        const auto work = KWin::effects->clientArea(KWin::MaximizeArea, tablet);
        if (work.width() > 0 && work.height() > 0) {
            m_previewOrigins.removeIf([&](const auto &origin) { return origin.window == arrival; });
            m_previewOrigins.append({arrival, QRectF(
                (carriedOrigin.x() - work.x()) / work.width(),
                (carriedOrigin.y() - work.y()) / work.height(),
                carriedOrigin.width() / work.width(),
                carriedOrigin.height() / work.height())});
            KWin::effects->addRepaintFull();
        }
    }
    return true;
}

void CardStageController::startArrivalTimer(KWin::EffectWindow *window)
{
    m_arrivalWindow = window;
    m_arrivalExpanding = false;
    m_arrivalWait.start();
    m_arrivalTimer.start(PreviewTransitionDuration);
    qInfo() << "Kadunce new app settling at center" << window->caption();
}

void CardStageController::stageWindowArrival(KWin::EffectWindow *window)
{
    if (!m_active || !window || window->isDeleted() || liveCardIndex(window) < 0) return;
    m_host->cancelInputForCardStage();
    if (m_presentation != CardPresentation::Spread) {
        handleWindowActivated(window);
        return;
    }
    const bool replacesGuest = m_launcherGuestActive;
    captureCardTransition(replacesGuest);
    if (replacesGuest) {
        // The new app replaces the guest aperture, never a shoulder. Keep
        // every existing app at its current painted origin during the reveal.
        auto *output = m_host->tabletOutputForCardStage();
        if (!output) return;
        const auto work = KWin::effects->clientArea(KWin::MaximizeArea, output);
        if (work.width() <= 0 || work.height() <= 0) return;
        const auto center = launcherGuestTarget(output);
        m_previewOrigins.removeIf([window](const auto &origin) { return origin.window == window; });
        m_previewOrigins.append({window, QRectF(
            (center.x() - work.x()) / work.width(), (center.y() - work.y()) / work.height(),
            center.width() / work.width(), center.height() / work.height())});
        m_launcherGuestArrival = true;
        m_launcherGuestPendingPage = 0;
        m_launcherGuestTransitionTimer.invalidate();
        m_launcherGuestOffset = 0.0;
    }
    const int id = liveCardIndex(window) + 1;
    for (int i = 0; i < m_workspace.count() && !m_workspace.sameStack(id, m_workspace.selectedId()); ++i)
        m_workspace.page(1);
    for (int i = 0; i < m_workspace.stackSizeForId(id) && m_workspace.selectedId() != id; ++i)
        m_workspace.pageStack(1);
    if (replacesGuest && m_workspace.count() == 2)
        m_workspace.setPairNeighborSide(m_launcherGuestPrimarySide);
    startArrivalTimer(window);
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
}

bool CardStageController::handleWindowAdded(KWin::EffectWindow *window)
{
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || !window || !tablet || !window->isNormalWindow()
        || !m_host->isManagedWindowForCardStage(window)
        || window->screen() != tablet || liveCardIndex(window) >= 0) {
        return false;
    }
    // §2: the display is presenting its panes, and this path publishes a
    // Spread presentation and an Active card. Refusing is what makes the
    // caller retire the layout first rather than remember to; a card drawn
    // over live panes is the state §2 does not name.
    if (m_presentation == CardPresentation::Bento) return false;

    const bool animateArrival = m_presentation == CardPresentation::Spread
        && !m_launcherGuestActive && !m_cardGrabActive;
    m_host->cancelInputForCardStage();
    if (animateArrival) captureCardTransition();
    finishCardGrab(false);
    if (m_presentation == CardPresentation::Active) {
        parkActiveSnapshot();
    }
    m_presentation = CardPresentation::Spread;
    int previousSelection = m_workspace.selectedIndex();
    m_workspace.append(window, animateArrival || m_launcherGuestActive);
    retainManagedOwnership(window);
    m_originalCardStackingOrder.append(window);
    m_host->connectManagedWindowForCardStage(window);
    finishNewArrival(window, animateArrival, previousSelection);
    return true;
}

void CardStageController::finishNewArrival(KWin::EffectWindow *window,
    bool animateArrival, int previousSelection)
{
    if (m_launcherGuestActive) {
        if (m_workspace.selectedIndex() <= previousSelection) ++previousSelection;
        m_workspace.selectIndex(previousSelection);
        syncSelectedElevation();
        KWin::effects->addRepaintFull();
        return; // Admission must not bypass the guest's matching handoff.
    }
    if (animateArrival) {
        startArrivalTimer(window);
        syncSelectedElevation();
        KWin::effects->addRepaintFull();
        return;
    }
    syncSelectedElevation();
    if (!enterActive()) {
        m_presentation = CardPresentation::Spread;
    }
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "admitted new card"
            << liveCardIndex(window) + 1 << window->caption() << "as Active";
}

void CardStageController::handleWindowClosed(KWin::EffectWindow *window)
{
    m_originalCardStackingOrder.removeAll(window);
    const int closedIndex = liveCardIndex(window);
    if (!m_active || closedIndex < 0) {
        return;
    }

    m_host->cancelInputForCardStage();

    if (m_workspace.count() == 3 && m_workspace.stackSizeForId(closedIndex + 1) == 1)
        captureCardTransition();
    finishCardGrab(false);
    const bool closedActive = m_activeRestore.window == window;
    if (closedActive) {
        m_activeRestore = ActiveRestoreSnapshot{};
    }
    forgetManagedRestore(window);
    const bool removed = m_workspace.removeAt(closedIndex);
    if (m_workspace.windows().isEmpty()) {
        release();
        return;
    }
    if (!removed) {
        qWarning() << "Kadunce" << Revision
                   << "could not remove closed card in place; rebuilding";
        if (m_presentation == CardPresentation::Active) {
            parkActiveSnapshot();
        }
        m_presentation = CardPresentation::Spread;
        rebuildLiveCards();
    } else if (closedActive) {
        m_presentation = CardPresentation::Spread;
    }
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
}

void CardStageController::handleWindowActivated(KWin::EffectWindow *window)
{
    if (m_applyingWindowState || !m_active || !window || m_cardGrabActive) {
        return;
    }
    if (window == m_arrivalWindow) return; // Let its center/expand sequence finish.
    // §2: this stage keeps its cards hidden while the display presents its
    // Bento layout. An activation reaching here was not routed into the
    // layout, and drawing a card over live panes is the state §2 does not
    // name, so the presentation stands.
    if (m_presentation == CardPresentation::Bento) return;
    if (m_arrivalWindow) clearCardTransition(); // An explicit different activation wins.
    const int targetIndex = liveCardIndex(window);
    if (targetIndex < 0) {
        return;
    }
    const int targetId = targetIndex + 1;
    if (m_presentation == CardPresentation::Active
        && m_workspace.selectedId() == targetId) {
        return;
    }

    m_host->cancelInputForCardStage();
    finishCardGrab(false);
    if (m_presentation == CardPresentation::Active) {
        parkActiveSnapshot();
    }
    m_presentation = CardPresentation::Spread;

    // Select through the stack model so a task-manager click can address both
    // standalone cards and a specific stack member while selection remains
    // centralized in the spread model.
    for (int step = 0;
         step < m_workspace.count()
         && !m_workspace.sameStack(targetId, m_workspace.selectedId());
         ++step) {
        m_workspace.page(1);
    }
    if (usesBentoProjectionAperture(window)) {
        syncSelectedElevation();
        (void)resumeSelectedBentoProjection();
        return;
    }
    const int stackSize = m_workspace.stackSizeForId(targetId);
    for (int step = 0;
         step < stackSize && m_workspace.selectedId() != targetId;
         ++step) {
        m_workspace.pageStack(1);
    }

    syncSelectedElevation();
    if (!enterActive()) {
        m_presentation = CardPresentation::Spread;
    }
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision
            << "promoted externally activated card" << window->caption();
}

void CardStageController::handleActiveGeometryChanged(
    KWin::EffectWindow *window)
{
    if (!m_active || m_presentation != CardPresentation::Active
        || !m_activeRestore.valid || m_activeRestore.window != window
        || !window->window()) {
        return;
    }
    if (m_applyingWindowState) return;
    // Late configure replies still need Active-size normalization, but never
    // resize recursively inside the notification or fight explicit user input.
    const auto *client = window->window();
    // Native start owns move/resize admission. Reported state may still describe
    // an older Wayland configure while our requested Active state is pending.
    if (client->isInteractiveMove() || client->isInteractiveResize()) return;
    if (client->isRequestedFullScreen()
        || client->requestedMaximizeMode() != KWin::MaximizeRestore
        || client->requestedQuickTileMode() != KWin::QuickTileMode{}) {
        handleManualWindowChange(window);
    } else if (m_activeSettleRemaining > 0 && !m_activeSettleTimer.isActive()) {
        m_activeSettleTimer.start();
    }
}

void CardStageController::handleManualWindowChange(KWin::EffectWindow *window)
{
    if (m_applyingWindowState || !m_active
        || m_presentation != CardPresentation::Active
        || !m_activeRestore.valid || m_activeRestore.window != window) return;
    // The user's new state wins. Release presentation without replaying the
    // old snapshot over an in-progress move/resize or explicit state request.
    m_activeRestore = ActiveRestoreSnapshot{};
    release();
}

} // namespace Kadunce
