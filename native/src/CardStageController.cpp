/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CardStageController.h"
#include "CardLineLayout.h"
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
}

CardStageController::CardStageController(CardStageHost *host)
    : m_host(host)
{
    m_arrivalTimer.setSingleShot(true);
    QObject::connect(&m_arrivalTimer, &QTimer::timeout, &m_arrivalTimer, [this]() {
        const auto window = m_arrivalWindow;
        if (!m_active || m_presentation != CardPresentation::CardLine
            || !window || window->isDeleted() || selectedWindow() != window
            || !window->window() || m_launcherGuestActive || m_cardGrabActive) {
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
        const auto target = activeTarget(tablet);
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

const CardLineModel &CardStageController::model() const
{
    return m_cardLine;
}

const QList<QPointer<KWin::EffectWindow>> &CardStageController::liveCards() const
{
    return m_liveCards;
}

KWin::EffectWindow *CardStageController::selectedWindow() const
{
    if (m_liveCards.isEmpty()) {
        return nullptr;
    }
    const int index = m_cardLine.selectedId() - 1;
    return index >= 0 && index < m_liveCards.size()
        ? m_liveCards.at(index).data() : nullptr;
}

int CardStageController::liveCardIndex(const KWin::EffectWindow *window) const
{
    for (int index = 0; index < m_liveCards.size(); ++index) {
        if (m_liveCards.at(index) == window) {
            return index;
        }
    }
    return -1;
}

int CardStageController::visibleSlot(const KWin::EffectWindow *window) const
{
    const int index = liveCardIndex(window);
    if (index < 0 || m_liveCards.isEmpty()) {
        return 99;
    }
    const int cardId = index + 1;
    if (m_launcherGuestActive && !m_launcherGuestArrival
        && m_presentation == CardPresentation::CardLine) {
        if (m_launcherGuestPrimaryWindow && m_cardLine.sameStack(cardId,
                liveCardIndex(m_launcherGuestPrimaryWindow) + 1)) {
            return m_launcherGuestPrimarySide;
        }
        if (m_launcherGuestSecondaryWindow && m_cardLine.sameStack(cardId,
                liveCardIndex(m_launcherGuestSecondaryWindow) + 1)) {
            return -m_launcherGuestPrimarySide;
        }
        return 99;
    }
    if (m_presentation == CardPresentation::Active) {
        return cardId == m_cardLine.selectedId() ? 0 : 99;
    }
    if (m_cardLine.sameStack(cardId, m_cardLine.selectedId())) {
        return 0;
    }
    if (m_cardGrabActive) {
        const std::array<int, 3> destinations =
            m_cardLine.detachedNeighborhood(m_cardGrabPageOffset);
        for (int slot = -1; slot <= 1; ++slot) {
            const int destination =
                destinations[static_cast<std::size_t>(slot + 1)];
            if (destination != 0
                && m_cardLine.sameStack(cardId, destination)) {
                return slot;
            }
        }
        return 99;
    }
    if (m_cardLine.count() == 2) {
        return m_cardLine.pairNeighborSide();
    }
    if (m_cardLine.count() >= 3) {
        const std::array<int, 3> neighborhood =
            m_cardLine.visibleNeighborhood();
        if (m_cardLine.sameStack(cardId, neighborhood[0])) {
            return -1;
        }
        if (m_cardLine.sameStack(cardId, neighborhood[2])) {
            return 1;
        }
    }
    return 99;
}

bool CardStageController::cardGrabActive() const
{
    return m_cardGrabActive;
}

double CardStageController::cardGrabOffset() const
{
    return m_cardGrabOffset;
}

int CardStageController::cardGrabPageOffset() const
{
    return m_cardGrabPageOffset;
}

int CardStageController::stackPreviewTarget() const
{
    return m_cardStackPreviewTarget;
}

bool CardStageController::stackPreviewArmed() const
{
    return m_cardStackPreviewArmed;
}

int CardStageController::stackInsertionIndex() const
{
    return m_cardStackInsertionIndex;
}

int CardStageController::previousStackInsertionIndex() const
{
    return m_cardStackPreviousInsertionIndex;
}

int CardStageController::stackBrowseTarget() const
{
    if (!m_cardGrabActive || m_cardLine.count() < 2) {
        return 0;
    }
    return m_cardLine.detachedNeighborhood(m_cardGrabPageOffset)[1];
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
    return QEasingCurve(QEasingCurve::InQuart).valueForProgress(progress);
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
    return (m_previewTransition.isValid()
            && m_previewTransition.elapsed() < PreviewTransitionDuration)
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
    const bool focusedPair = m_cardLine.count() == 2
        && (!m_launcherGuestActive || m_launcherGuestArrival) && !m_cardGrabActive;
    const CardLineLayout layout = focusedPair
        ? makeFocusedPairLayout(work.x(), work.y(), work.width(), work.height())
        : makeCardLineLayout(work.x(), work.y(), work.width(), work.height());
    CardStackEnvelope envelope{0.0, 0.0};
    if (m_presentation == CardPresentation::CardLine) {
        if (!m_cardGrabActive) {
            const int selectedId = m_cardLine.selectedId();
            const int memberCount = m_cardLine.stackSizeForId(selectedId);
            if (memberCount > 1) {
                envelope = makeOpenStackEnvelope(
                    memberCount,
                    m_cardLine.stackActivePositionForId(selectedId),
                    layout.cards[1].width, layout.cards[1].height);
            }
        } else {
            const int destinationId = stackBrowseTarget();
            const int destinationSize =
                m_cardLine.stackSizeForId(destinationId);
            if (m_cardStackPreviewTarget != 0) {
                envelope = makeInsertionStackEnvelope(
                    destinationSize + 1,
                    layout.cards[1].width, layout.cards[1].height);
            } else if (destinationSize > 1) {
                envelope = makeOpenStackEnvelope(
                    destinationSize,
                    m_cardLine.stackActivePositionForId(destinationId),
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
}

void CardStageController::captureCardTransition(bool includeGuest)
{
    auto *output = m_host->tabletOutputForCardStage();
    if (!output || m_presentation != CardPresentation::CardLine
        || (m_launcherGuestActive && !includeGuest) || m_cardGrabActive) {
        clearCardTransition();
        return;
    }
    const auto work = KWin::effects->clientArea(KWin::MaximizeArea, output);
    if (work.width() <= 0 || work.height() <= 0) return;
    QList<PreviewOrigin> origins;
    for (const auto &window : std::as_const(m_liveCards)) {
        if (!window || window->isDeleted() || visibleSlot(window) == 99) continue;
        const auto rect = previewTargetForWindow(output, window);
        origins.append({window, QRectF((rect.x() - work.x()) / work.width(),
            (rect.y() - work.y()) / work.height(), rect.width() / work.width(),
            rect.height() / work.height())});
    }
    m_previewOrigins = origins;
    m_previewTransition.start();
}

KWin::Rect CardStageController::previewTargetForWindow(
    KWin::LogicalOutput *output, const KWin::EffectWindow *window) const
{
    const int slot = visibleSlot(window);
    if (!output || slot == 99) return {};
    auto target = m_launcherGuestActive && !m_launcherGuestArrival ? launcherGuestTargetForSlot(output, slot)
                                       : cardTargetForSlot(output, slot);
    if (m_arrivalExpanding) {
        if (window == m_arrivalWindow) target = activeTarget(output);
        else target.translate(slot * output->geometry().width() / 2, 0);
    }
    const int duration = m_arrivalExpanding ? ArrivalExpandDuration : PreviewTransitionDuration;
    if (!m_previewTransition.isValid() || m_cardGrabActive
        || m_presentation != CardPresentation::CardLine
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
    const CardLineLayout layout = makeLauncherGuestLayout(work.x(), work.y(),
        work.width(), work.height(), m_launcherGuestGroupCount);
    const auto target = layout.cards[slot < 0 ? 0 : slot > 0 ? 2 : 1];
    return KWin::Rect(qRound(target.x), qRound(target.y),
                      qRound(target.width), qRound(target.height));
}

KWin::Rect CardStageController::activeTarget(KWin::LogicalOutput *output) const
{
    const KWin::RectF work = KWin::effects->clientArea(
        KWin::MaximizeArea, output);
    const CardRect target = makeActiveTarget(
        work.x(), work.y(), work.width(), work.height(), m_settings.gutter());
    return KWin::Rect(qRound(target.x), qRound(target.y),
                      qRound(target.width), qRound(target.height));
}

bool CardStageController::selectedStackContains(const QPointF &position) const
{
    if (!m_active || m_presentation != CardPresentation::CardLine) {
        return false;
    }
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    const int selectedId = m_cardLine.selectedId();
    const int memberCount = m_cardLine.stackSizeForId(selectedId);
    if (!tablet || memberCount <= 1) {
        return false;
    }
    const KWin::Rect center = m_launcherGuestActive ? cardTargetForSlot(tablet, 0)
        : previewTargetForWindow(tablet, selectedWindow());
    const CardStackEnvelope envelope = makeOpenStackEnvelope(
        memberCount, m_cardLine.stackActivePositionForId(selectedId),
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
        && m_presentation == CardPresentation::CardLine;
    KWin::EffectWindow *window = available ? selectedWindow() : nullptr;
    const int selectedId = available ? m_cardLine.selectedId() : 0;
    const int stackCount = available
        ? m_cardLine.stackSizeForId(selectedId) : 0;
    const int stackPosition = stackCount > 0
        ? m_cardLine.stackActivePositionForId(selectedId) + 1 : 0;
    return {
        available ? QStringLiteral("1") : QStringLiteral("0"),
        window ? window->caption() : QString(),
        QString::number(stackPosition),
        QString::number(stackCount),
    };
}

void CardStageController::beginCardGrab()
{
    clearCardTransition();
    if (!m_active || m_presentation != CardPresentation::CardLine
        || m_cardGrabActive || !selectedWindow()) {
        return;
    }
    if (!m_cardLine.selectedIsStandalone()
        && !m_cardLine.detachSelectedMember()) {
        return;
    }
    m_cardGrabActive = true;
    m_cardGrabOffset = 0.0;
    m_cardGrabPageOffset = 0;
    m_cardGrabMoved = false;
    m_cardStackPreviewTarget = 0;
    m_cardStackInsertionIndex = -1;
    m_cardStackPreviousInsertionIndex = -1;
    m_cardStackPreviewFrom = 0.0;
    m_cardStackPreviewTo = 0.0;
    m_cardStackPreviewArmed = false;
    m_cardStackPreviewTimer.invalidate();
    m_cardStackInsertionTimer.invalidate();
    m_cardGrabPointer = KWin::effects->cursorPos();
    m_cardGrabDestinationOutput.clear();
    KWin::effects->setElevatedWindow(selectedWindow(), true);
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "lifted Card Line card"
            << m_cardLine.selectedId();
}

void CardStageController::updateCardGrab(double horizontalDelta)
{
    if (!m_cardGrabActive) {
        return;
    }
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!tablet) {
        return;
    }
    const KWin::RectF work = KWin::effects->clientArea(
        KWin::MaximizeArea, tablet);
    const CardLineLayout layout = makeCardLineLayout(
        work.x(), work.y(), work.width(), work.height());
    const double pitch = layout.cards[1].width + layout.gutter;
    m_cardGrabOffset = std::clamp(
        horizontalDelta, -pitch * 1.10, pitch * 1.10);
    if (std::abs(m_cardGrabOffset) >= 36.0) {
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
    if (!m_cardGrabActive || m_cardLine.count() <= 2 || direction == 0) {
        return;
    }
    const int destinations = m_cardLine.count() - 1;
    const int next = m_cardGrabPageOffset + (direction < 0 ? -1 : 1);
    const int remainder = next % destinations;
    m_cardGrabPageOffset = remainder < 0 ? remainder + destinations : remainder;
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
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    int movement = 0;
    bool stacked = false;
    if (commit && m_cardStackPreviewArmed
        && m_cardStackPreviewTarget != 0) {
        stacked = m_cardLine.stackSelectedWith(
            m_cardStackPreviewTarget, m_cardStackInsertionIndex);
    } else if (commit && m_cardGrabMoved && tablet) {
        const KWin::RectF work = KWin::effects->clientArea(
            KWin::MaximizeArea, tablet);
        const CardLineLayout layout = makeCardLineLayout(
            work.x(), work.y(), work.width(), work.height());
        const double pitch = layout.cards[1].width + layout.gutter;
        if (m_cardGrabOffset <= -pitch * 0.82) {
            movement = -1;
        } else if (m_cardGrabOffset >= pitch * 0.82) {
            movement = 1;
        }
    }

    const bool restoreDetached = m_cardLine.hasDetachedMember()
        && (!commit || !m_cardGrabMoved);
    if (restoreDetached) {
        m_cardLine.restoreDetachedMember();
    } else {
        m_cardLine.commitDetachedMember();
    }
    if (!restoreDetached && !stacked && movement != 0) {
        m_cardLine.moveSelected(movement);
    }
    resetCardGrabState(grabbed, stacked);
    qInfo() << "Kadunce" << Revision << "released Card Line card"
            << m_cardLine.selectedId() << "movement" << movement
            << "stacked" << stacked;
}

void CardStageController::resetCardGrabState(
    KWin::EffectWindow *grabbed, bool stacked)
{
    if (grabbed && !grabbed->isDeleted() && !stacked) {
        KWin::effects->setElevatedWindow(grabbed, false);
    }
    m_cardGrabActive = false;
    m_cardGrabOffset = 0.0;
    m_cardGrabPageOffset = 0;
    m_cardGrabMoved = false;
    m_cardStackPreviewTarget = 0;
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
        || m_presentation != CardPresentation::CardLine) {
        return false;
    }
    KWin::LogicalOutput *destination = KWin::effects->screenAt(
        position.toPoint());
    if (!destination || m_host->isTabletOutputForCardStage(destination)) {
        return false;
    }
    KWin::EffectWindow *grabbed = selectedWindow();
    const int cardIndex = liveCardIndex(grabbed);
    if (!grabbed || cardIndex < 0 || !grabbed->window()) {
        return false;
    }

    KWin::Window *client = grabbed->window();
    m_cardLine.commitDetachedMember();
    KWin::effects->setElevatedWindow(grabbed, false);
    m_host->unredirectForCardStage(grabbed);
    m_originalCardStackingOrder.removeAll(grabbed);
    if (m_liveCards.size() > 1) {
        m_liveCards.removeAt(cardIndex);
        if (!m_cardLine.removeCard(cardIndex + 1)) {
            qWarning() << "Kadunce" << Revision
                       << "aborted cross-output model removal";
            m_liveCards.insert(cardIndex, grabbed);
            resetCardGrabState(grabbed, false);
            return true;
        }
    } else {
        m_liveCards.clear();
        m_originalCardStackingOrder.clear();
        m_active = false;
        m_presentation = CardPresentation::CardLine;
        m_host->setPagingShortcutsForCardStage(false);
    }

    resetCardGrabState(grabbed, false);
    if (client->isFullScreen()) {
        client->setFullScreen(false);
    }
    if (client->maximizeMode() != KWin::MaximizeRestore) {
        client->maximize(KWin::MaximizeRestore);
    }
    if (client->quickTileMode() != KWin::QuickTileMode{}) {
        client->setQuickTileMode(KWin::QuickTileMode{},
                                 client->frameGeometry().center());
    }
    client->sendToOutput(destination);
    const KWin::Rect target = activeTarget(destination);
    client->moveResize(KWin::RectF(target));

    const bool destinationManaged = m_host->admitCardToDesktopStage(
        grabbed, destination, KWin::RectF(target));
    if (!destinationManaged) {
        KWin::workspace()->raiseWindow(client);
        KWin::workspace()->activateWindow(client, true);
    }
    qInfo() << "Kadunce" << Revision << "handed card"
            << grabbed->caption() << "to" << destination->name()
            << "destination Bento" << destinationManaged;
    return true;
}

int CardStageController::cardStackCandidate() const
{
    if (!m_cardGrabActive || !m_cardGrabMoved
        || !m_cardLine.selectedIsStandalone() || m_cardLine.count() < 2) {
        return 0;
    }
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!tablet) {
        return 0;
    }
    const KWin::RectF work = KWin::effects->clientArea(
        KWin::MaximizeArea, tablet);
    const CardLineLayout layout = makeCardLineLayout(
        work.x(), work.y(), work.width(), work.height());
    if (std::abs(m_cardGrabOffset) > layout.cards[1].width * 0.48) {
        return 0;
    }
    return m_cardLine.detachedNeighborhood(m_cardGrabPageOffset)[1];
}

void CardStageController::setCardStackPreview(int destinationId)
{
    if (destinationId == 0 || destinationId != cardStackCandidate()) {
        return;
    }
    const double current = stackPreviewBlend();
    m_cardStackPreviewTarget = destinationId;
    m_cardStackInsertionIndex = m_cardLine.stackSizeForId(destinationId);
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
        || m_cardStackPreviewTarget == 0 || direction == 0) {
        return false;
    }
    const int destinationSize =
        m_cardLine.stackSizeForId(m_cardStackPreviewTarget);
    const int next = std::clamp(
        m_cardStackInsertionIndex + (direction < 0 ? -1 : 1),
        0, destinationSize);
    if (next == m_cardStackInsertionIndex) {
        return false;
    }
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
    const int selectedId = m_cardLine.selectedId();
    for (int index = 0; index < m_liveCards.size(); ++index) {
        KWin::EffectWindow *window = m_liveCards.at(index).data();
        if (window && !window->isDeleted()) {
            KWin::effects->setElevatedWindow(
                window, m_presentation == CardPresentation::CardLine
                    && index + 1 == selectedId
                    && m_cardLine.stackSizeForId(selectedId) > 1);
        }
    }
    syncSelectedStackingOrder();
}

void CardStageController::syncSelectedStackingOrder()
{
    if (!m_active || m_presentation != CardPresentation::CardLine
        || m_cardGrabActive) {
        return;
    }
    const std::vector<int> paintOrder =
        m_cardLine.stackPaintOrderForId(m_cardLine.selectedId());
    if (paintOrder.size() <= 1) {
        return;
    }
    for (const int cardId : paintOrder) {
        const int index = cardId - 1;
        if (index < 0 || index >= m_liveCards.size()) {
            continue;
        }
        KWin::EffectWindow *window = m_liveCards.at(index).data();
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
    if (m_active) {
        finishCardGrab(false);
        if (m_presentation == CardPresentation::CardLine) {
            if (!enterActive()) {
                return;
            }
        } else {
            restoreActiveSnapshot();
            m_presentation = CardPresentation::CardLine;
        }
        syncSelectedElevation();
        KWin::effects->addRepaintFull();
        qInfo() << "Kadunce" << Revision << "changed to"
                << (m_presentation == CardPresentation::Active
                        ? "Active" : "Card Line");
        return;
    }

    rebuildLiveCards();
    if (m_liveCards.isEmpty()) {
        qWarning() << "Kadunce" << Revision
                   << "has no eligible live window on the tablet";
        return;
    }
    m_active = true;
    m_presentation = CardPresentation::CardLine;
    m_host->setPagingShortcutsForCardStage(true);
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "activated with"
            << m_liveCards.size() << "live tablet cards; selected"
            << m_cardLine.selectedIndex() + 1;
}

void CardStageController::release()
{
    clearCardTransition();
    m_activeSettleTimer.stop();
    m_activeSettleRemaining = 0;
    if (!m_active) {
        return;
    }
    finishCardGrab(false);
    endLauncherGuest();
    const bool wasActive = m_presentation == CardPresentation::Active;
    const QPointer<KWin::EffectWindow> releasedWindow = selectedWindow();
    // Stop filtering the scene before fullscreen restoration changes layers,
    // activation or geometry. Those operations can synchronously reenter KWin.
    m_active = false;
    m_presentation = CardPresentation::CardLine;
    for (const QPointer<KWin::EffectWindow> &window :
         std::as_const(m_liveCards)) {
        if (window && !window->isDeleted()) {
            m_host->unredirectForCardStage(window);
            KWin::effects->setElevatedWindow(window, false);
        }
    }
    if (wasActive) {
        restoreActiveSnapshot();
    }
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
    m_liveCards.clear();
    m_originalCardStackingOrder.clear();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "released";
}

void CardStageController::pageHorizontal(int delta)
{
    if (!m_active) {
        return;
    }
    if (m_arrivalWindow) clearCardTransition();
    finishCardGrab(false);
    const bool wasActive = m_presentation == CardPresentation::Active;
    if (!wasActive && m_cardLine.count() == 2 && !m_launcherGuestActive) {
        if (delta == 0 || (delta < 0 ? -1 : 1) != m_cardLine.pairNeighborSide()) return;
        captureCardTransition();
    }
    const bool activeStack = wasActive
        && m_cardLine.stackSizeForId(m_cardLine.selectedId()) > 1;
    if (wasActive) {
        restoreActiveSnapshot();
    }
    if (activeStack) {
        m_cardLine.pageStack(delta);
    } else {
        m_cardLine.page(delta);
    }
    if (wasActive && !enterActive()) {
        m_presentation = CardPresentation::CardLine;
    }
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce horizontal navigation selected"
            << m_cardLine.selectedId() << "of" << m_cardLine.count();
}

bool CardStageController::beginLauncherGuest()
{
    if (!m_active || m_presentation != CardPresentation::CardLine
        || m_liveCards.isEmpty() || m_cardGrabActive) {
        return false;
    }
    captureCardTransition();
    m_arrivalTimer.stop();
    m_arrivalWindow.clear();
    m_arrivalExpanding = false;
    m_launcherGuestGroupCount = m_cardLine.count();
    m_launcherGuestArrival = false;
    m_launcherGuestPrimarySide = m_cardLine.count() == 2
        ? -m_cardLine.pairNeighborSide() : 1;
    m_launcherGuestPrimaryWindow = selectedWindow();
    m_launcherGuestSecondaryWindow = m_cardLine.count() > 1
        ? m_liveCards.value(m_cardLine.idAtOffset(-1) - 1) : nullptr;
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
    const CardLineLayout layout = makeCardLineLayout(
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
    m_launcherGuestPendingPage = m_cardLine.count() > 1
        && incomingSide != m_launcherGuestPrimarySide ? -1 : 0;
    KWin::effects->addRepaintFull();
    return true;
}

void CardStageController::endLauncherGuest()
{
    if (!m_launcherGuestActive && qFuzzyIsNull(m_launcherGuestOffset)) {
        return;
    }
    if (!m_launcherGuestArrival && m_launcherGuestPendingPage != 0) {
        m_cardLine.page(m_launcherGuestPendingPage);
    }
    // Keep the guest's established landing intact, then ease that landing
    // into the larger pair. The shoulder stays on the side it actually used.
    if (!m_launcherGuestArrival && m_cardLine.count() == 2 && m_launcherGuestTransitionTimer.isValid()) {
        m_cardLine.setPairNeighborSide(m_launcherGuestOffset < 0.0 ? -1 : 1);
        if (auto *output = m_host->tabletOutputForCardStage()) {
            const auto work = KWin::effects->clientArea(KWin::MaximizeArea, output);
            if (work.width() > 0 && work.height() > 0) {
                m_previewOrigins.clear();
                for (const auto &window : std::as_const(m_liveCards)) {
                    if (!window || window->isDeleted()) continue;
                    const int slot = m_cardLine.sameStack(liveCardIndex(window) + 1,
                        m_cardLine.selectedId()) ? 0 : m_cardLine.pairNeighborSide();
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
    if (!m_active || m_presentation != CardPresentation::CardLine
        || delta == 0
        || m_cardLine.stackSizeForId(m_cardLine.selectedId()) <= 1) {
        return;
    }
    finishCardGrab(false);
    m_cardLine.pageStack(delta);
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce stack selected member"
            << m_cardLine.selectedId() << "position"
            << m_cardLine.stackActivePositionForId(m_cardLine.selectedId()) + 1
            << "of" << m_cardLine.stackSizeForId(m_cardLine.selectedId());
}

void CardStageController::rebuildLiveCards()
{
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    m_liveCards.clear();
    if (!tablet) {
        return;
    }
    KWin::EffectWindow *active = KWin::effects->activeWindow();
    int activeIndex = -1;
    const QList<KWin::EffectWindow *> windows = KWin::effects->stackingOrder();
    for (KWin::EffectWindow *window : windows) {
        if (!m_host->isManagedWindowForCardStage(window)
            || window->screen() != tablet) {
            continue;
        }
        if (window == active) {
            activeIndex = m_liveCards.size();
        }
        m_host->connectManagedWindowForCardStage(window);
        m_liveCards.append(window);
    }
    if (!m_liveCards.isEmpty()) {
        m_originalCardStackingOrder = m_liveCards;
        m_cardLine = CardLineModel(m_liveCards.size());
        m_cardLine.selectIndex(activeIndex >= 0 ? activeIndex
                                               : m_liveCards.size() - 1);
    }
}

bool CardStageController::enterActive()
{
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
         std::as_const(m_liveCards)) {
        if (window && !window->isDeleted()) {
            m_host->unredirectForCardStage(window);
        }
    }
    m_activeRestore = ActiveRestoreSnapshot{
        .window = effectWindow,
        .geometry = effectWindow->frameGeometry(),
        .floatingGeometry = client->geometryRestore(),
        .fullscreenRestoreGeometry = client->fullscreenGeometryRestore(),
        .quickTileMode = client->quickTileMode(),
        .maximizeMode = client->maximizeMode(),
        .fullScreen = client->isFullScreen(),
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
    m_activeSettleTimer.start();
    KWin::workspace()->raiseWindow(client);
    KWin::workspace()->activateWindow(client, true);
    qInfo() << "Kadunce" << Revision
            << "entered interactive Active with" << effectWindow->caption();
    return true;
}

void CardStageController::restoreActiveSnapshot()
{
    m_activeSettleTimer.stop();
    m_activeSettleRemaining = 0;
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    if (!m_activeRestore.valid || !m_activeRestore.window
        || !m_activeRestore.window->window()) {
        m_activeRestore = ActiveRestoreSnapshot{};
        return;
    }
    const ActiveRestoreSnapshot snapshot = m_activeRestore;
    m_activeRestore = ActiveRestoreSnapshot{};
    KWin::Window *client = snapshot.window->window();
    restoreWindowState(client, snapshot, snapshot.geometry, true);
    qInfo() << "Kadunce" << Revision << "restored"
            << snapshot.window->caption() << "to" << snapshot.geometry;
}

void CardStageController::admitTransferredWindowToTablet(
    KWin::EffectWindow *window)
{
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!tablet || !window || !window->window()) {
        return;
    }
    KWin::Window *client = window->window();
    client->sendToOutput(tablet);
    client->moveResize(KWin::RectF(activeTarget(tablet)));
    if (!m_active) {
        KWin::workspace()->raiseWindow(client);
        KWin::workspace()->activateWindow(client, true);
        return;
    }
    finishCardGrab(false);
    if (m_presentation == CardPresentation::Active) {
        restoreActiveSnapshot();
    }
    m_presentation = CardPresentation::CardLine;
    if (liveCardIndex(window) < 0) {
        m_liveCards.append(window);
        m_originalCardStackingOrder.append(window);
        m_cardLine.appendCard();
    }
    syncSelectedElevation();
    if (!enterActive()) {
        m_presentation = CardPresentation::CardLine;
    }
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
    if (m_presentation != CardPresentation::CardLine) {
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
    for (int i = 0; i < m_cardLine.count() && !m_cardLine.sameStack(id, m_cardLine.selectedId()); ++i)
        m_cardLine.page(1);
    for (int i = 0; i < m_cardLine.stackSizeForId(id) && m_cardLine.selectedId() != id; ++i)
        m_cardLine.pageStack(1);
    if (replacesGuest && m_cardLine.count() == 2)
        m_cardLine.setPairNeighborSide(m_launcherGuestPrimarySide);
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

    const bool animateArrival = m_presentation == CardPresentation::CardLine
        && !m_launcherGuestActive && !m_cardGrabActive;
    if (animateArrival) captureCardTransition();
    finishCardGrab(false);
    if (m_presentation == CardPresentation::Active) {
        restoreActiveSnapshot();
    }
    m_presentation = CardPresentation::CardLine;
    int previousSelection = m_cardLine.selectedIndex();
    m_liveCards.append(window);
    m_originalCardStackingOrder.append(window);
    const int admittedId = animateArrival || m_launcherGuestActive
        ? m_cardLine.appendCenteredCard() : m_cardLine.appendCard();
    m_host->connectManagedWindowForCardStage(window);
    if (m_launcherGuestActive) {
        if (m_cardLine.selectedIndex() <= previousSelection) ++previousSelection;
        m_cardLine.selectIndex(previousSelection);
        syncSelectedElevation();
        KWin::effects->addRepaintFull();
        return true; // Admission must not bypass the guest's matching handoff.
    }
    if (animateArrival) {
        startArrivalTimer(window);
        syncSelectedElevation();
        KWin::effects->addRepaintFull();
        return true;
    }
    syncSelectedElevation();
    if (!enterActive()) {
        m_presentation = CardPresentation::CardLine;
    }
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "admitted new card"
            << admittedId << window->caption() << "as Active";
    return true;
}

void CardStageController::handleWindowClosed(KWin::EffectWindow *window)
{
    m_originalCardStackingOrder.removeAll(window);
    const int closedIndex = liveCardIndex(window);
    if (!m_active || closedIndex < 0) {
        return;
    }

    if (m_cardLine.count() == 3 && m_cardLine.stackSizeForId(closedIndex + 1) == 1)
        captureCardTransition();
    finishCardGrab(false);
    const bool closedActive = m_activeRestore.window == window;
    if (closedActive) {
        m_activeRestore = ActiveRestoreSnapshot{};
    }
    m_liveCards.removeAt(closedIndex);
    if (m_liveCards.isEmpty()) {
        release();
        return;
    }
    if (!m_cardLine.removeCard(closedIndex + 1)) {
        qWarning() << "Kadunce" << Revision
                   << "could not remove closed card in place; rebuilding";
        if (m_presentation == CardPresentation::Active) {
            restoreActiveSnapshot();
        }
        m_presentation = CardPresentation::CardLine;
        rebuildLiveCards();
    } else if (closedActive) {
        m_presentation = CardPresentation::CardLine;
    }
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
}

void CardStageController::handleWindowActivated(KWin::EffectWindow *window)
{
    if (!m_active || !window || m_cardGrabActive) {
        return;
    }
    if (window == m_arrivalWindow) return; // Let its center/expand sequence finish.
    if (m_arrivalWindow) clearCardTransition(); // An explicit different activation wins.
    const int targetIndex = liveCardIndex(window);
    if (targetIndex < 0) {
        return;
    }
    const int targetId = targetIndex + 1;
    if (m_presentation == CardPresentation::Active
        && m_cardLine.selectedId() == targetId) {
        return;
    }

    finishCardGrab(false);
    if (m_presentation == CardPresentation::Active) {
        restoreActiveSnapshot();
    }
    m_presentation = CardPresentation::CardLine;

    // Select the requested card through the existing stack model so a task
    // manager click can address both standalone cards and a specific member
    // of a stack without rewriting the frozen card-line core.
    for (int step = 0;
         step < m_cardLine.count()
         && !m_cardLine.sameStack(targetId, m_cardLine.selectedId());
         ++step) {
        m_cardLine.page(1);
    }
    const int stackSize = m_cardLine.stackSizeForId(targetId);
    for (int step = 0;
         step < stackSize && m_cardLine.selectedId() != targetId;
         ++step) {
        m_cardLine.pageStack(1);
    }

    syncSelectedElevation();
    if (!enterActive()) {
        m_presentation = CardPresentation::CardLine;
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
    if (client->isInteractiveMove() || client->isInteractiveResize() || client->isFullScreen()
        || client->maximizeMode() != KWin::MaximizeRestore
        || client->quickTileMode() != KWin::QuickTileMode{}) {
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
