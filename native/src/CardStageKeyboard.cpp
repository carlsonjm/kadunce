/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The room the card stage keeps for the on-screen keyboard.
#include "CardStageInternal.h"

#include <effect/effect.h>

namespace Kadunce
{

void CardStageController::refreshKeyboardRoom()
{
    m_keyboardRoomTimer.start();
}

void CardStageController::putBackKeyboardRoom()
{
    m_keyboardRoomTimer.stop();
    m_keyboardRoomRelease.stop();
    m_keyboardRoomGrace.stop();
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
    qInfo() << "Kadunce keyboard room returned" << logNameOf(room->window)
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
    // Its top stays below the Context Panel. A client that cannot be as
    // short as the room keeps its least height, its foot under the keys.
    m_keyboardRoom->top = base.y();
    const KWin::Rect target = KWin::RectF(base.x(), m_keyboardRoom->top, base.width(), height).toRect();
    // Measured against what was asked for, not the frame: a client still
    // drawing its last size, or one that trims itself to whole rows as a
    // terminal does, is not asked again.
    if (client->moveResizeGeometry().toRect() == target) return;
    m_keyboardRoomDraw = KeyboardRoomDraw{m_keyboardRoom->window,
        client->frameGeometry().height(), double(target.height()), {}};
    m_keyboardRoomDraw->since.start();
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    client->moveResize(KWin::RectF(target));
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce keyboard room" << logNameOf(m_keyboardRoom->window)
            << "height" << height << "target" << target;
}

void CardStageController::noteKeyboardRoomDrawn(KWin::EffectWindow *window)
{
    // A frame that comes closer to the size asked for is the client's answer.
    // A terminal trims itself to whole rows, so it may never match exactly.
    if (!m_keyboardRoomDraw || m_keyboardRoomDraw->window != window || !window->window()) return;
    constexpr qint64 Unanswered = 1000;
    const qint64 elapsed = m_keyboardRoomDraw->since.elapsed();
    const double height = window->window()->frameGeometry().height();
    const double asked = m_keyboardRoomDraw->to;
    if (elapsed < Unanswered && std::abs(height - asked) >= std::abs(m_keyboardRoomDraw->from - asked)) return;
    m_keyboardRoomDraw.reset();
    if (elapsed >= Unanswered) return;
    const QString application = window->windowClass();
    const auto kept = m_keyboardRoomDrawTimes.constFind(application);
    m_keyboardRoomDrawTimes.insert(application, keyboardRoomDrawTime(
        kept == m_keyboardRoomDrawTimes.cend() ? std::nullopt : std::optional<int>(*kept), int(elapsed)));
}

void CardStageController::keyboardHeading(double top, int durationMs)
{
    m_keyboardHeadingTop = top;
    m_keyboardHeadingSince.start();
    m_keyboardHeadingDuration = durationMs;
    m_keyboardHeadingTimer.stop();
    m_keyboardHeadingPending = false;
    auto *client = keyboardRoomClient();
    // Keys still below the screen's edge have made no room yet, so the card
    // has none to give up. They are rising, and the room they take is asked
    // for on the way, as below.
    if (!client) {
        m_keyboardHeadingPending = true;
        return;
    }
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

namespace
{
// Whether a panel still stands on the display at an edge the card's place
// grew toward: its window stays while it is only out of view.
bool panelStandsWhereRoomCameBack(const KWin::LogicalOutput *output, const KWin::Rect &current, const KWin::Rect &target)
{
    if (!output) return false;
    const QRectF screen(output->geometry());
    for (auto *dock : KWin::effects->stackingOrder()) {
        if (!dock || dock->isDeleted() || !dock->isDock() || !dock->isOnCurrentDesktop()
            || !dock->window() || !dock->window()->isShown()) continue;
        const QRectF panel(dock->frameGeometry());
        if (!screen.intersects(panel)) continue;
        constexpr double Near = 2.0;
        if ((target.y() < current.y() && std::abs(panel.top() - screen.top()) < Near && panel.width() > panel.height())
            || (target.y() + target.height() > current.y() + current.height()
                && std::abs(panel.bottom() - screen.bottom()) < Near && panel.width() > panel.height())
            || (target.x() < current.x() && std::abs(panel.left() - screen.left()) < Near && panel.height() > panel.width())
            || (target.x() + target.width() > current.x() + current.width()
                && std::abs(panel.right() - screen.right()) < Near && panel.height() > panel.width()))
            return true;
    }
    return false;
}
}

void CardStageController::followWorkArea(bool now)
{
    m_workAreaGrowTimer.stop();
    if (!m_active) return;
    // Spread and the neighbours are drawn from the area every frame.
    KWin::effects->addRepaintFull();
    const auto window = m_activeRestore.window;
    auto *tablet = m_host->tabletOutputForCardStage();
    // §3: the cards behind the one presented stood in the Active place the
    // area gave when they were taken. At sign-in that is before the panels
    // have taken their room, so they still reach a panel, which then stays
    // opaque until each card is brought forward. Those that reach past the
    // place the area gives now stand in it; one inside it is left alone, so
    // a panel giving room back moves nothing.
    if (tablet && !m_cardGrabActive
        && (m_presentation == CardPresentation::Active || m_presentation == CardPresentation::Spread)) {
        const KWin::RectF place(activeTarget(tablet));
        QList<QPointer<KWin::EffectWindow>> reaching;
        for (const auto &card : std::as_const(m_workspace.windows())) {
            if (!card || card->isDeleted() || !card->window() || card == window
                || card->screen() != tablet
                || card->isMinimized() || m_bentoProjectionWindows.contains(card)
                || card->window()->isInteractiveMove() || card->window()->isInteractiveResize())
                continue;
            const KWin::RectF placed = card->window()->moveResizeGeometry();
            if (placed.top() < place.top() - 1 || placed.left() < place.left() - 1
                || placed.bottom() > place.bottom() + 1 || placed.right() > place.right() + 1)
                reaching.append(card);
        }
        if (!reaching.isEmpty()) {
            qInfo() << "Kadunce cards behind the Active one follow the work area:" << reaching.size();
            settleCardsInActivePlace(reaching);
        }
    }
    if (m_presentation != CardPresentation::Active || !m_activeRestore.valid
        || !window || window->isDeleted() || !window->window() || !tablet) return;
    auto *client = window->window();
    if (client->isInteractiveMove() || client->isInteractiveResize()
        || client->isFullScreen() || client->maximizeMode() != KWin::MaximizeRestore
        || client->quickTileMode() != KWin::QuickTileMode{}) return;
    const KWin::Rect target = activePlacement(tablet);
    const KWin::Rect current = client->moveResizeGeometry().toRect();
    if (current == target) return;
    const bool grows = target.x() <= current.x() && target.y() <= current.y()
        && target.x() + target.width() >= current.x() + current.width()
        && target.y() + target.height() >= current.y() + current.height();
    // A panel still standing at the edge it gave room back at is only out of
    // view, brought from its edge a moment ago: the card grows with its slide.
    // One that is gone may be coming back, as the shell restarting.
    const bool slidOut = grows && panelStandsWhereRoomCameBack(tablet, current, target);
    if (!now && grows && !slidOut) {
        m_workAreaGrowTimer.start();
        qInfo() << "Kadunce Active card waits before growing into room a panel gave up"
                << logNameOf(window) << "to" << target;
        return;
    }
    if (!grows || slidOut) drawWorkAreaFollow(window, target, grows);
    else m_workAreaFollow.reset();
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    client->moveResize(KWin::RectF(target));
    qInfo() << "Kadunce Active card follows the work area" << logNameOf(window)
            << "to" << target << (m_workAreaFollow ? "drawn with the panel" : "at once");
}

void CardStageController::drawWorkAreaFollow(KWin::EffectWindow *window, const KWin::Rect &target, bool grows)
{
    // A panel's own slide from its edge and back: in
    // over 320 ms easing out, out over 120 ms easing in, at Plasma's speed.
    const int duration = motion(grows ? PanelSlideOutDuration : PanelSlideInDuration);
    if (duration <= 1 || !window->window()) {
        m_workAreaFollow.reset();
        return;
    }
    // A second word mid-slide, as a panel settling its thickness, carries on
    // from where the card is drawn now.
    const auto drawn = workAreaFollowRect(window);
    const QRectF frame(window->frameGeometry());
    WorkAreaFollow follow{window, drawn ? *drawn : frame, QRectF(KWin::RectF(target)), frame.size(), duration,
                          grows ? QEasingCurve::InCubic : QEasingCurve::OutCubic, {}};
    if (drawn && m_workAreaFollow->since.elapsed() < m_workAreaFollow->duration) {
        follow.duration = std::max(60, int(m_workAreaFollow->duration - m_workAreaFollow->since.elapsed()));
        follow.curve = m_workAreaFollow->curve;
    }
    follow.since.start();
    m_workAreaFollow = follow;
    m_workAreaFollowTrace.append(QStringLiteral("to %1,%2 %3x%4 in %5 ms").arg(target.x()).arg(target.y())
        .arg(target.width()).arg(target.height()).arg(follow.duration));
    KWin::effects->addRepaintFull();
}

std::optional<QRectF> CardStageController::workAreaFollowRect(const KWin::EffectWindow *window) const
{
    if (!m_workAreaFollow || m_workAreaFollow->window != window || !window || window->isDeleted()
        || !m_active || m_presentation != CardPresentation::Active) return std::nullopt;
    const auto &follow = *m_workAreaFollow;
    const qint64 elapsed = follow.since.elapsed();
    if (elapsed >= follow.duration) {
        // There, and held there until its client has drawn nearer the size
        // asked for, so the client's own answer never shows as a jump. One
        // that keeps its size, or trims itself as a terminal does, is let go.
        constexpr qint64 Unanswered = 1000;
        const QSizeF frame = QRectF(window->frameGeometry()).size();
        const auto off = [&follow](const QSizeF &size) {
            return std::abs(size.width() - follow.to.width()) + std::abs(size.height() - follow.to.height());
        };
        if (elapsed >= follow.duration + Unanswered || off(frame) < off(follow.asked) || off(follow.asked) < 1.0)
            return std::nullopt;
        return follow.to;
    }
    const double p = QEasingCurve(follow.curve).valueForProgress(double(elapsed) / follow.duration);
    const auto mix = [p](double from, double to) { return from + (to - from) * p; };
    return QRectF(mix(follow.from.x(), follow.to.x()), mix(follow.from.y(), follow.to.y()),
                  mix(follow.from.width(), follow.to.width()), mix(follow.from.height(), follow.to.height()));
}

std::optional<QRectF> CardStageController::applyWorkAreaFollow(const KWin::EffectWindow *window, KWin::WindowPaintData &data) const
{
    const auto place = workAreaFollowRect(window);
    const QRectF frame(window->frameGeometry());
    if (!place || frame.width() <= 0.0 || frame.height() <= 0.0) {
        // One line for the whole slide once it is over: when each frame was
        // drawn, the card's top and bottom in it, and its client's height.
        if (!m_workAreaFollowTrace.isEmpty() && (!m_workAreaFollow || m_workAreaFollow->window == window))
            qInfo().noquote() << "Kadunce Active card drawn with the panel, ms top bottom client:"
                              << std::exchange(m_workAreaFollowTrace, {}).join(QStringLiteral(" | "));
        return std::nullopt;
    }
    // Never squeezed: a client still drawn larger than the place is cut at
    // the place's edges, as a panel sliding over it would cut it, and as the
    // keys do. One drawn smaller is stretched to fill it until it answers.
    data.setXScale(data.xScale() * std::max(1.0, place->width() / frame.width()));
    data.setYScale(data.yScale() * std::max(1.0, place->height() / frame.height()));
    data.translate(place->x() - frame.x(), place->y() - frame.y());
    if (m_workAreaFollowTrace.size() < 400)
        m_workAreaFollowTrace.append(QStringLiteral("%1 %2 %3 %4").arg(m_workAreaFollow->since.elapsed())
            .arg(place->top(), 0, 'f', 1).arg(place->bottom(), 0, 'f', 1).arg(frame.height(), 0, 'f', 0));
    return place;
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
        m_keyboardRoomGrace.stop();
    }
    auto *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || m_presentation != CardPresentation::Active
        || !m_activeRestore.valid || !window || window->isDeleted()
        || !window->window() || !tablet) {
        return;
    }
    auto *client = window->window();
    // §2, A card that sizes itself is given no room: its size is its own.
    if (client->isInteractiveMove() || client->isInteractiveResize()
        || client->isFullScreen() || client->maximizeMode() != KWin::MaximizeRestore
        || client->quickTileMode() != KWin::QuickTileMode{}
        || m_host->sizesItselfForCardStage(window)) {
        return;
    }
    // Room is made for keys typing into this card, never for keys another
    // window asked for while the card waits behind it.
    auto keyboardTop = m_host->inputPanelTopForCardStage(tablet);
    // Keys typing into another window over the card, as Search's field does,
    // still cover its foot: room it was given stays, and none is made.
    const bool roomGiven = m_keyboardRoom
        && (m_keyboardRoom->height < m_keyboardRoom->base.height()
            || m_keyboardRoom->top != m_keyboardRoom->base.y());
    if (!m_host->keyboardTypesIntoForCardStage(window) && !roomGiven) keyboardTop.reset();
    if (keyboardTop) m_keyboardRoomGrace.stop();
    // Keys gone without a word of where they were going may only be blinking
    // as the keyboard hands over; the room is given back once they stay gone.
    if (!keyboardTop && roomGiven && !m_keyboardHeadingTop && !m_keyboardRoomReturnDue) {
        if (!m_keyboardRoomGrace.isActive()) m_keyboardRoomGrace.start();
        return;
    }
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
        if (m_keyboardRoom->height < base.height() || m_keyboardRoom->top != base.y()) {
            m_keyboardRoom->height = base.height();
            m_keyboardRoom->top = base.y();
            QScopedValueRollback<bool> applying(m_applyingWindowState, true);
            client->moveResize(base);
            KWin::effects->addRepaintFull();
            qInfo() << "Kadunce keyboard room returned" << logNameOf(window)
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
        m_keyboardRoom = KeyboardRoom{window, base, base.height(), base.y()};
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
        const auto drawn = m_keyboardRoomDrawTimes.constFind(window->windowClass());
        m_keyboardHeadingTimer.start(keyboardRoomAskDelay(m_keyboardHeadingDuration,
            drawn == m_keyboardRoomDrawTimes.cend() ? std::nullopt : std::optional<int>(*drawn)));
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

} // namespace Kadunce
