/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Where each card is drawn: row slots, carry geometry, transitions, poses and
// the Active place's target.
#include "CardStageInternal.h"

namespace Kadunce
{

namespace
{
// A Stack parting for, paging under or closing after a card held in it.
constexpr int HeldStackStepDuration = 360;
constexpr int StackOutlineFadeDuration = 180;
constexpr int LauncherGuestTransitionDuration = 220;
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
        return QStringLiteral("join");
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
    if (item == m_carry.riseIndex) {
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
    return m_row.moving() || activeSlideRunning() || m_lift.phase == Lift::Phase::Return
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

bool CardStageController::motionIsInstant()
{
    return motionInstant(KWin::effects ? KWin::effects->animationTimeFactor() : 1.0);
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
    m_growingChosen = false;
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
    // Both cards of the Active card's strip are drawn while it moves.
    if (activeSlideShift(window)) return 0;
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
        if (window == m_arrivalWindow) target = activeTargetFor(output, window);
        else target.translate(slot * output->geometry().width() / 2, 0);
    }
    if (m_openProgress && m_presentation == CardPresentation::Spread && !m_arrivalExpanding) {
        // Opening under three fingers: the Active card shrinks from where it
        // stood into the row, and its neighbours slide in from the sides.
        const double p = spreadOpenProgress();
        const bool fromActive = window == selectedWindow();
        const QRectF to(target.x(), target.y(), target.width(), target.height());
        QRectF from = fromActive ? QRectF(activeTargetFor(output, window)) : to;
        if (!fromActive) from.translate(slot * output->geometry().width() / 2.0, 0);
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
        closed.visible = closed.visible && closedDepth <= 3;
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
                pose.visible = pose.visible && (closedDepth == 0 || cardTuning().stackBehindShown);
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

KWin::Rect CardStageController::activeTargetFor(KWin::LogicalOutput *output,
    const KWin::EffectWindow *window) const
{
    const KWin::Rect whole = activeTarget(output);
    const auto share = m_activeWidths.constFind(window);
    if (share == m_activeWidths.cend() || !window || !window->window()) return whole;
    // A card in a Stack stands at the Stack's size, the whole Active width,
    // whatever width the wheel gave it alone.
    if (stackFrontToBack(window).size() > 1) return whole;
    return narrowedActive(whole, *share, window->window()->minSize().width());
}

KWin::Rect CardStageController::narrowedActive(const KWin::Rect &whole, double share,
                                               double minimumWidth)
{
    // Never narrower than the app allows.
    const int width = std::min(whole.width(), std::max(qRound(whole.width() * share),
                                                       int(std::ceil(minimumWidth))));
    return KWin::Rect(whole.x() + (whole.width() - width) / 2, whole.y(), width, whole.height());
}

void CardStageController::setActiveWidth(const KWin::EffectWindow *window, double share)
{
    if (share >= 1.0 || share <= 0.0) m_activeWidths.remove(window);
    else m_activeWidths.insert(window, share);
}

bool CardStageController::refitActive(KWin::EffectWindow *window)
{
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || m_presentation != CardPresentation::Active || !m_activeRestore.valid
        || !window || window->isDeleted() || m_activeRestore.window != window || !window->window()
        || !tablet) return false;
    {
        QScopedValueRollback<bool> applying(m_applyingWindowState, true);
        window->window()->moveResize(KWin::RectF(activePlacement(tablet)));
    }
    m_activeSettleRemaining = std::max(m_activeSettleRemaining, 2);
    KWin::effects->addRepaintFull();
    return true;
}

bool CardStageController::holdActivePlace(KWin::EffectWindow *window, std::optional<KWin::Rect> place)
{
    if (!place) {
        if (!m_heldActivePlace || m_heldActiveWindow != window) return false;
        m_heldActiveWindow.clear();
        m_heldActivePlace.reset();
        return true;
    }
    m_heldActiveWindow = window;
    m_heldActivePlace = place;
    return true;
}

KWin::Rect CardStageController::activePlacement(
    KWin::LogicalOutput *output) const
{
    if (m_heldActivePlace && m_heldActiveWindow && m_heldActiveWindow.data() == m_activeRestore.window.data())
        return *m_heldActivePlace;
    if (!m_keyboardRoom || !m_keyboardRoom->window
        || m_keyboardRoom->window != m_activeRestore.window) {
        return activeTargetFor(output, m_activeRestore.window);
    }
    KWin::RectF placement = m_keyboardRoom->base;
    if (m_host->inputPanelTopForCardStage(output).has_value()
        && m_host->keyboardTypesIntoForCardStage(m_keyboardRoom->window)) {
        placement.moveTop(m_keyboardRoom->top);
        placement.setHeight(m_keyboardRoom->height);
    }
    return placement.toRect();
}

} // namespace Kadunce
