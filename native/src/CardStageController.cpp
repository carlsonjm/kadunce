/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CardStageController.h"
#include "CardLineLayout.h"

#include <core/output.h>
#include <effect/effecthandler.h>
#include <window.h>
#include <workspace.h>

#include <QDebug>
#include <QEasingCurve>

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
}

CardStageController::CardStageController(CardStageHost *host)
    : m_host(host)
{
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
    if (m_liveCards.size() == 2) {
        return 1;
    }
    if (m_liveCards.size() >= 3) {
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
    return (m_cardStackPreviewTimer.isValid()
            && m_cardStackPreviewTimer.elapsed()
                < CardStackTransitionDuration)
        || (m_cardStackInsertionTimer.isValid()
            && m_cardStackInsertionTimer.elapsed()
                < CardStackTransitionDuration);
}

KWin::Rect CardStageController::cardTargetForSlot(
    KWin::LogicalOutput *output, int slot) const
{
    const KWin::RectF work = KWin::effects->clientArea(
        KWin::MaximizeArea, output);
    const CardLineLayout layout = makeCardLineLayout(
        work.x(), work.y(), work.width(), work.height());
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
    const CardRect target = makeReservedCardTarget(layout, slot, envelope);
    return KWin::Rect(qRound(target.x), qRound(target.y),
                      qRound(target.width), qRound(target.height));
}

KWin::Rect CardStageController::activeTarget(KWin::LogicalOutput *output) const
{
    const KWin::RectF work = KWin::effects->clientArea(
        KWin::MaximizeArea, output);
    const CardRect target = makeActiveTarget(
        work.x(), work.y(), work.width(), work.height());
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
    const KWin::Rect center = cardTargetForSlot(tablet, 0);
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
    if (!m_active) {
        return;
    }
    finishCardGrab(false);
    if (m_presentation == CardPresentation::Active) {
        restoreActiveSnapshot();
    }
    for (const QPointer<KWin::EffectWindow> &window :
         std::as_const(m_liveCards)) {
        if (window && !window->isDeleted()) {
            m_host->unredirectForCardStage(window);
            KWin::effects->setElevatedWindow(window, false);
        }
    }
    m_active = false;
    m_presentation = CardPresentation::CardLine;
    restoreOriginalStackingOrder();
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
    finishCardGrab(false);
    const bool wasActive = m_presentation == CardPresentation::Active;
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
    KWin::workspace()->raiseWindow(client);
    KWin::workspace()->activateWindow(client, true);
    qInfo() << "Kadunce" << Revision
            << "entered interactive Active with" << effectWindow->caption();
    return true;
}

void CardStageController::restoreActiveSnapshot()
{
    if (!m_activeRestore.valid || !m_activeRestore.window
        || !m_activeRestore.window->window()) {
        m_activeRestore = ActiveRestoreSnapshot{};
        return;
    }
    const ActiveRestoreSnapshot snapshot = m_activeRestore;
    m_activeRestore = ActiveRestoreSnapshot{};
    KWin::Window *client = snapshot.window->window();
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
    client->moveResize(snapshot.geometry);
    if (snapshot.quickTileMode != KWin::QuickTileMode{}) {
        client->setQuickTileMode(snapshot.quickTileMode,
                                 snapshot.geometry.center());
    } else if (snapshot.maximizeMode != KWin::MaximizeRestore) {
        client->maximize(snapshot.maximizeMode, snapshot.geometry);
    }
    if (snapshot.fullScreen) {
        client->setFullScreen(true);
    }
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

bool CardStageController::handleWindowAdded(KWin::EffectWindow *window)
{
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || !window || !tablet || !window->isNormalWindow()
        || !m_host->isManagedWindowForCardStage(window)
        || window->screen() != tablet || liveCardIndex(window) >= 0) {
        return false;
    }

    finishCardGrab(false);
    if (m_presentation == CardPresentation::Active) {
        restoreActiveSnapshot();
    }
    m_presentation = CardPresentation::CardLine;
    m_liveCards.append(window);
    m_originalCardStackingOrder.append(window);
    const int admittedId = m_cardLine.appendCard();
    m_host->connectManagedWindowForCardStage(window);
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
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!tablet) {
        return;
    }
    const KWin::Rect target = activeTarget(tablet);
    if (window->window()->isInteractiveResize()) {
        window->window()->cancelInteractiveMoveResize();
    }
    if (window->frameGeometry().toRect() != target) {
        window->window()->moveResize(KWin::RectF(target));
        qInfo() << "Kadunce" << Revision
                << "rejected an Active resize outside" << target;
    }
}

} // namespace Kadunce
