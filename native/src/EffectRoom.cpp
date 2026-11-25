/* SPDX-License-Identifier: GPL-2.0-or-later */

// Moment 3: a window held still over a place in a layout for a moment makes
// that room open before it is let go. The panes move to where the layout
// would put them, drawn only: nothing is placed until the drop, and a new
// place or a cancel closes the room again.
#include "EffectInternal.h"

namespace Kadunce
{

namespace
{
constexpr qint64 RoomHold = 150;
}

void Effect::aimRoom(KWin::ScreenPrePaintData &data)
{
    std::optional<KWin::RectF> aim;
    if (m_carriedWindow && m_carryDestination && m_carryPreview && !m_carryDestination->detachesToDesktop()
        && m_carryDestination->destinationOutput()) {
        aim = m_carryPreview;
        // The middle of a pane, joining its Stack, stays a highlight only.
        SessionScope own(this, m_currentSession, stageKeyFor(m_carryDestination->destinationOutput()));
        for (const auto &pane : m_desktopStage->panesOn(m_carryDestination->destinationOutput()->name()))
            if (pane && pane != m_carriedWindow && QRectF(pane->frameGeometry()) == QRectF(*aim)) aim.reset();
    }
    if (aim != m_roomAim) {
        m_roomAim = aim;
        m_roomAimSince.restart();
        if (m_roomOpen) closeRoom();
    } else if (aim && !m_roomOpen && m_roomAimSince.isValid() && m_roomAimSince.elapsed() >= RoomHold) {
        m_roomOpen = true;
        QTimer::singleShot(0, this, [this] { openRoom(); });
    }
    if (!m_carriedWindow && m_roomOpen) closeRoom();
    const int duration = motion(220);
    m_roomPreview.removeIf([this, duration](const RoomPreview &room) {
        return !room.window || room.window->isDeleted()
            || (!m_roomOpen && room.timer.elapsed() >= duration);
    });
    for (const auto &room : std::as_const(m_roomPreview))
        if (room.timer.elapsed() < duration) {
            data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
            m_continueRepaint = true;
        }
}

void Effect::openRoom()
{
    if (!m_roomOpen || !m_carriedWindow || !m_carryDestination || !m_carryDestination->destinationOutput()) return;
    SessionScope own(this, m_currentSession, stageKeyFor(m_carryDestination->destinationOutput()));
    QList<QPair<QPointer<KWin::EffectWindow>, KWin::RectF>> plan;
    (void)m_desktopStage->cardDropPreview(*m_carryDestination, &plan);
    int moved = 0;
    for (const auto &[window, rect] : std::as_const(plan)) {
        if (!window || window == m_carriedWindow || !m_desktopStage->managesWindow(window)) continue;
        const QRectF from = roomPreviewRect(window).value_or(QRectF(window->frameGeometry()));
        if (from == QRectF(rect)) continue;
        m_roomPreview.removeIf([&window](const RoomPreview &room) { return room.window == window; });
        RoomPreview room{window, from, QRectF(rect), {}};
        room.timer.start();
        m_roomPreview.append(room);
        ++moved;
    }
    qInfo().nospace() << "Kadunce " << Revision << " opens the room " << logNameOf(m_carriedWindow.data())
                      << " is held over before it is let go: " << moved << " panes make room";
    KWin::effects->addRepaintFull();
}

void Effect::closeRoom()
{
    // Back to where the layout stands, from wherever each pane is drawn now.
    m_roomOpen = false;
    for (RoomPreview &room : m_roomPreview) {
        if (!room.window) continue;
        room.from = roomPreviewRect(room.window).value_or(room.to);
        room.to = QRectF(room.window->frameGeometry());
        room.timer.restart();
    }
    KWin::effects->addRepaintFull();
}

std::optional<QRectF> Effect::roomPreviewRect(const KWin::EffectWindow *window) const
{
    for (const auto &room : m_roomPreview) {
        if (room.window != window || !window || window->isDeleted()) continue;
        const double t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
            std::clamp(double(room.timer.elapsed()) / motion(220), 0.0, 1.0));
        return QRectF(room.from.topLeft() + (room.to.topLeft() - room.from.topLeft()) * t,
                      room.from.size() + (room.to.size() - room.from.size()) * t);
    }
    return std::nullopt;
}

} // namespace Kadunce
