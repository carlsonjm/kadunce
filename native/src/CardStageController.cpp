/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CardStageController.h"
#include "HeldCardGeometry.h"
#include "NeighborStackPose.h"
#include "RowPageMotion.h"
#include "StackBrowseMotion.h"
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

QList<QPointer<KWin::EffectWindow>> CardStageController::preparationNeighbors() const
{
    QList<QPointer<KWin::EffectWindow>> result;
    if (!m_active || m_workspace.count() == 0
        || m_presentation != CardPresentation::CardLine || m_launcherGuestActive) return result;
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
        && m_presentation == CardPresentation::CardLine) {
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
    return m_workspace.detachedNeighborhood(m_cardGrabPageOffset)[1];
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
    const CardLineLayout layout = focusedPair
        ? makeFocusedPairLayout(work.x(), work.y(), work.width(), work.height())
        : makeCardLineLayout(work.x(), work.y(), work.width(), work.height());
    CardStackEnvelope envelope{0.0, 0.0};
    if (m_presentation == CardPresentation::CardLine) {
        if (!m_cardGrabActive) {
            const int selectedId = m_workspace.selectedId();
            const int memberCount = m_workspace.stackSizeForId(selectedId);
            if (memberCount > 1) {
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
    if (!output || m_presentation != CardPresentation::CardLine
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
        || m_presentation != CardPresentation::CardLine
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
    auto *tablet = m_host->tabletOutputForCardStage();
    if (!tablet) return {0.0, 0.0, 0.0, true};
    if (m_cardGrabActive && window == selectedWindow()) {
        const double t = m_cardGrabScaleTimer.isValid()
            ? heldPickupProgress(m_cardGrabScaleTimer.elapsed()) : 1.0;
        return {0.0, 0.0, m_cardGrabRotation * (1.0 - t), true};
    }
    const auto &cardLine = m_workspace.model();
    const int cardId = m_workspace.windows().indexOf(const_cast<KWin::EffectWindow *>(window)) + 1;
    const double previewBlend = stackPreviewBlend();
    const int memberCount = cardLine.stackSizeForId(cardId);
    const int memberIndex = cardLine.stackPositionForId(cardId);
    const int activeIndex = cardLine.stackActivePositionForId(cardId);
    CardStackPose pose{0.0, 0.0, 0.0, true};
    const bool previewDestination = cardGrabActive()
        && stackPreviewTarget() != 0
        && cardLine.sameStack(
            cardId, stackPreviewTarget());
    const int browseTarget = stackBrowseTarget();
    const bool browseDestination = cardGrabActive()
        && browseTarget != 0
        && cardLine.sameStack(cardId, browseTarget);
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
                   && cardLine.sameStack(
                       cardId, cardLine.selectedId())) {
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
    const int selectedId = m_workspace.selectedId();
    const int memberCount = m_workspace.stackSizeForId(selectedId);
    if (!tablet || memberCount <= 1) {
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
        && m_presentation == CardPresentation::CardLine;
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
            m_presentation = CardPresentation::CardLine;
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
    if (!m_active || m_presentation != CardPresentation::CardLine
        || m_cardGrabActive || !selectedWindow()
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
    qInfo() << "Kadunce" << Revision << "lifted Card Line card"
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
        const CardLineLayout layout = makeCardLineLayout(
            work.x(), work.y(), work.width(), work.height());
        const double pitch = layout.cards[1].width + layout.gutter;
        if (m_cardGrabOffset.x() <= -pitch * 0.82) {
            movement = -1;
        } else if (m_cardGrabOffset.x() >= pitch * 0.82) {
            movement = 1;
        }
    }

    const bool restoreDetached = m_workspace.hasDetachedMember()
        && (!commit || (!stacked && movement == 0));
    if (restoreDetached) {
        m_workspace.restoreDetachedMember();
    } else {
        m_workspace.commitDetachedMember();
    }
    if (!restoreDetached && !stacked && movement != 0) {
        m_workspace.moveSelected(movement);
    }
    resetCardGrabState(grabbed, stacked);
    qInfo() << "Kadunce" << Revision << "released Card Line card"
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
        || m_presentation != CardPresentation::CardLine) {
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
                m_presentation = CardPresentation::CardLine;
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
    const CardLineLayout layout = makeCardLineLayout(
        work.x(), work.y(), work.width(), work.height());
    if (std::abs(m_cardGrabOffset.x()) > layout.cards[1].width * 0.48
        || std::abs(m_cardGrabOffset.y()) > layout.cards[1].height * 0.48
        || !m_cardGrabDestinationOutput.isEmpty()) {
        return 0;
    }
    return m_workspace.detachedNeighborhood(m_cardGrabPageOffset)[1];
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
    for (int index = 0; index < m_workspace.windows().size(); ++index) {
        KWin::EffectWindow *window = m_workspace.windows().at(index).data();
        if (window && !window->isDeleted()) {
            KWin::effects->setElevatedWindow(
                window, m_presentation == CardPresentation::CardLine
                    && index + 1 == selectedId
                    && m_workspace.stackSizeForId(selectedId) > 1);
        }
    }
    syncSelectedStackingOrder();
}

void CardStageController::syncSelectedStackingOrder()
{
    if (!m_active || m_presentation != CardPresentation::CardLine) {
        return;
    }
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
        if (m_presentation == CardPresentation::CardLine) {
            if (!enterActive()) {
                return;
            }
        } else {
            parkActiveSnapshot();
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
    if (m_workspace.windows().isEmpty()) {
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
            << m_workspace.windows().size() << "live tablet cards; selected"
            << m_workspace.selectedIndex() + 1;
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
    m_presentation = CardPresentation::CardLine;
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
    m_originalCardStackingOrder.clear();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "released";
}

void CardStageController::pageHorizontal(int delta)
{
    if (!m_active) {
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
        m_presentation = CardPresentation::CardLine;
    }
    if (!wasActive) anchorRowTransition();
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce horizontal navigation selected"
            << m_workspace.selectedId() << "of" << m_workspace.count();
}

bool CardStageController::beginLauncherGuest()
{
    if (!m_active || m_presentation != CardPresentation::CardLine
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
    if (!m_active || m_presentation != CardPresentation::CardLine
        || delta == 0
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
    m_workspace.reset(admitted, activeIndex >= 0 ? activeIndex : admitted.size() - 1);
    if (!admitted.isEmpty()) m_originalCardStackingOrder = admitted;
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
    // All entry paths must retire Card Line's temporary compositor elevation.
    // Active is a native window; panel popups must retain their normal layers.
    syncSelectedElevation();
    m_activeSettleTimer.start();
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
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    parkActiveSnapshot();
    const auto snapshots = std::exchange(m_parkedRestores, {});
    for (const auto &snapshot : snapshots) {
        if (!snapshot.valid || !snapshot.window || snapshot.window->isDeleted()
            || !snapshot.window->window()) continue;
        restoreWindowState(snapshot.window->window(), snapshot, snapshot.geometry, true);
    }
}

void CardStageController::parkActiveSnapshot()
{
    m_activeSettleTimer.stop();
    if (m_activeRestore.valid && m_activeRestore.window) {
        const auto window = m_activeRestore.window;
        m_parkedRestores.removeIf([&](const auto &s) { return !s.window || s.window == window; });
        m_parkedRestores.append(m_activeRestore);
    }
    m_activeRestore = {};
}

void CardStageController::forgetManagedRestore(KWin::EffectWindow *window)
{
    if (m_activeRestore.window == window) m_activeRestore = {};
    m_parkedRestores.removeIf([window](const auto &s) { return !s.window || s.window == window; });
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
        saved->quickTileMode, saved->fullScreen, false};
}

bool CardStageController::admitTransferredWindowToTablet(
    KWin::EffectWindow *window, const std::function<bool()> &commitSource,
    const QRectF &carriedOrigin, const NativeMoveSnapshot *restore)
{
    QPointer<KWin::LogicalOutput> tablet = m_host->tabletOutputForCardStage();
    if (!tablet || !window || window->isDeleted() || !window->window()
        || (m_active && (!window->isNormalWindow()
            || !m_host->isManagedWindowForCardStage(window)))) {
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
        .valid = true,
    };
    const KWin::RectF target(activeTarget(tablet));
    const auto ticket = m_transferGuard.issue();
    if (!target.isValid() || !KWin::effects->screens().contains(tablet.data())
        || window->isUserMove() || window->isUserResize()) return false;
    const bool newMember = m_active && liveCardIndex(window) < 0;
    const bool animateArrival = m_presentation == CardPresentation::CardLine
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
        m_presentation = CardPresentation::CardLine;
        parkActiveSnapshot();
        if (!valid()) return true;
    }
    if (m_active && !managedRestore(arrival)) m_parkedRestores.append(incoming);
    client->sendToOutput(tablet);
    if (!valid()) return true;
    client->moveResize(target);
    if (!valid()) return true;
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
    // from the released face, not an invented off-screen Card Line slot. Do
    // this after admission cleanup, which can cancel the source carry.
    if (valid() && animateArrival && m_presentation == CardPresentation::CardLine
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

    const bool animateArrival = m_presentation == CardPresentation::CardLine
        && !m_launcherGuestActive && !m_cardGrabActive;
    m_host->cancelInputForCardStage();
    if (animateArrival) captureCardTransition();
    finishCardGrab(false);
    if (m_presentation == CardPresentation::Active) {
        parkActiveSnapshot();
    }
    m_presentation = CardPresentation::CardLine;
    int previousSelection = m_workspace.selectedIndex();
    m_workspace.append(window, animateArrival || m_launcherGuestActive);
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
        m_presentation = CardPresentation::CardLine;
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
        && m_workspace.selectedId() == targetId) {
        return;
    }

    m_host->cancelInputForCardStage();
    finishCardGrab(false);
    if (m_presentation == CardPresentation::Active) {
        parkActiveSnapshot();
    }
    m_presentation = CardPresentation::CardLine;

    // Select the requested card through the existing stack model so a task
    // manager click can address both standalone cards and a specific member
    // of a stack without rewriting the frozen card-line core.
    for (int step = 0;
         step < m_workspace.count()
         && !m_workspace.sameStack(targetId, m_workspace.selectedId());
         ++step) {
        m_workspace.page(1);
    }
    const int stackSize = m_workspace.stackSizeForId(targetId);
    for (int step = 0;
         step < stackSize && m_workspace.selectedId() != targetId;
         ++step) {
        m_workspace.pageStack(1);
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
