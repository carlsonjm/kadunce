/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CardStageController.h"

#include "ActiveStep.h"
#include "HeldTuck.h"
#include "RowCardTarget.h"
#include "KeyboardRoom.h"
#include "HeldCardGeometry.h"
#include "MotionTime.h"
#include "BentoCompositeGeometry.h"
#include "OwnershipHandoff.h"
#include "NeighborStackPose.h"
#include "RowPageMotion.h"
#include "StackBrowseMotion.h"
#include "SpreadStroke.h"
#include "SpreadLayout.h"
#include "FocusedPairLayout.h"
#include "WindowStateRestore.h"
#include "NativePlacement.h"

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
constexpr int PreviewTransitionDuration = 280;
// A Stack parting for, paging under or closing after a card held in it.
constexpr int HeldStackStepDuration = 360;
constexpr int StackOutlineFadeDuration = 180;
// Let go short of halfway, Spread opening under three fingers goes back to the
// Active card over this long.
constexpr int OpenSpreadReturnDuration = 220;
constexpr int ArrivalExpandDuration = 220;
constexpr double LauncherGuestCommitDistance = 58.0;
constexpr int LauncherGuestTransitionDuration = 220;
constexpr double LauncherGuestDragPreview = 0.18;
// How long a flicked app has to close before its card comes back, and the
// most a card that asked a question waits to be answered in Spread.
constexpr int ThrownCloseWait = 2000;
// How long a flicked app that draws after it was asked to close has to go
// before what it drew counts as a question asked inside its own window.
constexpr int ThrownQuestionWait = 400;
}

KWin::RectF CardStageHost::workAreaForCardStage(const KWin::LogicalOutput *output) const
{
    return KWin::effects->clientArea(KWin::MaximizeArea, output);
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
            m_arrivalTimer.start(motion(ArrivalExpandDuration));
        } else {
            clearCardTransition();
            (void)enterActive();
        }
        KWin::effects->addRepaintFull();
    });
    // Keyboard and focus notifications arrive in bursts and from inside the
    // compositor's own handling, so the room is made once, after.
    m_openReturnTimer.setSingleShot(true);
    QObject::connect(&m_openReturnTimer, &QTimer::timeout, &m_openReturnTimer, [this]() {
        // Back where the fingers started: the Active card, as it was.
        m_openProgress.reset();
        m_openReturn.invalidate();
        if (m_active && m_presentation == CardPresentation::Spread && enterActive()) {
            syncSelectedElevation();
            qInfo() << "Kadunce" << Revision << "went back to Active from three fingers let go short";
        }
        KWin::effects->addRepaintFull();
    });
    m_keyboardRoomTimer.setSingleShot(true);
    m_keyboardRoomTimer.setInterval(0);
    QObject::connect(&m_keyboardRoomTimer, &QTimer::timeout,
                     &m_keyboardRoomTimer, [this]() { updateKeyboardRoom(); });
    // The bottom panels return a moment after the keyboard leaves. Until then
    // the work area still includes their room, so the card keeps the
    // placement it had before the keyboard came.
    m_keyboardRoomRelease.setSingleShot(true);
    m_keyboardRoomRelease.setInterval(1000);
    QObject::connect(&m_keyboardRoomRelease, &QTimer::timeout,
                     &m_keyboardRoomRelease, [this]() { m_keyboardRoom.reset(); });
    // The keys move by a new frame every refresh, so a moment without one is
    // the keys at rest, when a client is asked for the room they take. The
    // moment is longer than a finger's pause in a drag, so a hand that slows
    // is not taken for keys that have stopped.
    m_keyboardRestTimer.setSingleShot(true);
    m_keyboardRestTimer.setInterval(100);
    QObject::connect(&m_keyboardRestTimer, &QTimer::timeout,
                     &m_keyboardRestTimer, [this]() { updateKeyboardRoom(true); });
    m_keyboardHeadingTimer.setSingleShot(true);
    QObject::connect(&m_keyboardHeadingTimer, &QTimer::timeout,
                     &m_keyboardHeadingTimer, [this]() {
        auto *client = keyboardRoomClient();
        if (!client || !m_keyboardHeadingTop) return;
        askKeyboardRoom(client, keyboardRoomFor(client, *m_keyboardHeadingTop));
    });
    // A flicked card's app either closes, asks a question with a dialog of its
    // own, or keeps running; the card is looked at until one of those is true.
    m_thrownTimer.setInterval(100);
    QObject::connect(&m_thrownTimer, &QTimer::timeout, &m_thrownTimer,
                     [this]() { checkThrownCards(); });
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
    // One workspace exists and it is bound to one output. CARD-LIFECYCLE.md §11
    // gives any other output ordinary windows or per-output Bento, never
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

QList<KWin::EffectWindow *> CardStageController::stackFrontToBack(const KWin::EffectWindow *window) const
{
    QList<KWin::EffectWindow *> result;
    const int index = liveCardIndex(window);
    if (index < 0) return result;
    for (const auto &member : m_workspace.stackFrontToBack(m_workspace.windows().at(index)))
        if (member) result.append(member.data());
    return result;
}

bool CardStageController::stackBehind(KWin::EffectWindow *face, const QList<KWin::EffectWindow *> &behind)
{
    if (!m_active || m_cardGrabActive || m_launcherGuestActive || !face) return false;
    QList<QPointer<KWin::EffectWindow>> members;
    for (auto *window : behind) members.append(window);
    if (!m_workspace.stackBehind(face, members)) return false;
    syncSelectedStackingOrder();
    KWin::effects->addRepaintFull();
    return true;
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
    if (m_cardGrabActive) {
        // Under a held card, the nearest entry beyond each side of the screen.
        auto *output = m_host->tabletOutputForCardStage();
        const auto frame = carryFrame(output);
        if (!frame) return result;
        QPointer<KWin::EffectWindow> nearest[2];
        double distance[2] = {0.0, 0.0};
        for (const auto &window : std::as_const(m_workspace.windows())) {
            if (!window || window->isDeleted() || visibleSlot(window) != 99) continue;
            const auto target = carryTarget(output, window);
            if (target.width() <= 0) continue;
            const double offset = target.center().x() - frame->centreX;
            const int side = offset < 0.0 ? 0 : 1;
            if (!nearest[side] || std::abs(offset) < distance[side]) {
                nearest[side] = window;
                distance[side] = std::abs(offset);
            }
        }
        for (const auto &window : nearest)
            if (window) result.append(window);
        return result;
    }
    for (int side : {-1, 1}) {
        const int id = m_workspace.idAtOffset(side * 2);
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
    if (m_presentation == CardPresentation::Bento
        || m_presentation == CardPresentation::Desktop) {
        // The panes, or the desktop, are the display. Individual cards stay
        // owned and hidden.
        return 99;
    }
    if (m_presentation == CardPresentation::Active) {
        return cardId == m_workspace.selectedId() ? 0 : 99;
    }
    if (m_workspace.sameStack(cardId, m_workspace.selectedId())) {
        return 0;
    }
    if (m_cardGrabActive) {
        // The row under a held card is drawn wherever it is on screen.
        auto *output = m_host->tabletOutputForCardStage();
        const auto target = carryTarget(output, window);
        if (!output || target.width() <= 0) return 99;
        const auto work = workArea(output);
        const double reach = target.width() * 0.2;
        if (target.right() + reach < work.left() || target.left() - reach > work.right()) return 99;
        return target.center().x() < work.center().x() ? -1 : 1;
    }
    if (m_workspace.count() == 2) {
        return rowNeighborSide();
    }
    if (m_workspace.count() >= 3) {
        // The row has two ends: nothing is drawn before the first entry or
        // after the last, though the order behind it still wraps.
        const std::array<int, 3> neighborhood =
            m_workspace.visibleNeighborhood();
        const int selectedIndex = m_workspace.selectedIndex();
        if (selectedIndex > 0 && m_workspace.sameStack(cardId, neighborhood[0])) {
            return -1;
        }
        if (selectedIndex < m_workspace.count() - 1
            && m_workspace.sameStack(cardId, neighborhood[2])) {
            return 1;
        }
    }
    return 99;
}

int CardStageController::rowNeighborSide() const
{
    return m_workspace.selectedIndex() == 0 ? 1 : -1;
}

int CardStageController::entryOffset(const KWin::EffectWindow *window) const
{
    const int index = liveCardIndex(window);
    return index < 0 ? 99
        : m_workspace.model().entryIndexForId(index + 1) - m_workspace.selectedIndex();
}

bool CardStageController::rowMotionApplies() const
{
    return m_active && m_presentation == CardPresentation::Spread
        && !m_cardGrabActive && !m_launcherGuestActive && !m_row.still();
}

RowStops CardStageController::rowStops(KWin::LogicalOutput *output) const
{
    RowStops row;
    if (!output || m_workspace.count() == 0) return row;
    const int selectedIndex = m_workspace.selectedIndex();
    // Centre to centre: a pair's centred card is larger than its neighbour.
    const double centre = QRectF(cardTargetForSlot(output, 0)).center().x();
    for (int entry = 0; entry < m_workspace.count(); ++entry) {
        row.stops.push_back(entry == selectedIndex ? 0.0
            : QRectF(cardTargetForSlot(output, entry - selectedIndex)).center().x() - centre);
    }
    return row;
}

KWin::Rect CardStageController::pairTargetInMotion(KWin::LogicalOutput *output,
                                                   const KWin::EffectWindow *window,
                                                   const KWin::Rect &resting) const
{
    // A pair centres the larger card. As the row carries the other one in,
    // the two trade sizes with the hand, so the row lands on exactly what the
    // new selection draws and nothing is left to jump.
    const RowStops row = rowStops(output);
    const int other = m_workspace.selectedIndex() == 0 ? 1 : 0;
    const double stop = row.stops.at(static_cast<std::size_t>(other));
    const double progress = stop == 0.0 ? 0.0 : m_row.position / stop;
    auto moved = resting;
    if (progress <= 0.0) {
        moved.translate(-qRound(m_row.position), 0);
        return moved;
    }
    const int otherId = m_workspace.idAtOffset(other - m_workspace.selectedIndex());
    const int entry = m_workspace.model().entryIndexForId(liveCardIndex(window) + 1);
    auto arrived = cardTargetForSlot(output, entry - other, otherId);
    if (progress >= 1.0) {
        arrived.translate(-qRound(m_row.position - stop), 0);
        return arrived;
    }
    const auto blend = [progress](int a, int b) { return qRound(a + (b - a) * progress); };
    return KWin::Rect(blend(resting.x(), arrived.x()), blend(resting.y(), arrived.y()),
                      blend(resting.width(), arrived.width()),
                      blend(resting.height(), arrived.height()));
}

int CardStageController::rowSlotInMotion(const KWin::EffectWindow *window) const
{
    const int offset = entryOffset(window);
    auto *output = m_host->tabletOutputForCardStage();
    if (offset == 99 || !output) return 99;
    if (offset == 0) return 0;
    const auto work = workArea(output);
    const auto target = cardTargetForSlot(output, offset);
    // A closed Stack's fan and a card's shadow reach a little past its slot.
    const double reach = target.width() * 0.2;
    const double left = target.x() - m_row.position;
    return left + target.width() + reach > work.left() && left - reach < work.right()
        ? offset : 99;
}

bool CardStageController::cardGrabActive() const
{
    return m_cardGrabActive;
}

QPointF CardStageController::cardGrabOffset() const
{
    // A card held in its own Stack follows the finger only a little way,
    // lifted out above the place it would take.
    if (m_cardGrabActive && m_carry.inStack) {
        const double height = m_cardGrabTarget.width() > 0
            ? m_carry.heldWidth * m_cardGrabTarget.height() / m_cardGrabTarget.width() : 0.0;
        return {carryStackLeash(m_cardGrabOffset.x()),
                carryStackLeash(m_cardGrabOffset.y()) - m_carry.lift * CarryStackLift * height};
    }
    return m_cardGrabOffset;
}

KWin::Rect CardStageController::cardGrabTarget() const
{
    // The finger keeps the point of the card it picked up, whatever size the
    // card is drawn at; the caller adds the finger's travel.
    if (!m_cardGrabActive || m_carry.heldWidth <= 0.0 || m_cardGrabTarget.width() <= 0)
        return m_cardGrabTarget;
    const double width = m_carry.heldWidth;
    const double height = width * m_cardGrabTarget.height() / m_cardGrabTarget.width();
    return KWin::Rect(qRound(m_cardGrabStart.x() - m_carry.grip.x() * width),
                      qRound(m_cardGrabStart.y() - m_carry.grip.y() * height),
                      qRound(width), qRound(height));
}

bool CardStageController::heldInStack() const
{
    return m_cardGrabActive && m_carry.inStack;
}

KWin::Rect CardStageController::heldStackSeam() const
{
    // The seam is fixed to the Stack, never to the held card.
    auto *tablet = m_host->tabletOutputForCardStage();
    return heldInStack() && tablet ? cardTargetForSlot(tablet, 0) : KWin::Rect();
}

CardStageController::StackOutline CardStageController::stackOutline() const
{
    constexpr double Strength = 0.65;
    if (heldInStack()) return {heldStackSeam(), Strength * m_carry.lift};
    if (!m_stackOutlineFade.isValid() || m_stackOutlineFade.elapsed() >= motion(StackOutlineFadeDuration))
        return {};
    const double left = 1.0 - double(m_stackOutlineFade.elapsed()) / motion(StackOutlineFadeDuration);
    return {m_stackOutlineRect, m_stackOutlineFrom * left * left};
}

int CardStageController::shownStackPosition(int cardId) const
{
    // A card held in its own Stack is numbered as it will be once let go.
    if (heldInStack() && cardId == m_workspace.selectedId()) {
        if (m_carry.paging.place == m_carry.home) return m_workspace.model().stackPositionForId(cardId);
        auto others = m_workspace.stackMembersForId(cardId);
        others.erase(std::remove(others.begin(), others.end(), cardId), others.end());
        return carryStackLanding(m_carry.stack, cardId, m_carry.paging.place, others).insertion;
    }
    return m_workspace.stackActivePositionForId(cardId);
}

QString CardStageController::carryAimName() const
{
    if (!m_cardGrabActive || !m_carry.moved) return {};
    if (m_carry.inStack) return QStringLiteral("place");
    switch (m_carry.aim.kind) {
    case CarryAim::Kind::Card:
        return m_carry.aim.part >= 0 ? QStringLiteral("pane") : QStringLiteral("join");
    case CarryAim::Kind::Gap:
        return m_carry.candidate.kind == CarryAim::Kind::Card
            ? QStringLiteral("over") : QStringLiteral("gap");
    }
    return {};
}

int CardStageController::carryAimIndex() const
{
    if (!m_cardGrabActive || !m_carry.moved) return -1;
    return m_carry.inStack ? m_carry.paging.place : m_carry.aim.index;
}

QString CardStageController::carryAimPane() const
{
    if (!m_cardGrabActive || !m_carry.moved || m_carry.aim.kind != CarryAim::Kind::Card) return {};
    const auto *pane = groupPane(m_carry.aim.part);
    return pane ? pane->internalId().toString(QUuid::WithoutBraces) : QString();
}

double CardStageController::carryPaneRecess(const KWin::EffectWindow *window) const
{
    if (!m_cardGrabActive || m_carry.risePart < 0 || m_carry.rise <= 0.0
        || !window || groupPane(m_carry.risePart) != window
        || carryItemOf(window) != m_carry.riseIndex) return 0.0;
    // The pane a held card would take gives way to a cutout as the card waits
    // over it, and all the way once it would take it.
    return m_carry.rise;
}

std::optional<CardStageController::HeldTuck> CardStageController::heldTuck() const
{
    if (!m_cardGrabActive || m_carry.risePart < 0 || m_carry.rise <= 0.0) return std::nullopt;
    auto *output = m_host->tabletOutputForCardStage();
    KWin::EffectWindow *pane = groupPane(m_carry.risePart);
    if (!output || !pane || carryItemOf(pane) != m_carry.riseIndex) return std::nullopt;
    const KWin::Rect group = carryTarget(output, pane);
    const KWin::Rect workspace = bentoProjectionWorkspace();
    const auto stored = bentoProjectionRect(pane);
    if (group.isEmpty() || workspace.isEmpty() || !stored) return std::nullopt;
    // The cutout is where the group draws the pane, worked out as it does.
    const auto composite = makeBentoCompositeGeometry(
        {double(group.x()), double(group.y()), double(group.width()), double(group.height())},
        {double(workspace.x()), double(workspace.y()), double(workspace.width()), double(workspace.height())});
    const auto frame = pane->frameGeometry();
    const auto expanded = pane->expandedGeometry();
    const auto projected = makeBentoProjectedPaneGeometry(composite,
        {double(workspace.x()), double(workspace.y()), double(workspace.width()), double(workspace.height())},
        *stored, {frame.x(), frame.y(), frame.width(), frame.height()},
        {expanded.x(), expanded.y(), expanded.width(), expanded.height()});
    if (!projected) return std::nullopt;
    const CardRect clip = projected->targetClip;
    const auto pose = heldTuckPose(clip, {double(group.x()), double(group.y()),
                                          double(group.width()), double(group.height())});
    const auto round = [](const CardRect &r) {
        return KWin::Rect(qRound(r.x), qRound(r.y), qRound(r.width), qRound(r.height));
    };
    return HeldTuck{m_carry.rise, round(pose.rect), pose.rotation, round(clip), group};
}

double CardStageController::carryScale() const
{
    return m_cardGrabActive ? m_carry.scale : 1.0;
}

std::optional<CardStageController::CarryFrame> CardStageController::carryFrame(
    KWin::LogicalOutput *output) const
{
    if (!output) return std::nullopt;
    const KWin::RectF work = workArea(output);
    if (work.width() <= 0 || work.height() <= 0) return std::nullopt;
    const SpreadLayout layout = makeSpreadLayout(work.x(), work.y(), work.width(), work.height());
    const CardRect &centre = layout.cards[1];
    return CarryFrame{centre.x + centre.width / 2.0, centre.y + centre.height / 2.0,
                      centre.width + layout.gutter, centre.width, centre.height,
                      work.x(), work.width()};
}

int CardStageController::carryItemOf(const KWin::EffectWindow *window) const
{
    const int index = liveCardIndex(window);
    if (!m_cardGrabActive || index < 0) return -1;
    const int entry = m_workspace.model().entryIndexForId(index + 1);
    for (std::size_t k = 0; k < m_carry.items.size(); ++k) {
        const auto &item = m_carry.items[k];
        if (item.entry == entry && (item.cardId == 0 || item.cardId == index + 1)) return int(k);
    }
    return -1;
}

double CardStageController::carryItemPosition(int k, double pitch) const
{
    if (k < 0 || std::size_t(k) >= m_carry.shifts.size()) return 0.0;
    return carryEntryPosition(k, m_carry.shifts[std::size_t(k)], pitch, m_carry.gapWidth);
}

double CardStageController::carryRowAt(double x, const CarryFrame &frame) const
{
    return m_carry.position + (x - frame.centreX) / std::max(m_carry.scale, 0.05);
}

KWin::Rect CardStageController::carryTarget(KWin::LogicalOutput *output,
                                            const KWin::EffectWindow *window) const
{
    const int item = carryItemOf(window);
    const auto frame = carryFrame(output);
    if (item < 0 || !frame) return {};
    const double centreX = frame->centreX
        + (carryItemPosition(item, frame->pitch) - m_carry.position) * m_carry.scale;
    double scale = m_carry.scale;
    double lift = 0.0;
    // Over a Bento group, its pane gives way rather than the group rising
    // (carryPaneRecess).
    if (item == m_carry.riseIndex && m_carry.risePart < 0) {
        scale *= 1.0 + (CarryJoinRise - 1.0) * m_carry.rise;
        lift = 0.04 * frame->height * m_carry.scale * m_carry.rise;
    }
    const double width = frame->width * scale;
    const double height = frame->height * scale;
    return KWin::Rect(qRound(centreX - width / 2.0), qRound(frame->centreY - height / 2.0 - lift),
                      qRound(width), qRound(height));
}

bool CardStageController::animationsRunning() const
{
    return m_row.moving() || m_lift.phase == Lift::Phase::Return
        || m_lift.phase == Lift::Phase::Throw
        || (m_cardGrabActive && (m_carry.animating || (m_cardGrabScaleTimer.isValid()
            && m_cardGrabScaleTimer.elapsed() < motion(HeldPickupDuration))))
        || (m_previewTransition.isValid() && m_previewTransition.elapsed() < transitionDuration())
        || (m_stackOutlineFade.isValid() && m_stackOutlineFade.elapsed() < motion(StackOutlineFadeDuration))
        || (m_openProgress && m_openReturn.isValid())
        || (m_launcherGuestTransitionTimer.isValid()
            && m_launcherGuestTransitionTimer.elapsed()
                < motion(LauncherGuestTransitionDuration));
}

int CardStageController::motion(int base)
{
    return motionDuration(base, KWin::effects ? KWin::effects->animationTimeFactor() : 1.0);
}

int CardStageController::transitionDuration() const
{
    if (m_landTransition) return motion(CarryLandDuration);
    if (m_stackStepTransition) return motion(HeldStackStepDuration);
    if (m_pickupTransition) return motion(HeldPickupDuration);
    if (m_rowPageTransition) return motion(RowPageDuration);
    if (m_stackBrowseDirection) return motion(StackBrowseDuration);
    if (m_arrivalExpanding) return motion(ArrivalExpandDuration);
    return motion(PreviewTransitionDuration);
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
            / motion(LauncherGuestTransitionDuration),
        0.0, 1.0);
    const double eased = QEasingCurve(QEasingCurve::OutCubic)
        .valueForProgress(elapsed);
    return m_launcherGuestTransitionFrom
        + (1.0 - m_launcherGuestTransitionFrom) * eased;
}

KWin::Rect CardStageController::cardTargetForSlot(
    KWin::LogicalOutput *output, int slot) const
{
    return cardTargetForSlot(output, slot, m_workspace.selectedId());
}

KWin::Rect CardStageController::cardTargetForSlot(
    KWin::LogicalOutput *output, int slot, int selectedId) const
{
    const KWin::RectF work = workArea(output);
    // A card held in its own Stack leaves the row as it stood.
    const bool rowStands = !m_cardGrabActive || m_carry.inStack;
    const bool focusedPair = m_workspace.count() == 2 && std::abs(slot) < 2
        && (!m_launcherGuestActive || m_launcherGuestArrival) && rowStands;
    const SpreadLayout layout = focusedPair
        ? makeFocusedPairLayout(work.x(), work.y(), work.width(), work.height())
        : makeSpreadLayout(work.x(), work.y(), work.width(), work.height());
    // Every Stack keeps the room its fan takes wherever it stands, so the gap
    // between two entries never depends on which is centred, and the row lands
    // on a Stack without anything beside it moving (CARD-LIFECYCLE.md §9).
    const bool reserves = m_presentation == CardPresentation::Spread && rowStands;
    const auto &spread = m_workspace.model();
    const int centre = spread.entryIndexForId(selectedId);
    const auto envelopeAt = [&](int offset) -> CardStackEnvelope {
        const int index = centre + offset;
        // Past either end of the row there is nothing; the row never wraps.
        if (!reserves || centre < 0 || index < 0 || index >= spread.count()) return {0.0, 0.0};
        const int id = spread.idAtOffset(index - spread.selectedIndex());
        const int memberCount = spread.stackSizeForId(id);
        if (memberCount <= 1) return {0.0, 0.0};
        if (m_bentoProjectionSession
            && usesBentoProjectionAperture(m_workspace.windows().value(id - 1))) {
            return {0.0, 0.0};
        }
        // A Stack parted for a held card keeps room for the cards either side
        // of its seam.
        if (m_cardGrabActive && offset == 0) {
            return makeInsertionStackEnvelope(memberCount, layout.cards[1].width,
                                              layout.cards[1].height);
        }
        return makeOpenStackEnvelope(memberCount, spread.stackActivePositionForId(id),
                                     layout.cards[1].width, layout.cards[1].height);
    };
    CardRect target;
    if (focusedPair) {
        const CardStackEnvelope own = envelopeAt(0);
        target = makeReservedFocusedPairTarget(layout, slot, own);
        const CardStackEnvelope other = slot == 0 ? CardStackEnvelope{0.0, 0.0} : envelopeAt(slot);
        target.x += slot < 0 ? -other.right : slot > 0 ? -other.left : 0.0;
    } else {
        target = makeRowCardTarget(layout, slot, envelopeAt);
    }
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
    m_slideLeavers = false;
    m_landTransition = false;
    m_landWindow.clear();
    m_stackStepTransition = false;
    m_stackBrowseDirection = 0;
    m_stackBrowseOutgoing.clear();
}

void CardStageController::anchorRowTransition()
{
    auto *output = m_host->tabletOutputForCardStage();
    if (!m_rowPageTransition || !output) return;
    const auto work = workArea(output);
    if (work.width() <= 0) return;
    const int centerId = m_workspace.selectedId();
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
    if (rowMotionApplies()) return rowSlotInMotion(window);
    const int slot = visibleSlot(window);
    if (slot == 99 && m_slideLeavers && m_previewTransition.isValid()
        && m_previewTransition.elapsed() < transitionDuration()) {
        for (const auto &origin : m_previewOrigins) {
            if (origin.window != window || !origin.visible) continue;
            const int offset = entryOffset(window);
            return offset == 0 ? 99 : offset;
        }
        return 99;
    }
    if (slot != 99 || !m_rowPageTransition || !m_previewTransition.isValid()
        || m_previewTransition.elapsed() >= motion(RowPageDuration)) return slot;
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
    const auto work = workArea(output);
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
            const QPointF offset = cardGrabOffset();
            rect.translate(qRound(offset.x()), qRound(offset.y()));
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
    m_slideLeavers = false;
    m_landTransition = false;
    m_landWindow.clear();
    m_stackStepTransition = false;
    m_stackBrowseDirection = 0;
    m_stackBrowseOutgoing.clear();
    m_previewTransition.start();
}

KWin::Rect CardStageController::previewTargetForWindow(
    KWin::LogicalOutput *output, const KWin::EffectWindow *window) const
{
    // A card held in its own Stack leaves the row standing as it stood.
    if (m_cardGrabActive && !m_carry.inStack && m_presentation == CardPresentation::Spread
        && window != selectedWindow()) {
        return carryTarget(output, window);
    }
    auto target = restingPreviewTarget(output, window);
    if (target.width() <= 0) return target;
    if (rowMotionApplies() && m_workspace.count() == 2) target = pairTargetInMotion(output, window, target);
    else if (rowMotionApplies()) target.translate(-qRound(m_row.position), 0);
    if (m_lift.phase != Lift::Phase::None && window == m_lift.window)
        target.translate(0, qRound(m_lift.y));
    return target;
}

KWin::Rect CardStageController::restingPreviewTarget(
    KWin::LogicalOutput *output, const KWin::EffectWindow *window) const
{
    const int slot = paintSlot(window);
    if (!output || slot == 99) return {};
    auto target = m_launcherGuestActive && !m_launcherGuestArrival ? launcherGuestTargetForSlot(output, slot)
                                       : cardTargetForSlot(output, slot);
    if (m_rowPageTransition && visibleSlot(window) == 99) {
        const auto work = workArea(output);
        target.translate(slot < 0 ? work.x() - target.right() - 32
                                 : work.right() - target.x() + 32, 0);
    }
    if (m_arrivalExpanding) {
        if (window == m_arrivalWindow) target = activeTarget(output);
        else target.translate(slot * output->geometry().width() / 2, 0);
    }
    if (m_openProgress && m_presentation == CardPresentation::Spread && !m_arrivalExpanding) {
        // Opening under three fingers: the Active card shrinks from where it
        // stood into the row, and its neighbours slide in from the sides.
        const double p = spreadOpenProgress();
        const QRectF to(target.x(), target.y(), target.width(), target.height());
        QRectF from = window == selectedWindow() ? QRectF(activeTarget(output)) : to;
        if (window != selectedWindow()) from.translate(slot * output->geometry().width() / 2.0, 0);
        const auto blend = [p](double a, double b) { return qRound(a + (b - a) * p); };
        return KWin::Rect(blend(from.x(), to.x()), blend(from.y(), to.y()),
                          blend(from.width(), to.width()), blend(from.height(), to.height()));
    }
    const int duration = m_arrivalExpanding ? motion(ArrivalExpandDuration) : motion(PreviewTransitionDuration);
    if (!m_previewTransition.isValid() || m_cardGrabActive || m_poseTransition
        || m_presentation != CardPresentation::Spread
        || m_previewTransition.elapsed() >= duration) return target;
    const auto work = workArea(output);
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
    const int duration = transitionDuration();
    // Under a held card only a pickup, a page of the row, or a held Stack
    // card's paging moves the others, and never the held card itself.
    const bool heldTransition = m_rowPageTransition || m_pickupTransition
        || (m_carry.inStack && m_stackBrowseDirection);
    if (!output || !m_poseTransition
        || (m_cardGrabActive && (!heldTransition || window == selectedWindow()))
        || !m_previewTransition.isValid()
        || m_presentation != CardPresentation::Spread
        || m_previewTransition.elapsed() >= duration) return 1.0;
    const auto work = workArea(output);
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
    const double progress = std::clamp(double(m_previewTransition.elapsed()) / duration, 0.0, 1.0);
    // A landing moves the whole row as one zoom, eased in and out; anything
    // else settles.
    const double t = m_landTransition
        ? carryLandProgress(m_landScale, QEasingCurve(QEasingCurve::InOutCubic).valueForProgress(progress))
        : QEasingCurve(QEasingCurve::OutCubic).valueForProgress(progress);
    for (const auto &origin : m_previewOrigins) {
        if (origin.window != window) continue;
        const auto blend = [t](double a, double b) { return qRound(a + (b - a) * t); };
        const double opacity = (origin.visible ? origin.opacity : 0.0) * (1.0 - t) + (pose.visible ? 1.0 : 0.0) * t;
        const bool destinationVisible = pose.visible;
        pose.visible = opacity > 0.0;
        if (!origin.visible) return opacity; // No visible source pose to travel from.
        if (m_landTransition && window == m_landWindow && destinationVisible && rect.width() > 0) {
            // The card let go drops into the line a little ahead of the row
            // and takes the row's size, and travels to its place on the row's
            // own curve, so it never swings toward it.
            const double drop = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(std::min(1.0, progress * 1.7));
            const QRectF from(work.x() + origin.normalized.x() * work.width(),
                              work.y() + origin.normalized.y() * work.height(),
                              origin.normalized.width() * work.width(),
                              origin.normalized.height() * work.height());
            const QRectF to(rect.x(), rect.y(), rect.width(), rect.height());
            const double rowWidth = to.width() * (m_landScale + (1.0 - m_landScale) * t);
            const double width = from.width() + (rowWidth - from.width()) * drop;
            const double height = width * to.height() / to.width();
            const double x = from.center().x() + (to.center().x() - from.center().x()) * t;
            const double y = from.center().y() + (to.center().y() - from.center().y()) * drop;
            rect = KWin::Rect(qRound(x - width / 2.0), qRound(y - height / 2.0), qRound(width), qRound(height));
            pose.rotation = origin.rotation + (pose.rotation - origin.rotation) * drop;
            pose.visible = true;
            return 1.0;
        }
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
            ? heldPickupProgress(m_cardGrabScaleTimer.elapsed(), motion(HeldPickupDuration)) : 1.0;
        return {0.0, 0.0, m_cardGrabRotation * (1.0 - t), true};
    }
    const auto &spread = m_workspace.model();
    const int cardId = m_workspace.windows().indexOf(const_cast<KWin::EffectWindow *>(window)) + 1;
    const int memberCount = spread.stackSizeForId(cardId);
    const int memberIndex = spread.stackPositionForId(cardId);
    const int activeIndex = spread.stackActivePositionForId(cardId);
    CardStackPose pose{0.0, 0.0, 0.0, true};
    if (memberCount > 1) {
        // Compact neighbors around their selected face, not storage order.
        // Keep the same visible identities as the open fan (three shoulders).
        const int closedDepth = (activeIndex - memberIndex + memberCount) % memberCount;
        CardStackPose closed = makeClosedStackPose(
            memberCount - 1 - closedDepth, memberCount,
            tablet->geometry().width());
        closed.visible = closedDepth <= 3;
        if (cardGrabActive() && m_carry.inStack && spread.sameStack(cardId, spread.selectedId())) {
            // The Stack a card is held in parts front to back at the place it
            // would take: the cards in front of it to the right of the seam,
            // those behind to the left, the way its fan shows them.
            const auto order = carryStackOrder(m_carry.stack, spread.selectedId(), m_carry.paging.place);
            const auto at = std::find(order.cbegin(), order.cend(), cardId);
            const int last = int(order.size()) - 1;
            return at == order.cend() ? closed
                : makeInsertionStackPose(last - int(std::distance(order.cbegin(), at)), int(order.size()),
                                         last - m_carry.paging.place, width);
        }
        if (cardGrabActive() && !m_carry.inStack) {
            // Under a carried card every Stack is closed, at the row's size.
            closed.x *= m_carry.scale;
            closed.y *= m_carry.scale;
            return closed;
        }
        // Every Stack in the row shows its fan and keeps its room, so the row
        // lands on one without anything moving. Beside the search launcher the
        // row's Stacks close up.
        if (!m_launcherGuestActive || m_launcherGuestArrival) {
            pose = makeOpenStackPose(
                memberIndex, memberCount,
                activeIndex, width);
        } else {
            pose = closed;
            const int side = visibleSlot(window);
            if (side == -1 || side == 1) {
                const auto work = workArea(tablet);
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
    const KWin::RectF work = workArea(output);
    const SpreadLayout layout = makeLauncherGuestLayout(work.x(), work.y(),
        work.width(), work.height(), m_launcherGuestGroupCount);
    const auto target = layout.cards[slot < 0 ? 0 : slot > 0 ? 2 : 1];
    return KWin::Rect(qRound(target.x), qRound(target.y),
                      qRound(target.width), qRound(target.height));
}

KWin::Rect CardStageController::activeTarget(KWin::LogicalOutput *output) const
{
    const KWin::RectF work = workArea(output);
    // The gutter is the same on every edge, a reserving panel included: the
    // work area already stops at the panel, and at a panel that has stepped
    // aside for the keys (workAreaForCardStage). The room the card gives up
    // for the keys is made from this target, in updateKeyboardRoom.
    const CardRect target = makeActiveTarget(
        work.x(), work.y(), work.width(), work.height(), m_settings.gutter());
    return KWin::Rect(qRound(target.x), qRound(target.y),
                      qRound(target.width), qRound(target.height));
}

KWin::Rect CardStageController::activePlacement(
    KWin::LogicalOutput *output) const
{
    if (!m_keyboardRoom || !m_keyboardRoom->window
        || m_keyboardRoom->window != m_activeRestore.window) {
        return activeTarget(output);
    }
    KWin::RectF placement = m_keyboardRoom->base;
    if (m_host->inputPanelTopForCardStage(output).has_value()
        && m_host->keyboardTypesIntoForCardStage(m_keyboardRoom->window))
        placement.setHeight(m_keyboardRoom->height);
    return placement.toRect();
}

void CardStageController::refreshKeyboardRoom()
{
    m_keyboardRoomTimer.start();
}

void CardStageController::putBackKeyboardRoom()
{
    m_keyboardRoomTimer.stop();
    m_keyboardRoomRelease.stop();
    m_keyboardRestTimer.stop();
    m_keyboardHeadingTimer.stop();
    m_keyboardHeadingPending = false;
    m_keyboardHeadingTop.reset();
    const auto room = std::exchange(m_keyboardRoom, std::nullopt);
    if (!room || room->height >= room->base.height() || !room->window
        || room->window->isDeleted() || !room->window->window()) {
        return;
    }
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    room->window->window()->moveResize(room->base);
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce keyboard room returned" << room->window->caption()
            << "to" << room->base.toRect();
}

KWin::Window *CardStageController::keyboardRoomClient() const
{
    const auto window = m_activeRestore.window;
    if (!m_active || m_presentation != CardPresentation::Active
        || !m_activeRestore.valid || !window || window->isDeleted()
        || !window->window() || !m_keyboardRoom || m_keyboardRoom->window != window
        || !m_host->keyboardTypesIntoForCardStage(window)) {
        return nullptr;
    }
    auto *client = window->window();
    if (client->isInteractiveMove() || client->isInteractiveResize()
        || client->isFullScreen() || client->maximizeMode() != KWin::MaximizeRestore
        || client->quickTileMode() != KWin::QuickTileMode{}) {
        return nullptr;
    }
    return client;
}

double CardStageController::keyboardRoomFor(const KWin::Window *client,
                                            double keyboardTop) const
{
    // Its top edge, its width and its place stay; only its bottom edge
    // follows the keys. A client that cannot be that short keeps the least
    // height it can have.
    const KWin::RectF base = m_keyboardRoom->base;
    const double room = keyboardRoomHeight(base.y(), base.height(),
        keyboardTop, m_settings.gutter());
    return std::min(base.height(),
        client->constrainFrameSize(QSizeF(base.width(), room)).height());
}

void CardStageController::askKeyboardRoom(KWin::Window *client, double height)
{
    m_keyboardRoom->height = height;
    const KWin::RectF base = m_keyboardRoom->base;
    const KWin::Rect target = KWin::RectF(base.x(), base.y(), base.width(), height).toRect();
    // Measured against what was asked for, not the frame: a client still
    // drawing its last size, or one that trims itself to whole rows as a
    // terminal does, is not asked again.
    if (client->moveResizeGeometry().toRect() == target) return;
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    client->moveResize(KWin::RectF(target));
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce keyboard room" << m_keyboardRoom->window->caption()
            << "height" << height << "target" << target;
}

void CardStageController::keyboardHeading(double top, int durationMs)
{
    m_keyboardHeadingTop = top;
    m_keyboardHeadingSince.start();
    m_keyboardHeadingDuration = durationMs;
    m_keyboardHeadingTimer.stop();
    m_keyboardHeadingPending = false;
    auto *client = keyboardRoomClient();
    if (!client) return;
    // Room the keys give back is asked for before they move, so the client
    // is that tall by the time they uncover it. Room they take is asked for
    // as they arrive, timed from their first frame rather than from this
    // word, which reaches here before they start: a client that answers
    // before the keys are there stands clear of them.
    const double heading = keyboardRoomFor(client, top);
    if (heading > m_keyboardRoom->height) askKeyboardRoom(client, heading);
    else if (heading < m_keyboardRoom->height) m_keyboardHeadingPending = true;
}

std::optional<double> CardStageController::keyboardRoomEdge(
    const KWin::EffectWindow *window) const
{
    if (!window || !m_keyboardRoom || m_keyboardRoom->window != window
        || m_presentation != CardPresentation::Active
        || !m_host->keyboardTypesIntoForCardStage(window)) {
        return std::nullopt;
    }
    const auto top = m_host->inputPanelTopForCardStage(m_host->tabletOutputForCardStage());
    if (!top) return std::nullopt;
    return *top - m_settings.gutter();
}

void CardStageController::keepKeyboardRoomPlacement()
{
    auto *client = keyboardRoomClient();
    auto *tablet = m_host->tabletOutputForCardStage();
    if (!client || !tablet) return;
    const KWin::Rect target = activePlacement(tablet);
    if (client->moveResizeGeometry().toRect() == target) return;
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    client->moveResize(KWin::RectF(target));
}

void CardStageController::followWorkArea()
{
    if (!m_active) return;
    // Spread and the neighbours are drawn from the area every frame.
    KWin::effects->addRepaintFull();
    const auto window = m_activeRestore.window;
    auto *tablet = m_host->tabletOutputForCardStage();
    if (m_presentation != CardPresentation::Active || !m_activeRestore.valid
        || !window || window->isDeleted() || !window->window() || !tablet) return;
    auto *client = window->window();
    if (client->isInteractiveMove() || client->isInteractiveResize()
        || client->isFullScreen() || client->maximizeMode() != KWin::MaximizeRestore
        || client->quickTileMode() != KWin::QuickTileMode{}) return;
    const KWin::Rect target = activePlacement(tablet);
    if (client->moveResizeGeometry().toRect() == target) return;
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    client->moveResize(KWin::RectF(target));
    qInfo() << "Kadunce Active card follows the work area" << window->caption()
            << "to" << target;
}

void CardStageController::repaintKeyboardEdge(const KWin::RectF &from,
                                              const KWin::RectF &to) const
{
    if (!m_keyboardRoom || m_presentation != CardPresentation::Active) return;
    const double gutter = m_settings.gutter();
    const double top = std::min(from.y(), to.y()) - gutter - 1;
    const double bottom = std::max(from.y(), to.y()) + 1;
    const double left = std::min(from.x(), to.x());
    const double right = std::max(from.x() + from.width(), to.x() + to.width());
    KWin::effects->addRepaint(KWin::RectF(left, top, right - left, bottom - top).toAlignedRect());
}

void CardStageController::updateKeyboardRoom(bool resting)
{
    const auto window = m_activeRestore.window;
    if (m_keyboardRoom && m_keyboardRoom->window != window) {
        // Whatever took the card out of Active placed it; the room left with it.
        m_keyboardRoom.reset();
        m_keyboardRoomRelease.stop();
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
    // Room is made for keys typing into this card, never for keys another
    // window asked for while the card waits behind it.
    auto keyboardTop = m_host->inputPanelTopForCardStage(tablet);
    if (!m_host->keyboardTypesIntoForCardStage(window)) keyboardTop.reset();
    if (!keyboardTop) {
        m_keyboardRestTimer.stop();
        m_keyboardHeadingTimer.stop();
        m_keyboardHeadingPending = false;
        m_keyboardHeadingTop.reset();
        if (!m_keyboardRoom) return;
        const KWin::RectF base = m_keyboardRoom->base;
        // KWin sends a size a moment after it is asked for, and sends nothing
        // for the size a client already has, and it puts the window back as
        // the keys close. Any of these can land the client somewhere else
        // after the room is given back, so the settle asks again when it does.
        m_activeSettleRemaining = std::max(m_activeSettleRemaining, 2);
        if (m_keyboardRoom->height < base.height()) {
            m_keyboardRoom->height = base.height();
            QScopedValueRollback<bool> applying(m_applyingWindowState, true);
            client->moveResize(base);
            KWin::effects->addRepaintFull();
            qInfo() << "Kadunce keyboard room returned" << window->caption()
                    << "to" << base.toRect();
        }
        m_keyboardRoomRelease.start();
        return;
    }
    m_keyboardRoomRelease.stop();
    if (!m_keyboardRoom) {
        // The placement asked for rather than the frame, which still shows the
        // old size while a card that has just arrived acknowledges its new one.
        const KWin::RectF base = client->moveResizeGeometry();
        m_keyboardRoom = KeyboardRoom{window, base, base.height()};
    }
    // The card is drawn ending a gutter above the keys on every frame they
    // move (keyboardRoomEdge), so its client is asked for a size once a
    // motion rather than once a frame: what the keys give back at once, what
    // they take as they arrive or once they rest.
    const double now = keyboardRoomFor(client, *keyboardTop);
    const std::optional<double> heading = m_keyboardHeadingTop
        ? std::optional<double>(keyboardRoomFor(client, *m_keyboardHeadingTop)) : std::nullopt;
    if (!resting && m_keyboardHeadingPending && m_keyboardHeadingTop) {
        m_keyboardHeadingPending = false;
        m_keyboardHeadingTimer.start(m_keyboardHeadingDuration);
    }
    if (resting && m_keyboardHeadingTop && m_keyboardHeadingSince.isValid()
        && m_keyboardHeadingSince.elapsed() < m_keyboardHeadingDuration) {
        // Keys that said where they are going and are not due yet have only
        // paused on the way.
        m_keyboardRestTimer.start();
        return;
    }
    if (resting) {
        m_keyboardHeadingTop.reset();
        m_keyboardHeadingPending = false;
    } else {
        m_keyboardRestTimer.start();
    }
    if (const auto ask = keyboardRoomAsk(m_keyboardRoom->height, now, heading,
            m_keyboardRoom->base.height(), resting)) {
        askKeyboardRoom(client, *ask);
    }
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

void CardStageController::holdCardsAside()
{
    for (const auto &window : std::as_const(m_workspace.windows())) {
        if (!window || window->isDeleted() || !window->window()
            || window->window()->isHidden() || m_aside.contains(window)) continue;
        window->setData(CardAsideRole, true);
        window->window()->setHidden(true);
        m_aside.append(window);
    }
}

void CardStageController::bringCardsBack()
{
    for (const auto &window : std::exchange(m_aside, {})) {
        if (!window || window->isDeleted()) continue;
        window->setData(CardAsideRole, QVariant());
        if (window->window()) window->window()->setHidden(false);
    }
}

void CardStageController::returnWindowToDesktop(KWin::EffectWindow *window)
{
    if (!m_active || !window || window->isDeleted() || liveCardIndex(window) >= 0) return;
    finishCardGrab(false);
    if (m_presentation == CardPresentation::Active) parkActiveSnapshot();
    m_returnedToDesktop.removeIf([window](const auto &w) { return !w || w->isDeleted() || w == window; });
    m_returnedToDesktop.append(window);
    showDesktop();
}

void CardStageController::showDesktop()
{
    if (!m_active) return;
    m_presentation = CardPresentation::Desktop;
    m_returnToDesktop = false;
    syncSelectedElevation();
    m_host->setPagingShortcutsForCardStage(false);
    // The window last returned to the desktop takes the keys first, so KWin
    // has no hidden card to hand them on from.
    m_returnedToDesktop.removeIf([](const auto &w) { return !w || w->isDeleted(); });
    if (!m_returnedToDesktop.isEmpty() && m_returnedToDesktop.last()->window())
        KWin::workspace()->activateWindow(m_returnedToDesktop.last()->window(), true);
    holdCardsAside();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "showed the desktop, with" << m_aside.size() << "cards aside";
}

bool CardStageController::releaseNativeCarryToDesktop(const PreparedCarrySource &source,
    KWin::LogicalOutput *output, const KWin::RectF &geometry)
{
    if (!m_active || m_launcherGuestActive || !nativeCarrySourceValid(source) || !output
        || !m_host->isTabletOutputForCardStage(output)
        || !KWin::effects->screens().contains(output) || !geometry.isValid()
        || source.m_window->isUserMove() || source.m_window->isUserResize()) return false;
    const QPointer<KWin::EffectWindow> leaving = source.m_window;
    const QPointer<KWin::Window> client = leaving->window();
    if (!client || liveCardIndex(leaving) < 0) return false;
    const auto removal = m_workspace.prepareRemoval(leaving);
    if (!removal || !m_workspace.commitRemoval(*removal)) return false;
    // The card leaves Spread before any native work, which can reenter this
    // stage, and its old geometry is never replayed: it lands where it was let go.
    ++m_restoreGeneration;
    forgetManagedRestore(leaving);
    if (m_activeRestore.window == leaving) m_activeRestore = ActiveRestoreSnapshot{};
    if (m_presentedActive == leaving) m_presentedActive = nullptr;
    m_originalCardStackingOrder.removeAll(leaving);
    m_activeSettleTimer.stop();
    m_activeSettleRemaining = 0;
    KWin::effects->setElevatedWindow(leaving, false);
    m_host->unredirectForCardStage(leaving);
    // §12: with no card left the display's session ends, and the next window
    // to open there starts cards again. Otherwise the desktop shows, so the
    // window is seen where it was let go.
    m_presentation = CardPresentation::Spread;
    m_active = !m_workspace.windows().isEmpty();
    const QPointer<KWin::LogicalOutput> target = output;
    const auto valid = [&] {
        return target && KWin::effects->screens().contains(target.data())
            && leaving && !leaving->isDeleted() && leaving->window() == client
            && !leaving->isUserMove() && !leaving->isUserResize();
    };
    // Where the person let it go, with the keys; the cards go aside after,
    // so KWin never has a hidden window to hand the focus on from.
    if (applyNativePlacement(client.data(), target.data(), geometry, valid)) {
        KWin::workspace()->raiseWindow(client);
        if (valid()) KWin::workspace()->activateWindow(client, true);
    }
    if (m_active && leaving) returnWindowToDesktop(leaving);
    m_host->setPagingShortcutsForCardStage(m_active && m_presentation != CardPresentation::Desktop);
    KWin::effects->addRepaintFull();
    return true;
}

void CardStageController::beginCardGrab(const QPointF &position)
{
    if (!m_active || m_presentation != CardPresentation::Spread
        || m_cardGrabActive || m_launcherGuestActive
        || !m_row.still() || m_lift.phase != Lift::Phase::None
        || !qIsFinite(position.x()) || !qIsFinite(position.y())) {
        return;
    }
    auto *tablet = m_host->tabletOutputForCardStage();
    const auto frame = carryFrame(tablet);
    if (!tablet || !frame) return;
    // The card under the finger, wherever it stands. A Bento group stays
    // together in Spread; its panes come out by a pull down.
    KWin::EffectWindow *window = cardAt(tablet, position);
    const int cardId = liveCardIndex(window) + 1;
    if (!window || cardId <= 0 || usesBentoProjectionAperture(window)) return;
    // Freeze the presentation at pickup: the finger keeps the point of the
    // card it took, and the row stands where it stood.
    auto pickup = previewTargetForWindow(tablet, window);
    auto pickupPose = stackPoseForWindow(window, pickup.width());
    pickup.translate(qRound(pickupPose.x), qRound(pickupPose.y));
    (void)applyPoseTransition(window, pickup, pickupPose);
    if (pickup.isEmpty()) return;
    const QPointer<KWin::EffectWindow> selectedBefore = selectedWindow();
    const int rowIndexBefore = m_workspace.selectedIndex();
    const QPointer<KWin::EffectWindow> faceBefore = m_workspace.windows().value(
        m_workspace.idAtOffset(m_workspace.model().entryIndexForId(cardId) - rowIndexBefore) - 1);
    // A card in a Stack is reordered within it; only a pull down takes it out.
    const std::vector<int> membersBefore = m_workspace.stackMembersForId(cardId);
    const bool inStack = membersBefore.size() > 1;
    captureCardTransition(false, true);
    if (!m_workspace.selectCard(cardId)) {
        m_carry.faceBefore = faceBefore;
        m_carry.selectedBefore = selectedBefore;
        restoreCarryOrigin();
        m_carry = {};
        clearCardTransition();
        return;
    }
    m_cardGrabActive = true;
    m_pickupTransition = m_poseTransition;
    m_stackStepTransition = m_poseTransition && inStack;
    m_stackOutlineFade.invalidate();
    m_cardGrabOffset = {};
    m_cardGrabStart = position;
    m_cardGrabTarget = pickup;
    m_cardGrabRotation = pickupPose.rotation;
    m_cardGrabScaleTimer.start();
    m_cardGrabPointer = position;
    m_cardGrabDestinationOutput.clear();
    m_carry = {};
    m_carry.heldIndex = m_workspace.selectedIndex();
    m_carry.inStack = inStack;
    if (inStack) {
        // Held in its own Stack, the card stays over it and the row stands:
        // sideways travel takes it through the Stack's places, counted from
        // the card that was in front before the hold.
        const auto face = std::find(membersBefore.cbegin(), membersBefore.cend(),
                                    liveCardIndex(faceBefore) + 1);
        m_carry.stack = carryStackDeck(membersBefore,
            face == membersBefore.cend() ? 0 : int(std::distance(membersBefore.cbegin(), face)));
        const auto place = std::find(m_carry.stack.cbegin(), m_carry.stack.cend(), cardId);
        m_carry.home = int(std::distance(m_carry.stack.cbegin(), place));
        m_carry.paging = {m_carry.home, position.x()};
    } else {
        // The row without the held card, every entry standing as one.
        for (int entry = 0; entry < m_workspace.count(); ++entry)
            if (entry != m_carry.heldIndex) m_carry.items.push_back({entry, 0});
        m_carry.low = 0;
        m_carry.high = int(m_carry.items.size());
        m_carry.home = m_carry.heldIndex;
    }
    // Every item keeps its place, parted where the card was.
    m_carry.gap = m_carry.home;
    m_carry.shifts.assign(m_carry.items.size(), 0.0);
    for (int k = m_carry.home; k < int(m_carry.items.size()); ++k) m_carry.shifts[std::size_t(k)] = 1.0;
    m_carry.aim = {CarryAim::Kind::Gap, m_carry.home};
    m_carry.candidate = m_carry.aim;
    m_carry.candidateSince.start();
    // The row stands as it stood.
    m_carry.position = rowIndexBefore * frame->pitch;
    m_carry.heldWidth = pickup.width();
    m_carry.grip = QPointF((position.x() - pickup.x()) / pickup.width(),
                           (position.y() - pickup.y()) / pickup.height());
    m_carry.lean = carryLeanStart(position.x(), frame->screenLeft, frame->screenWidth);
    m_carry.selectedBefore = selectedBefore;
    m_carry.faceBefore = faceBefore;
    m_carry.animating = true;
    if (!m_motionClock.isValid()) m_motionClock.start();
    KWin::effects->setElevatedWindow(window, true);
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "lifted Spread card" << cardId
            << (inStack ? "to reorder its Stack" : "from entry") << m_carry.heldIndex + 1;
}

void CardStageController::updateCardGrab(const QPointF &position)
{
    if (!m_cardGrabActive || !qIsFinite(position.x()) || !qIsFinite(position.y())) {
        return;
    }
    m_cardGrabOffset = position - m_cardGrabStart;
    updateCardGrabDestination(position);
    if (!m_carry.moved
        && std::hypot(m_cardGrabOffset.x(), m_cardGrabOffset.y()) >= CarryMoveDistance) {
        m_carry.moved = true;
        qInfo() << "Kadunce" << Revision << "carrying Spread card" << m_workspace.selectedId();
    }
    if (m_carry.inStack) pageHeldStackCard(position.x());
    aimCarry();
    m_carry.animating = true;
    if (!m_motionClock.isValid()) m_motionClock.start();
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

void CardStageController::pageHeldStackCard(double fingerX)
{
    const auto paging = carryPageStack(m_carry.paging, fingerX, int(m_carry.stack.size()));
    const int step = paging.place - m_carry.paging.place;
    if (step == 0) {
        m_carry.paging = paging;
        return;
    }
    // The Stack's cards move to their new places as the slow stroke moves
    // them, the card passed coming forward or going back.
    captureCardTransition(false, true);
    m_carry.paging = paging;
    m_stackBrowseDirection = step > 0 ? 1 : -1;
    m_stackStepTransition = true;
    qInfo() << "Kadunce" << Revision << "held card would take place" << paging.place + 1
            << "of" << m_carry.stack.size() << "in its Stack";
}

void CardStageController::aimCarry()
{
    auto *tablet = m_host->tabletOutputForCardStage();
    const auto frame = carryFrame(tablet);
    if (!m_cardGrabActive || !m_carry.moved || m_carry.inStack || !frame) return;
    // The held card's centre, along the row.
    const QRectF held = QRectF(cardGrabTarget()).translated(m_cardGrabOffset);
    const double centre = carryRowAt(held.center().x(), *frame);
    const int items = int(m_carry.items.size());
    std::vector<double> positions(std::size_t(items), 0.0);
    // A card joins any card. It takes a pane of a Bento group instead, and
    // only a group that can take it is reached.
    std::vector<double> reach(std::size_t(items), 0.0);
    const bool groupTakes = groupTakesHeldCard();
    int group = -1;
    for (int k = 0; k < items; ++k) {
        positions[std::size_t(k)] = carryItemPosition(k, frame->pitch);
        const int entry = m_carry.items[std::size_t(k)].entry;
        const auto face = m_workspace.windows().value(
            m_workspace.idAtOffset(entry - m_workspace.selectedIndex()) - 1);
        if (!face) continue;
        if (!usesBentoProjectionAperture(face)) {
            reach[std::size_t(k)] = CarryJoinReach;
        } else if (groupTakes) {
            reach[std::size_t(k)] = CarryGroupReach;
            group = k;
        }
    }
    if (m_carry.sliding) {
        // A row sliding under the card stands closed up and joins nothing:
        // its gap follows the card, to part where the row stops.
        const int gap = carryGapAt(centre, positions, m_carry.low, m_carry.high);
        m_carry.aim = m_carry.candidate = {CarryAim::Kind::Gap, gap};
        m_carry.gap = gap;
        m_carry.candidateSince.restart();
        return;
    }
    CarryAim aim = carryAim(centre, positions, frame->width, reach, m_carry.low, m_carry.high);
    // A card that would join another keeps it until carried a little further
    // off, so a hand resting on it does not flicker between the two.
    if (m_carry.aim.kind == CarryAim::Kind::Card
        && (aim.kind != CarryAim::Kind::Card || aim.index != m_carry.aim.index)
        && m_carry.aim.index < items
        && std::abs(centre - positions[std::size_t(m_carry.aim.index)])
            < (m_carry.aim.index == group ? CarryGroupKeep : CarryJoinReach * CarryJoinKeep)
                * frame->width) {
        aim = {CarryAim::Kind::Card, m_carry.aim.index};
    }
    // Over a Bento group the pane under the finger is what it would take,
    // and the pane it had stays until the finger is well past it; a pane too
    // small for it takes nothing, and the card is over a gap.
    if (aim.kind == CarryAim::Kind::Card && aim.index == group) {
        const auto paneOf = [group](const CarryAim &named) {
            return named.kind == CarryAim::Kind::Card && named.index == group ? named.part : -1;
        };
        const int kept = paneOf(m_carry.candidate) >= 0 ? paneOf(m_carry.candidate) : paneOf(m_carry.aim);
        aim.part = groupPartFor(m_cardGrabPointer, kept);
        if (aim.part < 0) aim = {CarryAim::Kind::Gap, carryGapAt(centre, positions, m_carry.low, m_carry.high)};
    }
    if (!(aim == m_carry.candidate)) {
        m_carry.candidate = aim;
        m_carry.candidateSince.restart();
    }
    if (aim == m_carry.aim) return;
    const bool onGroup = group >= 0 && m_carry.aim.index == group
        && std::abs(centre - positions[std::size_t(group)]) < CarryGroupKeep * frame->width;
    const CarryAim next = carryAimStep(m_carry.aim, aim, m_carry.candidateSince.elapsed(), m_carry.gap,
                                       onGroup);
    if (next == m_carry.aim) return;
    m_carry.aim = next;
    if (next.kind == CarryAim::Kind::Gap) {
        m_carry.gap = next.index;
    } else {
        m_carry.riseIndex = next.index;
        m_carry.risePart = next.part;
        if (next.part >= 0)
            qInfo() << "Kadunce" << Revision << "held card would take pane" << next.part + 1
                    << "of the Bento group at item" << next.index + 1;
        else
            qInfo() << "Kadunce" << Revision << "held card would join item" << next.index + 1;
    }
    m_carry.animating = true;
}

void CardStageController::advanceCarry(double seconds)
{
    auto *tablet = m_host->tabletOutputForCardStage();
    const auto frame = carryFrame(tablet);
    if (!m_cardGrabActive || !frame) return;
    bool moving = false;
    const auto ease = [&](double &value, double target, double seconds_, double tau, double close) {
        value += (target - value) * (1.0 - std::exp(-seconds_ / tau));
        if (std::abs(target - value) < close) value = target;
        else moving = true;
    };
    if (m_carry.inStack) {
        // Held in its own Stack, the card rises and lifts out of it and the
        // row stands; its Stack's cards move as sideways travel pages it.
        ease(m_carry.heldWidth, m_cardGrabTarget.width() * carryHeldFraction(1.0),
             seconds, 0.08, 0.3);
        ease(m_carry.lift, 1.0, seconds, 0.08, 0.002);
        m_carry.animating = moving;
        return;
    }
    const int items = int(m_carry.items.size());
    const int span = m_carry.high - m_carry.low;
    const bool onTablet = m_cardGrabDestinationOutput.isEmpty();
    const double finger = m_cardGrabStart.x() + m_cardGrabOffset.x();
    // Resting on a Bento group, the finger moves between its panes, most of
    // them off the middle: the row holds still under it, at the zoom it has,
    // until the card is carried off the group.
    const auto onPane = [](const CarryAim &aim) {
        return aim.kind == CarryAim::Kind::Card && aim.part >= 0;
    };
    const bool onGroup = m_carry.moved && !m_carry.sliding
        && (onPane(m_carry.candidate) || onPane(m_carry.aim));
    if (m_carry.moved)
        m_carry.lean = carryLeanFollow(m_carry.lean, finger, frame->screenLeft, frame->screenWidth);
    m_carry.depth = m_carry.moved && onTablet && !onGroup
        ? carryLeanDepth(finger, m_carry.lean, frame->screenLeft, frame->screenWidth) : 0.0;
    // The row stays three across. Pulled down a short way, the card takes it
    // to one set zoom, and back up to three across, gliding either way.
    if (m_carry.moved) {
        const bool zoomed = carryZoomed(m_carry.zoomed, m_cardGrabOffset.y(), onGroup);
        if (zoomed != m_carry.zoomed) {
            m_carry.zoomed = zoomed;
            m_carry.zoomFrom = m_carry.scale;
            m_carry.zoomSince.start();
            qInfo() << "Kadunce" << Revision << (zoomed ? "zoomed out under" : "zoomed back in under")
                    << "Spread card" << m_workspace.selectedId();
        }
    }
    const double level = m_carry.zoomed
        ? carryZoomLevel(span, frame->pitch, frame->width, frame->screenWidth) : 1.0;
    const bool gliding = m_carry.zoomSince.isValid() && m_carry.zoomSince.elapsed() < motion(CarryZoomDuration);
    const double scale = gliding
        ? carryZoomAt(m_carry.zoomFrom, level, double(m_carry.zoomSince.elapsed()) / motion(CarryZoomDuration))
        : level;
    if (gliding) moving = true;
    // The zoom moves around the held card, so what is under it stays under it.
    const double heldX = QRectF(cardGrabTarget()).translated(m_cardGrabOffset).center().x();
    const double under = carryRowAt(heldX, *frame);
    m_carry.scale = scale;
    if (m_carry.moved)
        m_carry.position = under - (heldX - frame->centreX) / std::max(m_carry.scale, 0.05);
    const double heldTarget = m_carry.moved
        ? frame->width * carryHeldFraction(m_carry.scale)
        : m_cardGrabTarget.width() * carryHeldFraction(1.0);
    ease(m_carry.heldWidth, heldTarget, seconds, 0.08, 0.3);
    // Carried, the card comes to sit centred under the finger, so where it
    // was picked up changes nothing about where it goes.
    if (m_carry.moved) ease(m_carry.grip.rx(), 0.5, seconds, 0.08, 0.002);
    if (m_carry.moved) {
        if (m_carry.zoomed
            && carryRowFits(span, frame->pitch, frame->width, frame->screenWidth, m_carry.scale)) {
            // All of it shows: it stands centred, and a lean has nothing to reach.
            ease(m_carry.position, (items - 1 + m_carry.gapWidth) / 2.0 * frame->pitch,
                 seconds, 0.09, 0.3);
            m_carry.sliding = false;
        } else {
            const double speed = onTablet && !onGroup
                ? carrySlideSpeed(finger, m_carry.lean, frame->screenLeft, frame->screenWidth,
                                  m_carry.scale) : 0.0;
            const double before = m_carry.position;
            m_carry.position = carryClampPosition(m_carry.position + speed * seconds,
                                                  m_carry.low, m_carry.high, frame->pitch);
            // Sliding, the row stands closed up; creeping or stopped, even
            // against an end, it parts under the card.
            const double shown = std::abs(m_carry.position - before) * m_carry.scale
                / std::max(seconds, 0.001);
            m_carry.sliding = shown > CarrySlideShown;
            if (m_carry.position != before) moving = true;
        }
        // Come to rest over a card, the row settles it under the held one,
        // where the held card stood then; it does not follow the finger after.
        // A Bento group stays where it is, so the pane under the finger does.
        if (m_carry.depth == 0.0 && !m_carry.sliding && !onGroup
            && m_carry.candidate.kind == CarryAim::Kind::Card) {
            if (m_carry.settle != m_carry.candidate.index) {
                m_carry.settle = m_carry.candidate.index;
                m_carry.settleX = heldX;
            }
            ease(m_carry.position, carryItemPosition(m_carry.settle, frame->pitch)
                    - (m_carry.settleX - frame->centreX) / std::max(m_carry.scale, 0.05),
                 seconds, CarrySettleTime, 0.3);
        } else {
            m_carry.settle = -1;
        }
    }
    // The row moved under a still finger, so what it is over may have too.
    aimCarry();
    // The held card's gap: whole where it was picked up, closed while the row
    // slides, parted where it stops. Over a card it holds as it is, so parting
    // never pushes that card out from under the held one.
    const bool overCard = m_carry.moved && !m_carry.sliding
        && (m_carry.candidate.kind == CarryAim::Kind::Card || m_carry.aim.kind == CarryAim::Kind::Card);
    if (!overCard)
        ease(m_carry.gapWidth, !m_carry.moved ? 1.0 : m_carry.sliding ? 0.0 : CarryGapWidth,
             seconds, 0.09, 0.002);
    for (int k = 0; k < items; ++k) {
        const double target = k >= m_carry.gap ? 1.0 : 0.0;
        ease(m_carry.shifts[std::size_t(k)], target, seconds, 0.055, 0.002);
    }
    // The card under a resting one rises as it waits, and the rest of the
    // way once it would take it.
    double riseTarget = 0.0;
    if (m_carry.aim.kind == CarryAim::Kind::Card) {
        riseTarget = 1.0;
    } else if (m_carry.moved && m_carry.candidate.kind == CarryAim::Kind::Card) {
        m_carry.riseIndex = m_carry.candidate.index;
        m_carry.risePart = m_carry.candidate.part;
        riseTarget = 0.6 * std::min(1.0, double(m_carry.candidateSince.elapsed()) / CarryJoinDwell);
    }
    ease(m_carry.rise, riseTarget, seconds, 0.05, 0.002);
    if (riseTarget == 0.0 && m_carry.rise == 0.0) {
        m_carry.riseIndex = -1;
        m_carry.risePart = -1;
    }
    // A card resting on another, or over a new gap, is still waiting to count.
    if (!(m_carry.candidate == m_carry.aim) && m_carry.moved) moving = true;
    m_carry.animating = moving;
}

void CardStageController::restoreCarryOrigin()
{
    if (m_workspace.hasDetachedMember()) m_workspace.restoreDetachedMember();
    // The Stack shows the face it had, and the row the entry it had.
    for (const auto &window : {m_carry.faceBefore, m_carry.selectedBefore}) {
        const int cardId = liveCardIndex(window) + 1;
        if (cardId > 0) (void)m_workspace.selectCard(cardId);
    }
}

void CardStageController::finishCardGrab(bool commit)
{
    if (!m_cardGrabActive) {
        return;
    }
    KWin::EffectWindow *grabbed = selectedWindow();
    if (!commit) clearCardTransition();
    // Release animation is presentation only. Capture before changing order;
    // the held surface remains directly attached to its contact until release.
    // Cancellation/disable must never acquire an animation lifetime.
    if (commit && !m_launcherGuestActive) {
        captureCardTransition(false, true);
        if (m_carry.inStack) {
            // Its Stack closes at the pace it parted.
            m_stackStepTransition = m_poseTransition;
        } else if (m_poseTransition) {
            // The card lands in one move: it drops into the line and the row
            // grows back around it, cards leaving the screen as it does.
            m_landTransition = true;
            m_landScale = m_carry.scale;
            m_landWindow = grabbed;
            m_slideLeavers = true;
        }
    }
    if (m_carry.inStack) {
        // The outline of its place fades rather than vanishing.
        const auto outline = stackOutline();
        m_stackOutlineRect = outline.rect;
        m_stackOutlineFrom = outline.opacity;
        m_stackOutlineFade.start();
    }
    const CarryAim aim = m_carry.aim;
    const bool still = commit && !m_carry.moved;
    const bool home = m_carry.inStack ? m_carry.paging.place == m_carry.home
                                      : aim.kind == CarryAim::Kind::Gap && aim.index == m_carry.home;
    bool stacked = false;
    const char *outcome = "stayed";
    if (!commit || still || home) {
        // Cancelled, let go still, or where it was: back as it was.
        restoreCarryOrigin();
        outcome = !commit ? "cancelled" : still ? "held still" : "stayed";
    } else if (m_carry.inStack) {
        // Into its own Stack at its new place, and whichever card is then in
        // front is the one the Stack shows.
        const int held = m_workspace.selectedId();
        auto others = m_workspace.stackMembersForId(held);
        others.erase(std::remove(others.begin(), others.end(), held), others.end());
        const auto landing = carryStackLanding(m_carry.stack, held, m_carry.paging.place, others);
        if (!others.empty() && m_workspace.detachSelectedMember())
            stacked = m_workspace.stackSelectedWith(others.front(), landing.insertion);
        if (stacked) {
            m_workspace.commitDetachedMember();
            stacked = m_workspace.selectCard(landing.face);
        } else {
            restoreCarryOrigin();
        }
        outcome = stacked ? "reordered" : "stayed";
    } else if (aim.kind == CarryAim::Kind::Card && aim.part >= 0) {
        // Let go on a pane, the card takes it, and the group opens as its
        // layout with the card growing from where it was tucked
        // (CARD-LIFECYCLE.md §5).
        const auto card = [](const QRectF &r) { return CardRect{r.x(), r.y(), r.width(), r.height()}; };
        const QPointF offset = cardGrabOffset();
        const QRectF free = QRectF(cardGrabTarget()).translated(offset);
        const auto tuck = heldTuck();
        const CardRect drawn = tuck
            ? heldTuckBlend(card(free), card(QRectF(tuck->tucked)), tuck->progress) : card(free);
        stacked = replaceGroupPane(aim.part);
        if (stacked) m_openGroupFrom = QRectF(drawn.x, drawn.y, drawn.width, drawn.height);
        if (!stacked) restoreCarryOrigin();
        outcome = stacked ? "took a pane of the Bento group" : "stayed";
    } else if (aim.kind == CarryAim::Kind::Card) {
        const int entry = m_carry.items[std::size_t(aim.index)].entry;
        const int destinationId = m_workspace.idAtOffset(entry - m_workspace.selectedIndex());
        // It joins at the front, and is the face the Stack shows.
        stacked = m_workspace.stackSelectedWith(destinationId,
            m_workspace.model().stackActivePositionForId(destinationId));
        outcome = stacked ? "joined" : "stayed";
    } else {
        const int movement = std::clamp(aim.index, 0, m_workspace.count() - 1)
            - m_workspace.selectedIndex();
        if (movement != 0) m_workspace.moveSelected(movement);
        outcome = movement != 0 ? "moved" : "stayed";
    }
    resetCardGrabState(grabbed, stacked);
    qInfo() << "Kadunce" << Revision << "released Spread card"
            << m_workspace.selectedId() << outcome << "at entry" << m_workspace.selectedIndex() + 1;
    if (const auto from = std::exchange(m_openGroupFrom, std::nullopt))
        m_host->openGroupAfterDropForCardStage(*from);
}

void CardStageController::resetCardGrabState(
    KWin::EffectWindow *grabbed, bool stacked)
{
    // A release changes the row. Do not replay a row page captured against
    // its old membership over the committed stack/drop.
    if (m_rowPageTransition) clearCardTransition();
    if (grabbed && !grabbed->isDeleted() && !stacked) {
        KWin::effects->setElevatedWindow(grabbed, false);
    }
    m_cardGrabActive = false;
    m_cardGrabOffset = {};
    m_cardGrabStart = {};
    m_cardGrabTarget = {};
    m_cardGrabRotation = 0.0;
    m_cardGrabScaleTimer.invalidate();
    m_cardGrabPointer = {};
    m_cardGrabDestinationOutput.clear();
    m_carry = {};
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

void CardStageController::syncSelectedElevation()
{
    // Whatever left the desktop, by Spread, a choice or an arrival, brings
    // the cards held aside back first.
    if (m_presentation != CardPresentation::Desktop) bringCardsBack();
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
    if (selectedIsBentoProjection() || m_cardGrabActive) return;
    const int faceId = m_workspace.selectedId();
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
    stopOpeningSpread();
    m_host->cancelInputForCardStage();
    dropRowMotion();
    if (m_lift.phase != Lift::Phase::None && m_lift.phase != Lift::Phase::Throw) endLift();
    if (m_active) {
        finishCardGrab(false);
        if (m_presentation == CardPresentation::Desktop) {
            // The cards come back into the row, and going back returns here.
            m_presentation = CardPresentation::Spread;
            m_returnToDesktop = true;
            bringCardsBack();
            m_host->setPagingShortcutsForCardStage(true);
        } else if (m_presentation == CardPresentation::Spread) {
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
                // The person came from the layout, so going back resumes it.
                m_returnToGroup = true;
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
    forgetCloseAsk(nullptr);
    m_heldInPlace.clear();
    bringCardsBack();
    m_returnToDesktop = false;
    m_returnedToDesktop.clear();
    stopOpeningSpread();
    m_transferGuard.invalidate();
    m_host->cancelInputForCardStage();
    clearCardTransition();
    m_row = {};
    finishScrub();
    m_lift = {};
    m_motionClock.invalidate();
    // A window still closing stays out of sight; one that did not close is
    // going back to the desktop and must be seen there.
    m_thrown.removeIf([](const Thrown &thrown) {
        return !thrown.window || !thrown.window->isDeleted();
    });
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
    // Only on the desktop shown: activating a window elsewhere would switch
    // desktops under the person while every desktop is being released.
    if (releasedWindow && !releasedWindow->isDeleted()
        && releasedWindow->window() && releasedWindow->window()->isFullScreen()
        && releasedWindow->isOnCurrentDesktop()
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
    // Behind a layout the card is drawn nowhere, so it must also be under the
    // panes, or the touches on them would reach it.
    if (m_presentation == CardPresentation::Bento) KWin::workspace()->lowerWindow(window->window());
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "took" << window->caption()
            << "as a card after its display went away;"
            << m_workspace.windows().size() << "individual cards";
    return true;
}

bool CardStageController::startBehindLayout()
{
    if (m_active || !m_workspace.windows().isEmpty()) return false;
    // As for a sleeping pane: the display is showing panes, and a card behind
    // them is drawn nowhere until the person calls it or opens Spread.
    m_active = true;
    m_presentation = CardPresentation::Bento;
    m_host->setPagingShortcutsForCardStage(false);
    return true;
}

void CardStageController::stopBehindLayoutIfEmpty()
{
    if (!m_active || !m_workspace.windows().isEmpty() || m_presentation != CardPresentation::Bento) return;
    m_active = false;
    m_presentation = CardPresentation::Spread;
    m_originalCardStackingOrder.clear();
}

bool CardStageController::admitDisplacedPaneAsHiddenCard(
    KWin::EffectWindow *window, const std::function<bool()> &commitSource,
    const NativeMoveSnapshot *restore)
{
    QPointer<KWin::LogicalOutput> tablet = m_host->tabletOutputForCardStage();
    // Each refusal says which rule refused, so a pane left plain on hardware
    // can be traced to it.
    const char *refusal = !m_active ? "cards are not active"
        : m_presentation != CardPresentation::Bento ? "the display is not presenting Bento"
        : m_cardGrabActive ? "a card is held"
        : m_launcherGuestActive ? "the launcher guest is open"
        : !tablet ? "there is no card display"
        : !window || window->isDeleted() || !window->window() ? "the window is gone"
        : !window->isNormalWindow() ? "the window is not a normal window"
        : !m_host->isManagedWindowForCardStage(window) ? "the window is not one cards hold"
        : window->isUserMove() || window->isUserResize() ? "the window is being moved or resized"
        : liveCardIndex(window) >= 0 ? "the window is already a card"
        : nullptr;
    if (refusal) {
        qInfo() << "Kadunce" << Revision << "did not take a displaced pane as a hidden card:" << refusal
                << (window ? window->caption() : QString());
        return false;
    }
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
    if (!admission) {
        qInfo() << "Kadunce" << Revision << "did not take a displaced pane as a hidden card:"
                << "the card workspace refused it" << window->caption();
        return false;
    }
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
    dropRowMotion();
    if (m_arrivalWindow) clearCardTransition();
    finishCardGrab(false);
    const bool wasActive = m_presentation == CardPresentation::Active;
    // An open card's side swipe goes round its Stack, which it never leaves;
    // a card on its own walks the row. In Spread the row moves.
    bool activeStack = false;
    if (wasActive) {
        const ActiveStep step = activeStep(m_workspace.stackSizeForId(m_workspace.selectedId()),
            m_workspace.selectedIndex(), m_workspace.count(), delta);
        activeStack = step.along == ActiveStep::Along::Stack;
        delta = step.delta;
    } else if (!m_launcherGuestActive) {
        // The row has two ends; paging stops at them.
        const int selectedIndex = m_workspace.selectedIndex();
        delta = std::clamp(delta, -selectedIndex, m_workspace.count() - 1 - selectedIndex);
    }
    if (delta == 0) return;
    if (!wasActive && !m_launcherGuestActive) {
        captureCardTransition(false, true);
        m_rowPageTransition = true;
    }
    if (wasActive) {
        parkActiveSnapshot();
    }
    if (activeStack) {
        m_workspace.pageStack(delta);
    } else {
        m_workspace.page(delta);
        // Arriving at a Stack from beside it, an open card starts at its
        // near end, so the walk goes through every card.
        const auto members = m_workspace.model().stackMembersForId(m_workspace.selectedId());
        if (wasActive && members.size() > 1)
            (void)m_workspace.selectCard(delta > 0 ? members.front() : members.back());
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
    dropRowMotion();
    captureCardTransition();
    m_arrivalTimer.stop();
    m_arrivalWindow.clear();
    m_arrivalExpanding = false;
    m_launcherGuestGroupCount = m_workspace.count();
    m_launcherGuestArrival = false;
    m_launcherGuestPrimarySide = m_workspace.count() == 2
        ? -rowNeighborSide() : 1;
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
    const KWin::RectF work = workArea(tablet);
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
            const auto work = workArea(output);
            if (work.width() > 0 && work.height() > 0) {
                m_previewOrigins.clear();
                for (const auto &window : std::as_const(m_workspace.windows())) {
                    if (!window || window->isDeleted()) continue;
                    const int slot = m_workspace.sameStack(liveCardIndex(window) + 1,
                        m_workspace.selectedId()) ? 0 : rowNeighborSide();
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

bool CardStageController::endLauncherGuestOnCard(const QPointF &position)
{
    // Read the card while the row still stands around Search: the card
    // there is not the one under that point once the row closes up.
    auto *tablet = m_host->tabletOutputForCardStage();
    KWin::EffectWindow *window = m_launcherGuestActive && !m_launcherGuestArrival && tablet
        ? cardAt(tablet, position) : nullptr;
    const int cardId = window ? liveCardIndex(window) + 1 : 0;
    endLauncherGuest();
    const int entry = cardId > 0 ? m_workspace.model().entryIndexForId(cardId) : -1;
    if (entry < 0) return false;
    if (entry != m_workspace.selectedIndex()) m_workspace.selectIndex(entry);
    qInfo() << "Kadunce" << Revision << "tap beside Search opens entry" << entry + 1;
    return true;
}

void CardStageController::pageStack(int delta)
{
    if (!m_active || m_presentation != CardPresentation::Spread
        || delta == 0 || selectedIsBentoProjection()
        || m_workspace.stackSizeForId(m_workspace.selectedId()) <= 1) {
        return;
    }
    m_host->cancelInputForCardStage();
    dropRowMotion();
    if (m_arrivalWindow) clearCardTransition(); // Explicit browsing wins over auto-expand.
    finishCardGrab(false);
    browseStack(delta);
}

void CardStageController::browseStack(int delta)
{
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

bool CardStageController::catchRow()
{
    if (!m_row.moving() || std::abs(m_row.velocity) <= RowCatchSpeed) return false;
    m_row.mode = RowMotion::Mode::Rest;
    m_row.velocity = 0.0;
    return true;
}

void CardStageController::settleRowFromStroke()
{
    if (m_row.mode != RowMotion::Mode::Rest || m_row.position == 0.0) return;
    settleRow(m_row, rowStops(m_host->tabletOutputForCardStage()));
    m_motionClock.start();
    KWin::effects->addRepaintFull();
}

bool CardStageController::beginRowDrag()
{
    auto *output = m_host->tabletOutputForCardStage();
    if (!m_active || m_presentation != CardPresentation::Spread || !output
        || m_cardGrabActive || m_launcherGuestActive || m_workspace.count() == 0
        || m_lift.phase != Lift::Phase::None) {
        return false;
    }
    // A canned page or an arrival still playing gives way to the hand.
    if (m_arrivalWindow || m_rowPageTransition) clearCardTransition();
    m_rowDragOrigin = rowUnstretch(m_row.position, rowStops(output));
    m_row.mode = RowMotion::Mode::Drag;
    m_row.velocity = 0.0;
    return true;
}

void CardStageController::updateRowDrag(double travel)
{
    if (m_row.mode != RowMotion::Mode::Drag) return;
    m_row.position = rowStretch(m_rowDragOrigin - travel,
                                rowStops(m_host->tabletOutputForCardStage()));
    KWin::effects->addRepaintFull();
}

void CardStageController::finishRowDrag(double velocity)
{
    if (m_row.mode != RowMotion::Mode::Drag) return;
    const RowStops row = rowStops(m_host->tabletOutputForCardStage());
    releaseRow(m_row, row, -velocity * 1000.0);
    // How hard a real hand throws is what the row's friction is judged by.
    const int aim = rowStopIndex(row, m_row.target);
    qInfo() << "Kadunce row let go at" << std::abs(velocity) << "px/ms from entry"
            << m_workspace.selectedIndex() + 1 << "of" << m_workspace.count() << "toward"
            << (m_row.mode == RowMotion::Mode::Coast ? QStringLiteral("an end")
                : aim < 0 ? QStringLiteral("nowhere") : QString::number(aim + 1));
    m_motionClock.start();
    KWin::effects->addRepaintFull();
}

bool CardStageController::beginScrub()
{
    if (!m_active || m_presentation != CardPresentation::Spread || m_cardGrabActive
        || m_launcherGuestActive || !m_row.still() || m_lift.phase != Lift::Phase::None
        || selectedIsBentoProjection()
        || m_workspace.stackSizeForId(m_workspace.selectedId()) <= 1) {
        return false;
    }
    if (m_arrivalWindow) clearCardTransition();
    m_scrubbing = true;
    m_scrubApplied = 0;
    return true;
}

void CardStageController::updateScrub(double travel)
{
    if (!m_scrubbing) return;
    const int steps = scrubSteps(travel);
    if (steps == m_scrubApplied) return;
    browseStack(steps - m_scrubApplied);
    m_scrubApplied = steps;
}

void CardStageController::finishScrub()
{
    m_scrubbing = false;
    m_scrubApplied = 0;
}

KWin::EffectWindow *CardStageController::cardAt(KWin::LogicalOutput *output,
                                                const QPointF &position) const
{
    // Nearest the eye first: the centred face, then every other face, then
    // the cards fanned behind them.
    QList<KWin::EffectWindow *> order;
    if (auto *selected = selectedWindow()) order.append(selected);
    for (int pass = 0; pass < 2; ++pass) {
        for (const auto &window : std::as_const(m_workspace.windows())) {
            if (!window || window->isDeleted() || order.contains(window.data())
                || thrownAway(window) || paintSlot(window) == 99) continue;
            const int cardId = liveCardIndex(window) + 1;
            const bool face = m_workspace.model().stackPositionForId(cardId)
                == m_workspace.model().stackActivePositionForId(cardId);
            if (face == (pass == 0)) order.append(window.data());
        }
    }
    for (auto *window : std::as_const(order)) {
        if (paintSlot(window) == 99) continue;
        auto rect = previewTargetForWindow(output, window);
        auto pose = stackPoseForWindow(window, rect.width());
        if (!pose.visible) continue;
        rect.translate(qRound(pose.x), qRound(pose.y));
        if (QRectF(rect).contains(position)) return window;
    }
    return nullptr;
}

bool CardStageController::beginLift(const QPointF &position)
{
    auto *output = m_host->tabletOutputForCardStage();
    if (!m_active || m_presentation != CardPresentation::Spread || !output
        || m_cardGrabActive || m_launcherGuestActive || m_lift.phase != Lift::Phase::None) {
        return false;
    }
    auto *window = cardAt(output, position);
    if (!window) return false;
    // A Bento group lifts the pane under the finger, which only a pull down
    // takes out.
    const bool inGroup = usesBentoProjectionAperture(window);
    if (inGroup) window = groupPaneAt(output, position);
    if (!window) return false;
    if (m_arrivalWindow) clearCardTransition();
    m_lift = {window, 0.0, 0.0,
              !inGroup && m_workspace.stackSizeForId(liveCardIndex(window) + 1) > 1,
              inGroup, Lift::Phase::Drag};
    KWin::effects->setElevatedWindow(window, true);
    KWin::effects->addRepaintFull();
    return true;
}

void CardStageController::updateLift(double travel)
{
    if (m_lift.phase != Lift::Phase::Drag) return;
    m_lift.y = m_lift.inGroup ? groupLiftShown(travel) : liftShown(travel, m_lift.inStack);
    KWin::effects->addRepaintFull();
}

void CardStageController::finishLift(double velocity)
{
    if (m_lift.phase != Lift::Phase::Drag) return;
    if (!m_lift.window || m_lift.window->isDeleted()) {
        endLift();
        return;
    }
    m_motionClock.start();
    switch (m_lift.inGroup ? groupLiftOutcome(m_lift.y, velocity)
                           : liftOutcome(m_lift.y, velocity, m_lift.inStack)) {
    case LiftOutcome::Close:
        throwLifted(velocity);
        break;
    case LiftOutcome::PullOut:
        pullOutLifted();
        break;
    case LiftOutcome::Return:
        m_lift.phase = Lift::Phase::Return;
        m_lift.velocity = velocity * 1000.0;
        break;
    }
    KWin::effects->addRepaintFull();
}

void CardStageController::throwLifted(double velocity)
{
    const QPointer<KWin::EffectWindow> window = m_lift.window;
    m_lift.phase = Lift::Phase::Throw;
    m_lift.velocity = std::min(velocity * 1000.0, -CloseThrowSpeed);
    qInfo() << "Kadunce" << Revision << "flicked card" << liveCardIndex(window) + 1
            << "closed:" << window->caption();
    forgetCloseAsk(window);
    CloseAsk ask{window, {}, {}, {}};
    ask.asked.start();
    ask.damage = QObject::connect(window, &KWin::EffectWindow::windowDamaged, &m_thrownTimer,
                                  [this](KWin::EffectWindow *drawn) {
        for (auto &ask : m_closeAsks)
            if (ask.window == drawn && !ask.drew.isValid()) ask.drew.start();
    });
    m_closeAsks.append(ask);
    // Ask as it leaves the hand, so a quick app is gone by the time the card is.
    window->closeWindow();
}

void CardStageController::pullOutLifted()
{
    const QPointer<KWin::EffectWindow> window = m_lift.window;
    const bool group = m_lift.inGroup;
    const int cardId = liveCardIndex(window) + 1;
    // The card travels from where the finger left it to its own place.
    captureCardTransition(false, true);
    endLift();
    if (cardId <= 0 || !(group ? pullPaneOutOfGroup(window) : m_workspace.releaseMember(cardId))) {
        clearCardTransition();
        KWin::effects->addRepaintFull();
        return;
    }
    m_slideLeavers = true;
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "pulled card" << cardId
            << (group ? "out of the Bento group" : "out of its Stack")
            << "; it now stands alone at entry"
            << m_workspace.model().entryIndexForId(cardId) + 1;
}

std::optional<CardRect> CardStageController::groupPaneDrawn(KWin::LogicalOutput *output,
                                                            KWin::EffectWindow *pane) const
{
    if (!m_bentoProjectionSession || !output || !pane || pane->isDeleted() || pane->isMinimized()
        || !m_bentoProjectionPaneWindows.contains(pane)) return std::nullopt;
    const auto area = bentoProjectionWorkspace();
    const CardRect workspace{double(area.x()), double(area.y()),
                             double(area.width()), double(area.height())};
    const auto stage = makeBentoStageArea(workspace);
    const auto stored = bentoProjectionRect(pane);
    const auto target = previewTargetForWindow(output, pane);
    if (!stored || target.width() <= 0 || stage.width <= 0.0) return std::nullopt;
    // The pane where the group card draws it, from the layout's own place.
    const auto composite = makeBentoCompositeGeometry(
        {double(target.x()), double(target.y()), double(target.width()), double(target.height())},
        workspace);
    return mapBentoCompositeRect(composite,
        {stage.x + stored->x * stage.width, stage.y + stored->y * stage.height,
         stored->width * stage.width, stored->height * stage.height});
}

KWin::EffectWindow *CardStageController::groupPaneAt(KWin::LogicalOutput *output,
                                                    const QPointF &position) const
{
    if (!m_bentoProjectionSession || !output) return nullptr;
    KWin::EffectWindow *nearest = nullptr;
    double nearestDistance = 0.0;
    for (const auto &pane : m_bentoProjectionPaneWindows) {
        const auto drawn = groupPaneDrawn(output, pane);
        if (!drawn) continue;
        const double distance = carryDistanceTo(*drawn, position.x(), position.y());
        if (!nearest || distance < nearestDistance) {
            nearest = pane;
            nearestDistance = distance;
        }
    }
    return nearest;
}

bool CardStageController::pullPaneOutOfGroup(KWin::EffectWindow *pane)
{
    const int pulledId = liveCardIndex(pane) + 1;
    if (!m_bentoProjectionSession || !isBentoProjectionPane(pane) || pulledId <= 0
        || m_cardGrabActive) return false;
    const auto members = m_bentoProjectionWindows;
    for (const auto &member : members) {
        if (!member || !m_workspace.sameStack(liveCardIndex(member) + 1, pulledId)) return false;
    }
    // The group is no longer one: nothing is left to resume, and its members
    // are drawn as the ordinary cards they already are. Each keeps the record
    // it came into the group with, so release still returns it where it began.
    m_bentoProjectionSession.reset();
    m_bentoProjectionWindows.clear();
    m_bentoProjectionPaneWindows.clear();
    m_returnToGroup = false;
    m_host->retireBentoProjectionForCardStage(members);
    // The rest leave the group's Stack after its first awake pane, in order,
    // and the pulled pane stands just after where the group stood.
    QPointer<KWin::EffectWindow> kept;
    for (const auto &member : members) {
        if (member && member != pane && !member->isMinimized()) {
            kept = member;
            break;
        }
    }
    for (int i = members.size() - 1; i >= 0; --i) {
        const auto &member = members.at(i);
        const int cardId = liveCardIndex(member) + 1;
        if (!member || member == pane || member == kept || cardId <= 0) continue;
        if (m_workspace.stackSizeForId(cardId) > 1) (void)m_workspace.releaseMember(cardId);
    }
    if (m_workspace.stackSizeForId(pulledId) > 1) (void)m_workspace.releaseMember(pulledId);
    for (const auto &member : members) {
        if (member && !member->isDeleted()) KWin::effects->setElevatedWindow(member, false);
    }
    return m_workspace.stackSizeForId(pulledId) == 1;
}

KWin::EffectWindow *CardStageController::groupPane(int part) const
{
    if (!m_bentoProjectionSession || part < 0 || part >= m_bentoProjectionSession->panes.size())
        return nullptr;
    return m_bentoProjectionSession->panes.at(part).window.data();
}

bool CardStageController::groupTakesHeldCard() const
{
    // A held card from the row is a card of its own; the partner test is the
    // one that says a card, awake and on this display, may become a pane.
    // A finger over another display is taking the card there instead.
    auto *held = m_cardGrabActive && !m_carry.inStack && m_cardGrabDestinationOutput.isEmpty()
        ? selectedWindow() : nullptr;
    return m_bentoProjectionSession && held && m_workspace.selectedIsStandalone()
        && isEligiblePartner(held) && managedRestore(held).has_value();
}

bool CardStageController::groupPaneHolds(int part) const
{
    auto *held = selectedWindow();
    if (!held || !held->window() || !groupPane(part)) return false;
    const auto &projection = *m_bentoProjectionSession;
    const auto &area = projection.workspaceArea;
    const auto stage = makeBentoStageArea(
        {double(area.x()), double(area.y()), double(area.width()), double(area.height())});
    const auto pixels = makePixelBentoLayout(projection.rects, int(std::lround(stage.x)),
        int(std::lround(stage.y)), int(std::lround(stage.width)), int(std::lround(stage.height)));
    if (std::size_t(part) >= pixels.size()) return false;
    // The pane keeps its size, so a card it cannot hold does not take it.
    const QSizeF minimum = held->window()->minSize();
    const auto &pixel = pixels[std::size_t(part)];
    return pixel.width >= minimum.width() && pixel.height >= minimum.height();
}

int CardStageController::groupPartFor(const QPointF &position, int kept) const
{
    auto *output = m_host->tabletOutputForCardStage();
    if (!m_bentoProjectionSession || !output) return -1;
    const auto &panes = m_bentoProjectionSession->panes;
    std::vector<CardRect> drawn(std::size_t(panes.size()), CardRect{0.0, 0.0, 0.0, 0.0});
    std::vector<bool> holds(std::size_t(panes.size()), false);
    for (int part = 0; part < panes.size(); ++part) {
        const auto rect = groupPaneDrawn(output, panes.at(part).window.data());
        if (!rect) continue;
        drawn[std::size_t(part)] = *rect;
        holds[std::size_t(part)] = groupPaneHolds(part);
    }
    return carryGroupPart(drawn, holds, position.x(), position.y(), kept);
}

bool CardStageController::replaceGroupPane(int part)
{
    KWin::EffectWindow *held = selectedWindow();
    KWin::EffectWindow *pane = groupPane(part);
    const int heldId = liveCardIndex(held) + 1;
    const int paneId = liveCardIndex(pane) + 1;
    if (!pane || heldId <= 0 || paneId <= 0 || !groupTakesHeldCard() || !groupPaneHolds(part)
        || m_workspace.stackSizeForId(paneId) < 2) return false;
    const auto restore = managedRestore(held);
    if (!restore) return false;
    // The card takes the pane's rect, so every other pane keeps its window,
    // its size and its side, and the group resumes with the card in it. The
    // card comes with the record it has as a card, so release still returns
    // it where it began.
    BentoProjectionSession projection = *m_bentoProjectionSession;
    projection.panes[part] = {held, *restore, false};
    if (projection.lead == pane) projection.lead = held;
    if (projection.sideWindow == pane) projection.sideWindow = held;
    for (auto &stacked : projection.stackingOrder)
        if (stacked == pane) stacked = held;
    if (!validBentoProjectionShape(projectionShape(projection))) return false;
    // In the row the card joins the group's Stack where the pane stood in it,
    // and the pane leaves it to stand just after the group, as a pane pulled
    // out does. Both or neither.
    const int position = m_workspace.stackPositionForId(paneId);
    SpreadModel trial = m_workspace.model();
    if (!trial.stackSelectedWith(paneId, position) || !trial.releaseMember(paneId)) return false;
    if (!m_workspace.stackSelectedWith(paneId, position) || !m_workspace.releaseMember(paneId))
        return false;
    if (m_activeRestore.window == held) parkActiveSnapshot();
    retireActiveIdentity(held);
    *m_bentoProjectionSession = projection;
    for (auto *list : {&m_bentoProjectionWindows, &m_bentoProjectionPaneWindows})
        for (auto &member : *list)
            if (member == pane) member = held;
    if (!pane->isDeleted()) KWin::effects->setElevatedWindow(pane, false);
    ++m_restoreGeneration;
    qInfo() << "Kadunce" << Revision << "card" << heldId << held->caption()
            << "took pane" << part + 1 << "of the Bento group from" << pane->caption()
            << "; it now stands alone at entry" << m_workspace.model().entryIndexForId(paneId) + 1;
    return true;
}

bool CardStageController::cardAtPoint(const QPointF &position) const
{
    auto *tablet = m_host->tabletOutputForCardStage();
    return m_active && m_presentation == CardPresentation::Spread && tablet
        && cardAt(tablet, position);
}

bool CardStageController::selectReturnEntry()
{
    // Where the person was: the Bento layout they opened Spread from, or the
    // card they last had open, or failing both the entry already centred.
    if (m_returnToGroup) {
        for (const auto &window : m_bentoProjectionPaneWindows) {
            const int cardId = liveCardIndex(window) + 1;
            const int entry = cardId > 0 ? m_workspace.model().entryIndexForId(cardId) : -1;
            if (entry >= 0) {
                m_workspace.selectIndex(entry);
                return true;
            }
        }
    }
    if (auto *active = activeCardIdentity()) {
        const int cardId = liveCardIndex(active) + 1;
        if (cardId > 0 && m_workspace.selectCard(cardId)) return true;
    }
    return m_workspace.count() > 0;
}

bool CardStageController::backFromSpread()
{
    if (!m_active || m_presentation != CardPresentation::Spread || m_cardGrabActive
        || m_lift.phase != Lift::Phase::None) return false;
    if (m_returnToDesktop) {
        showDesktop();
        return false;
    }
    return selectReturnEntry();
}

bool CardStageController::beginOpenSpread()
{
    if (!m_active || m_presentation != CardPresentation::Active || m_cardGrabActive
        || m_launcherGuestActive || m_lift.phase != Lift::Phase::None || m_openProgress
        || !m_host->tabletOutputForCardStage()) return false;
    m_host->cancelInputForCardStage();
    dropRowMotion();
    clearCardTransition();
    parkActiveSnapshot();
    m_presentation = CardPresentation::Spread;
    m_openProgress = 0.0;
    m_openReturn.invalidate();
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "opening Spread under three fingers";
    return true;
}

void CardStageController::followOpenSpread(double progress)
{
    if (!m_openProgress || m_openReturn.isValid()) return;
    m_openProgress = std::clamp(progress, 0.0, 1.0);
    KWin::effects->addRepaintFull();
}

void CardStageController::finishOpenSpread(bool open)
{
    if (!m_openProgress || m_openReturn.isValid()) return;
    if (open) {
        // The row finishes forming from wherever the fingers left it.
        captureCardTransition();
        m_openProgress.reset();
        qInfo() << "Kadunce" << Revision << "changed to Spread under three fingers";
    } else {
        m_openReturnFrom = *m_openProgress;
        m_openReturn.start();
        m_openReturnTimer.start(motion(OpenSpreadReturnDuration));
    }
    KWin::effects->addRepaintFull();
}

double CardStageController::spreadOpenProgress() const
{
    if (!m_openProgress) return 1.0;
    if (!m_openReturn.isValid()) return *m_openProgress;
    const double u = std::clamp(double(m_openReturn.elapsed()) / motion(OpenSpreadReturnDuration), 0.0, 1.0);
    return m_openReturnFrom * (1.0 - QEasingCurve(QEasingCurve::OutCubic).valueForProgress(u));
}

void CardStageController::stopOpeningSpread()
{
    m_openReturnTimer.stop();
    m_openReturn.invalidate();
    m_openProgress.reset();
}

CardStageController::SpreadTap CardStageController::tapSpread(const QPointF &position)
{
    auto *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || m_presentation != CardPresentation::Spread || !tablet
        || m_cardGrabActive || m_launcherGuestActive || !m_row.still()
        || m_lift.phase != Lift::Phase::None) return SpreadTap::None;
    KWin::EffectWindow *window = cardAt(tablet, position);
    if (!window) {
        qInfo() << "Kadunce" << Revision << "tap on empty Spread: back to where the person was";
        if (m_returnToDesktop) {
            showDesktop();
            return SpreadTap::None;
        }
        return selectReturnEntry() ? SpreadTap::Open : SpreadTap::None;
    }
    const int cardId = liveCardIndex(window) + 1;
    const int entry = cardId > 0 ? m_workspace.model().entryIndexForId(cardId) : -1;
    if (entry < 0) return SpreadTap::None;
    const auto &model = m_workspace.model();
    const bool face = model.stackPositionForId(cardId) == model.stackActivePositionForId(cardId);
    if (!face && entry == m_workspace.selectedIndex() && !usesBentoProjectionAperture(window)) {
        // A card fanned behind the centred Stack's face comes forward, and
        // Spread stays open to see it.
        const int before = model.stackActivePositionForId(cardId);
        captureCardTransition(false, true);
        m_stackBrowseOutgoing = selectedWindow();
        (void)m_workspace.selectCard(cardId);
        m_stackBrowseDirection = model.stackActivePositionForId(cardId) < before ? -1 : 1;
        syncSelectedElevation();
        KWin::effects->addRepaintFull();
        qInfo() << "Kadunce" << Revision << "tap brought card" << cardId << "to its Stack's face";
        return SpreadTap::Forward;
    }
    // Any other card opens: a Stack's face, a card beside it, the Bento group.
    if (entry != m_workspace.selectedIndex()) m_workspace.selectIndex(entry);
    qInfo() << "Kadunce" << Revision << "tap opens entry" << entry + 1;
    return SpreadTap::Open;
}

void CardStageController::endLift()
{
    const auto window = m_lift.window;
    m_lift = {};
    if (window && !window->isDeleted()) KWin::effects->setElevatedWindow(window, false);
    syncSelectedElevation();
}

void CardStageController::cancelStroke()
{
    auto *output = m_host->tabletOutputForCardStage();
    if (m_row.mode == RowMotion::Mode::Drag
        || (m_row.mode == RowMotion::Mode::Rest && m_row.position != 0.0)) {
        settleRow(m_row, rowStops(output));
        m_motionClock.start();
    }
    finishScrub();
    if (m_lift.phase == Lift::Phase::Drag) {
        m_lift.phase = Lift::Phase::Return;
        m_lift.velocity = 0.0;
        m_motionClock.start();
    }
    KWin::effects->addRepaintFull();
}

void CardStageController::advanceMotion()
{
    if (!m_motionClock.isValid()) return;
    const double seconds = std::min(0.05, m_motionClock.restart() / 1000.0);
    auto *output = m_host->tabletOutputForCardStage();
    if (m_cardGrabActive) advanceCarry(seconds);
    if (m_row.moving()) {
        if (!rowMotionApplies() || !output) m_row = {};
        else if (!stepRow(m_row, rowStops(output), seconds)) commitRowStop();
    }
    if (m_lift.phase == Lift::Phase::Return) {
        springToward(m_lift.y, m_lift.velocity, 0.0, LiftSpring, seconds);
        if (std::abs(m_lift.y) < 1.0 && std::abs(m_lift.velocity) < 20.0) endLift();
    } else if (m_lift.phase == Lift::Phase::Throw) {
        m_lift.y += m_lift.velocity * seconds;
        const auto window = m_lift.window;
        const auto rect = output && window ? previewTargetForWindow(output, window) : KWin::Rect();
        const auto work = output ? workArea(output) : KWin::RectF();
        if (!window || window->isDeleted() || rect.width() <= 0 || rect.bottom() < work.top()) {
            // Out of sight: the next card can be lifted while this one closes.
            if (window && !window->isDeleted() && liveCardIndex(window) >= 0) {
                Thrown thrown{window, {}};
                thrown.since.start();
                m_thrown.append(thrown);
                m_thrownTimer.start();
            }
            endLift();
        }
    }
    if (!animationsRunning()) m_motionClock.invalidate();
    KWin::effects->addRepaintFull();
}

void CardStageController::commitRowStop()
{
    auto *output = m_host->tabletOutputForCardStage();
    const int entry = rowStopIndex(rowStops(output), m_row.position);
    const int offset = entry < 0 ? 0 : entry - m_workspace.selectedIndex();
    if (offset == 0) {
        m_row = {};
        KWin::effects->addRepaintFull();
        return;
    }
    // The row stands exactly where the new selection draws its cards; only a
    // Stack's fan opening or closing is left to travel.
    captureCardTransition(false, true);
    m_row = {};
    m_workspace.page(offset);
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce row settled on entry" << m_workspace.selectedIndex() + 1
            << "of" << m_workspace.count();
}

void CardStageController::dropRowMotion()
{
    finishScrub();
    if (m_row.still()) return;
    const auto row = rowStops(m_host->tabletOutputForCardStage());
    m_row.position = nearestRowStop(row, m_row.position);
    m_row.mode = RowMotion::Mode::Rest;
    m_row.velocity = 0.0;
    commitRowStop();
}

double CardStageController::liftOpacity(const KWin::EffectWindow *window) const
{
    if (m_lift.phase == Lift::Phase::None || window != m_lift.window || m_lift.y >= 0.0) return 1.0;
    return std::clamp(1.0 + m_lift.y / 900.0, 0.35, 1.0);
}

bool CardStageController::thrownAway(const KWin::EffectWindow *window) const
{
    if (!window) return false;
    for (const auto &thrown : m_thrown)
        if (thrown.window == window) return true;
    return false;
}

void CardStageController::checkThrownCards()
{
    for (int i = m_thrown.size() - 1; i >= 0; --i) {
        const auto window = m_thrown.at(i).window;
        if (!window) {
            m_thrown.removeAt(i);
            continue;
        }
        // A closed window's card is gone; its thrown mark stays until KWin
        // lets the window go, so no close animation draws it full size.
        if (window->isDeleted() || liveCardIndex(window) < 0) continue;
        if (window->findModal() || askedInside(window)) returnThrownCard(window, true);
        else if (m_thrown.at(i).since.elapsed() >= ThrownCloseWait) returnThrownCard(window, false);
    }
    if (m_thrown.isEmpty()) m_thrownTimer.stop();
}

bool CardStageController::askedInside(const KWin::EffectWindow *window) const
{
    for (const auto &ask : m_closeAsks)
        if (ask.window == window)
            return ask.drew.isValid() && ask.asked.elapsed() >= ThrownQuestionWait;
    return false;
}

void CardStageController::forgetCloseAsk(const KWin::EffectWindow *window)
{
    for (int i = m_closeAsks.size() - 1; i >= 0; --i) {
        const auto &asked = m_closeAsks.at(i).window;
        if (window && asked && asked != window) continue;
        QObject::disconnect(m_closeAsks.at(i).damage);
        m_closeAsks.removeAt(i);
    }
}

void CardStageController::returnThrownCard(KWin::EffectWindow *window, bool present)
{
    m_thrown.removeIf([window](const Thrown &thrown) { return thrown.window == window; });
    qint64 asked = -1;
    for (const auto &ask : m_closeAsks)
        if (ask.window == window) asked = ask.asked.elapsed();
    forgetCloseAsk(window);
    if (!m_active || m_presentation != CardPresentation::Spread) return;
    qInfo() << "Kadunce" << Revision << "flicked card came back:" << window->caption()
            << (present ? "it asked a question" : "it did not close")
            << asked << "ms after it was asked to close";
    if (m_lift.phase == Lift::Phase::None && !m_cardGrabActive) {
        // Drops back into its place from above.
        auto *output = m_host->tabletOutputForCardStage();
        const auto rect = output ? previewTargetForWindow(output, window) : KWin::Rect();
        const auto work = output ? workArea(output) : KWin::RectF();
        m_lift = {window, rect.width() > 0 ? work.top() - rect.bottom() : 0.0, 0.0,
                  false, false, Lift::Phase::Return};
        m_motionClock.start();
    }
    if (present && !m_cardGrabActive && !m_launcherGuestActive) {
        dropRowMotion();
        if (selectCardEntry(window)) {
            syncSelectedElevation();
            m_host->presentSelectedForCardStage();
        }
    }
    KWin::effects->addRepaintFull();
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
    int awakeIndex = -1;
    QList<QPointer<KWin::EffectWindow>> admitted;
    const QList<KWin::EffectWindow *> windows = KWin::effects->stackingOrder();
    for (KWin::EffectWindow *window : windows) {
        // CARD-LIFECYCLE.md §2 and §7: a window minimized before Kadunce held
        // the display is held too, as a sleeping card, so picking it wakes it
        // as a card. Only an awake window can be the one presented.
        if (!m_host->mayHoldWindowForCardStage(window)
            || window->screen() != tablet) {
            continue;
        }
        const bool awake = m_host->isManagedWindowForCardStage(window);
        if (awake && window == active) {
            activeIndex = admitted.size();
        }
        if (awake) {
            awakeIndex = admitted.size();
        }
        admitted.append(window);
    }
    // Nothing awake is nothing to present: the display waits for a window to
    // open, as it does holding nothing, and no minimized window is woken.
    if (awakeIndex < 0) {
        admitted.clear();
    }
    for (const auto &window : std::as_const(admitted)) {
        m_host->connectManagedWindowForCardStage(window);
    }
    // Entry publishes the whole admitted set before any of it is described.
    // A restore record is defined only for a published card, so capture here
    // cannot precede the reset that publishes them.
    publishOwnershipThenRecord(
        [&] {
            m_workspace.reset(admitted,
                activeIndex >= 0 ? activeIndex : awakeIndex);
            return true;
        },
        [&] {
            for (const auto &window : admitted) retainManagedOwnership(window);
            if (!admitted.isEmpty()) m_originalCardStackingOrder = admitted;
        });
    settleCardsInActivePlace(admitted);
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
    // A card chosen from the desktop or from Spread opened there: the cards
    // come back before one is shown.
    m_returnToDesktop = false;
    bringCardsBack();
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
    m_returnToGroup = false;
    // All entry paths must retire Spread's temporary compositor elevation.
    // Active is a native window; panel popups must retain their normal layers.
    syncSelectedElevation();
    m_activeSettleTimer.start();
    // A keyboard may already be up over the card that just arrived.
    refreshKeyboardRoom();
    KWin::workspace()->raiseWindow(client);
    KWin::workspace()->activateWindow(client, true);
    qInfo() << "Kadunce" << Revision
            << "entered interactive Active with" << effectWindow->caption();
    return true;
}

void CardStageController::settleCardsInActivePlace(const QList<QPointer<KWin::EffectWindow>> &cards)
{
    // §3: a card behind the one presented stands where it would as Active. A
    // window taken maximized would otherwise still reach the bottom of the
    // work area, where a panel that watches for windows reaching it stays
    // opaque until each card has been brought forward once. Its own place is
    // already in its restore record, so release gives it back.
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!tablet) return;
    const KWin::RectF target(activeTarget(tablet));
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    int settled = 0;
    for (const auto &window : cards) {
        if (!window || window->isDeleted() || !window->window()
            || !m_host->isManagedWindowForCardStage(window)
            || window->screen() != tablet) continue;
        KWin::Window *client = window->window();
        if (client->isFullScreen()) client->setFullScreen(false);
        if (client->maximizeMode() != KWin::MaximizeRestore) client->maximize(KWin::MaximizeRestore);
        if (client->quickTileMode() != KWin::QuickTileMode{})
            client->setQuickTileMode(KWin::QuickTileMode{}, window->frameGeometry().center());
        if (client->moveResizeGeometry() == target) continue;
        client->moveResize(target);
        ++settled;
    }
    if (settled > 0)
        qInfo() << "Kadunce" << Revision << "stood" << settled << "cards in the Active card's place";
}

void CardStageController::holdCardInActivePlace(KWin::EffectWindow *window)
{
    // §3: an application that restores its saved state maximizes its window a
    // moment after showing it, by which time another card may stand in front.
    // Its wish is kept in its restore record, so release gives it back
    // maximized, and the card goes back to the Active place. A card is put
    // back at most every half second, and one whose minimum size cannot fit
    // the place is left alone, so no application is fought in a loop.
    if (m_applyingWindowState || m_cardGrabActive || !window || window->isDeleted()
        || !window->window() || liveCardIndex(window) < 0
        || m_bentoProjectionWindows.contains(window)
        || !m_host->isManagedWindowForCardStage(window)) return;
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!tablet || window->screen() != tablet) return;
    KWin::Window *client = window->window();
    if (client->isInteractiveMove() || client->isInteractiveResize()) return;
    const KWin::RectF target(activeTarget(tablet));
    const bool asked = client->isRequestedFullScreen()
        || client->requestedMaximizeMode() != KWin::MaximizeRestore
        || client->requestedQuickTileMode() != KWin::QuickTileMode{};
    const KWin::RectF placed = client->moveResizeGeometry();
    const bool reaches = placed.bottom() > target.bottom() + 1
        || placed.right() > target.right() + 1;
    if (!asked && !reaches) return;
    const QSizeF minimum = client->minSize();
    if (minimum.width() > target.width() || minimum.height() > target.height()) return;
    auto &since = m_heldInPlace[window];
    if (since.isValid() && since.elapsed() < 500) return;
    since.start();
    if (asked) {
        for (auto &saved : m_parkedRestores) {
            if (saved.window != window) continue;
            saved.maximizeMode = client->requestedMaximizeMode();
            saved.fullScreen = client->isRequestedFullScreen();
            saved.quickTileMode = client->requestedQuickTileMode();
            break;
        }
    }
    const QPointer<KWin::EffectWindow> held(window);
    QTimer::singleShot(0, &m_activeSettleTimer, [this, held] {
        if (!held || held->isDeleted() || !held->window() || !m_active
            || liveCardIndex(held) < 0 || m_activeRestore.window == held) return;
        settleCardsInActivePlace({held});
    });
}

void CardStageController::restoreActiveSnapshot()
{
    ++m_restoreGeneration;
    m_activeSettleTimer.stop();
    m_activeSettleRemaining = 0;
    m_keyboardRoomTimer.stop();
    m_keyboardRoomRelease.stop();
    m_keyboardRestTimer.stop();
    m_keyboardHeadingTimer.stop();
    m_keyboardHeadingPending = false;
    m_keyboardHeadingTop.reset();
    m_keyboardRoom.reset();
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
    // A card leaving Active stays a card, so it leaves at its own size rather
    // than the one it gave up for the keyboard.
    if (m_keyboardRoom && m_keyboardRoom->window == m_activeRestore.window) {
        putBackKeyboardRoom();
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
        const auto work = workArea(tablet);
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
    m_arrivalTimer.start(motion(PreviewTransitionDuration));
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
        const auto work = workArea(output);
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
    // A new card changes the row under a moving hand: land it on a card first.
    dropRowMotion();

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
    for (const auto &ask : m_closeAsks) {
        if (ask.window != window) continue;
        qInfo() << "Kadunce" << Revision << "flicked card's app closed" << ask.asked.elapsed()
                << "ms after it was asked;" << (ask.drew.isValid()
                    ? qPrintable(QStringLiteral("it drew %1 ms after").arg(ask.asked.elapsed() - ask.drew.elapsed()))
                    : "it drew nothing in between");
    }
    forgetCloseAsk(window);
    m_originalCardStackingOrder.removeAll(window);
    m_aside.removeAll(window);
    m_returnedToDesktop.removeAll(window);
    const int closedIndex = liveCardIndex(window);
    if (!m_active || closedIndex < 0) {
        return;
    }

    m_host->cancelInputForCardStage();
    dropRowMotion();
    // KWin hands the focus on as soon as this returns, in the same call, and
    // that choice is not the person's: Spread stays where it is.
    if (m_presentation == CardPresentation::Spread && !m_closeHandoff) {
        m_closeHandoff = true;
        QTimer::singleShot(0, &m_thrownTimer, [this]() { m_closeHandoff = false; });
    }
    const bool flicked = thrownAway(window)
        || (window == m_lift.window && m_lift.phase == Lift::Phase::Throw);
    if (window == m_lift.window) {
        // Closed before it left the screen: it stays out of sight all the same.
        if (flicked && !thrownAway(window)) {
            Thrown thrown{window, {}};
            thrown.since.start();
            m_thrown.append(thrown);
            m_thrownTimer.start();
        }
        endLift();
    }

    if (m_workspace.count() == 3 && m_workspace.stackSizeForId(closedIndex + 1) == 1)
        captureCardTransition();
    else if (flicked && m_presentation == CardPresentation::Spread)
        captureCardTransition(false, true); // The row closes the gap it left.
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

bool CardStageController::releaseCard(KWin::EffectWindow *window)
{
    const int index = liveCardIndex(window);
    if (!m_active || index < 0 || !window || window->isDeleted() || !window->window())
        return false;
    m_host->cancelInputForCardStage();
    finishCardGrab(false);
    clearCardTransition();
    const bool wasActive = m_activeRestore.window == window;
    // The Active card leaves at its own size, its keyboard room given back,
    // and its record joins the others to be read below.
    if (wasActive) parkActiveSnapshot();
    std::optional<ActiveRestoreSnapshot> record;
    for (const auto &saved : std::as_const(m_parkedRestores)) {
        if (saved.window == window && saved.valid) {
            record = saved;
            break;
        }
    }
    forgetManagedRestore(window);
    const bool removed = m_workspace.removeAt(index);
    m_originalCardStackingOrder.removeAll(window);
    KWin::effects->setElevatedWindow(window, false);
    m_host->unredirectForCardStage(window);
    if (record) {
        QScopedValueRollback<bool> applying(m_applyingWindowState, true);
        restoreWindowState(window->window(), *record, record->geometry, true);
    }
    qInfo() << "Kadunce" << Revision << "let" << window->caption()
            << "go for another desktop;" << m_workspace.windows().size() << "individual cards";
    if (m_workspace.windows().isEmpty()) {
        release();
        return true;
    }
    if (!removed) {
        if (m_presentation == CardPresentation::Active) parkActiveSnapshot();
        m_presentation = CardPresentation::Spread;
        rebuildLiveCards();
    } else if (wasActive) {
        m_presentation = CardPresentation::Spread;
    }
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    return true;
}

void CardStageController::handleWindowActivated(KWin::EffectWindow *window)
{
    if (m_applyingWindowState || !m_active || !window || m_cardGrabActive) {
        return;
    }
    if (window == m_arrivalWindow) return; // Let its center/expand sequence finish.
    if (m_closeHandoff && m_presentation == CardPresentation::Spread) {
        qInfo() << "Kadunce" << Revision << "kept Spread as a closed card handed the focus to"
                << window->caption();
        return;
    }
    // §2: this stage keeps its cards hidden while the display presents its
    // Bento layout. An activation reaching here was not routed into the
    // layout, and drawing a card over live panes is the state §2 does not
    // name, so the presentation stands.
    if (m_presentation == CardPresentation::Bento) return;
    if (m_arrivalWindow) clearCardTransition(); // An explicit different activation wins.
    const int targetIndex = liveCardIndex(window);
    if (targetIndex < 0) {
        // §2: a window returned to the desktop is shown on it, so asking for
        // it shows the desktop; the cards would otherwise stand over it.
        if (m_presentation != CardPresentation::Desktop && m_returnedToDesktop.contains(window)) {
            m_host->cancelInputForCardStage();
            finishCardGrab(false);
            if (m_presentation == CardPresentation::Active) parkActiveSnapshot();
            showDesktop();
        }
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
    if (m_active && window && m_activeRestore.window != window) {
        holdCardInActivePlace(window);
        return;
    }
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
