/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Moving a placed window and the dividers between panes.
#include "DesktopStageInternal.h"

namespace Kadunce
{

namespace
{
// A surface laid over one pane at its size, as Gooseberry's notes are over
// the window they belong to, covers that pane and nothing beside it, so no
// divider of the layout is hidden behind it.
bool liesOverPane(const KWin::EffectWindow *window, const QList<QPointer<KWin::EffectWindow>> &panes)
{
    const QRectF frame(window->frameGeometry());
    return std::any_of(panes.cbegin(), panes.cend(), [&frame](const auto &pane) {
        if (!pane || pane->isDeleted()) return false;
        const QRectF place(pane->frameGeometry());
        return std::abs(frame.x() - place.x()) <= 1.5 && std::abs(frame.y() - place.y()) <= 1.5
            && std::abs(frame.width() - place.width()) <= 1.5
            && std::abs(frame.height() - place.height()) <= 1.5;
    });
}
}

void DesktopStageController::handleWindowMoveResizeStarted(KWin::EffectWindow *window)
{
    m_applicationGuard.invalidate();
    if (!window || m_interactionWindow) {
        return;
    }
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        Session &session = it.value();
        if (!session.applying && session.windows.contains(window)) {
            m_interactionWindow = window;
            m_interactionStart = window->frameGeometry();
            m_interactionOutput = it.key();
            m_pendingDropOutput.clear();
            m_interactionResize = window->isUserResize();
            m_settleTimer.stop();
            qInfo() << "Kadunce" << Revision
                    << "began Bento interaction" << logNameOf(window)
                    << (m_interactionResize ? "rail" : "carry");
            // KWin resizes a pane by its own edge only where Kadunce drew no
            // divider to take the press, so say which pane kept it from one.
            if (m_interactionResize && session.zones.isEmpty()) {
                if (auto *output = outputForKey(session.outputName)) {
                    const auto pixels = sessionPixels(session, stageArea(output));
                    for (int index = 0; index < session.windows.size() && index < int(pixels.size()); ++index) {
                        const auto &pane = session.windows.at(index);
                        const auto &place = pixels.at(std::size_t(index));
                        if (pane && !pane->isDeleted()
                            && (pane->screen() != output || pane->isMinimized()
                                || !withinPixel(pane->frameGeometry(), place)))
                            qInfo() << "Kadunce" << Revision << "drew no divider on" << session.outputName
                                    << "as" << logNameOf(pane) << "stands at" << pane->frameGeometry()
                                    << "outside its place" << QRectF(place.x, place.y, place.width, place.height);
                    }
                }
            }
            return;
        }
    }
}

void DesktopStageController::handleWindowMoveResizeStepped(KWin::EffectWindow *window,
                                           const KWin::RectF &)
{
    if (!window || !window->window()
        || m_interactionWindow != window
        || m_interactionResize) {
        return;
    }
    KWin::LogicalOutput *destination = KWin::effects->screenAt(
        KWin::effects->cursorPos().toPoint());
    if (!destination) {
        return;
    }
    const QString destinationKey = outputKey(destination);
    if (destinationKey == m_interactionOutput) {
        m_pendingDropOutput.clear();
        return;
    }
    if (m_pendingDropOutput != destinationKey) {
        m_pendingDropOutput = destinationKey;
        qInfo() << "Kadunce" << Revision
                << "armed Bento drop across output seam"
                << m_interactionOutput << "to" << destinationKey;
    }
}

void DesktopStageController::handleWindowMoveResizeFinished(KWin::EffectWindow *window)
{
    if (!window || m_interactionWindow != window) {
        return;
    }
    const QString sourceKey = m_interactionOutput;
    const KWin::RectF start = m_interactionStart;
    const bool resized = m_interactionResize;
    m_interactionWindow = nullptr;
    m_interactionStart = {};
    m_interactionOutput.clear();
    m_pendingDropOutput.clear();
    m_interactionResize = false;

    auto source = m_sessions.find(sourceKey);
    if (source == m_sessions.end()) {
        return;
    }
    // KWin has finalized (or canceled) the move before this signal. A cursor
    // left across the seam must not turn Escape into an accepted transfer.
    KWin::LogicalOutput *destination = window->screen();
    const bool changedOutput = destination
        && outputKey(destination) != sourceKey;
    if (changedOutput && !resized) {
        if (!handoffWindowToOutput(window, destination, window->frameGeometry())) {
            // A native titlebar drag already moved the client. Rejected managed
            // transfer retains the source session; replay its unchanged layout.
            source = m_sessions.find(sourceKey);
            if (source != m_sessions.end() && applySession(source.value(), false)) scheduleSettle();
        }
        return;
    }

    source = m_sessions.find(sourceKey);
    if (source == m_sessions.end()) {
        return;
    }
    if (!source->zones.isEmpty()) {
        // A pane resized in its zone moved the zone's edges, which KWin owns;
        // one moved into another zone takes that zone.
        syncZonesFromKde(source.value());
    } else if (resized) {
        adjustRail(source.value(), window, start,
                        window->frameGeometry());
    }
    if (applySession(source.value(), false)) scheduleSettle();
}

QList<DesktopStageController::GrabRail> DesktopStageController::grabRails() const
{
    QList<GrabRail> rails;
    if (m_restoring || m_interactionWindow) return rails;
    for (const auto &session : m_sessions) {
        auto *output = outputForKey(session.outputName);
        // KWin owns the edges between panes in its zones. A pane short of its
        // place still shares the divider at its place's edge.
        if (!output || session.applying || !session.zones.isEmpty()
            || !sessionGeometryMatches(session, true)) continue;
        const QRectF area(stageArea(output));
        for (int i = 0; i < int(session.rects.size()); ++i) {
            const auto &a = session.rects[i];
            for (int j = 0; j < int(session.rects.size()); ++j) {
                if (i == j) continue;
                const auto &b = session.rects[j];
                for (bool vertical : {true, false}) {
                    const double boundary = vertical ? a.x+a.width : a.y+a.height;
                    const double opposite = vertical ? b.x : b.y;
                    const double first = std::max(vertical ? a.y : a.x, vertical ? b.y : b.x);
                    const double last = std::min(vertical ? a.y+a.height : a.x+a.width,
                        vertical ? b.y+b.height : b.x+b.width);
                    if (std::abs(boundary-opposite) > .002 || last <= first) continue;
                    const QPointF center(area.x() + (vertical ? boundary : (first+last)/2)*area.width(),
                        area.y() + (vertical ? (first+last)/2 : boundary)*area.height());
                    bool covered = false;
                    const auto stacking = KWin::effects->stackingOrder();
                    for (auto it = stacking.crbegin(); it != stacking.crend(); ++it) {
                        auto *w = *it;
                        if (w == session.windows[i] || w == session.windows[j]) break;
                        if (w->isVisible() && !w->isDesktop() && !session.windows.contains(w)
                            && !liesOverPane(w, session.windows)
                            && QRectF(w->frameGeometry()).intersects(QRectF(center.x()-16,center.y()-16,32,32))) {
                            covered = true; break;
                        }
                    }
                    if (covered) continue; // Do not paint/grab through Plasma popups or free windows.
                    rails.append({session.outputName, i, vertical,
                        QRectF(center.x()-(vertical ? 2 : 21), center.y()-(vertical ? 21 : 2),
                            vertical ? 4 : 42, vertical ? 42 : 4),
                        (vertical
                            ? QRectF(center.x()-16,area.y()+first*area.height(),32,(last-first)*area.height())
                            : QRectF(area.x()+first*area.width(),center.y()-16,(last-first)*area.width(),32))
                            .intersected(area)});
                }
            }
        }
    }
    if (railValid()) for (auto &rail : rails) {
        const auto &drag = *m_railDrag;
        if (rail.output != drag.rail.output || rail.index != drag.rail.index
            || rail.vertical != drag.rail.vertical) continue;
        const auto &rect = drag.preview.rects[rail.index];
        QPointF center = rail.pill.center();
        if (rail.vertical) center.setX(drag.area.x()+(rect.x+rect.width)*drag.area.width());
        else center.setY(drag.area.y()+(rect.y+rect.height)*drag.area.height());
        rail.pill.setSize(rail.vertical ? QSizeF(4,48) : QSizeF(48,4));
        rail.pill.moveCenter(center);
    }
    return rails;
}

QString DesktopStageController::dividersWithheld(KWin::LogicalOutput *output) const
{
    const Session *session = output ? sessionForOutput(output) : nullptr;
    if (!session) return QStringLiteral("no layout");
    if (m_restoring) return QStringLiteral("windows are being given back");
    if (m_interactionWindow)
        return QStringLiteral("%1 is being moved or resized").arg(logNameOf(m_interactionWindow));
    if (session->applying) return QStringLiteral("the layout is being placed");
    if (!session->zones.isEmpty()) return QStringLiteral("KWin's zones own the edges");
    if (sessionGeometryMatches(*session, true)) return {};
    const auto pixels = sessionPixels(*session, stageArea(output));
    for (int index = 0; index < session->windows.size() && index < int(pixels.size()); ++index) {
        const auto &pane = session->windows.at(index);
        const auto &place = pixels.at(std::size_t(index));
        if (!pane || pane->isDeleted()) return QStringLiteral("a pane closed");
        if (pane->screen() != output || pane->isMinimized()
            || !withinPixel(pane->frameGeometry(), place)) {
            const QRectF frame(pane->frameGeometry());
            return QStringLiteral("%1 stands at %2,%3 %4x%5, outside its place %6,%7 %8x%9")
                .arg(logNameOf(pane)).arg(frame.x()).arg(frame.y()).arg(frame.width()).arg(frame.height())
                .arg(place.x).arg(place.y).arg(place.width).arg(place.height);
        }
    }
    return QStringLiteral("its panes and places differ in number");
}

bool DesktopStageController::railValid() const
{
    if (!m_railDrag || m_restoring || m_interactionWindow
        || m_applicationGuard.generation() != m_railDrag->generation) return false;
    const auto it = m_sessions.constFind(m_railDrag->rail.output);
    auto *output = outputForKey(m_railDrag->rail.output);
    return it != m_sessions.cend() && output && stageArea(output) == m_railDrag->area
        && it->windows == m_railDrag->original.windows && sessionGeometryMatches(*it, true);
}

bool DesktopStageController::beginRail(QPointF position)
{
    if (m_railDrag) return false;
    for (const auto &rail : grabRails()) {
        // Claim the entire shared divider at down, before Plasma sees a hold.
        // The centered pill is feedback, not the extent of the input target.
        if (!rail.hitArea.contains(position)) continue;
        const auto session = m_sessions.value(rail.output);
        bool covered = false;
        const auto stacking = KWin::effects->stackingOrder();
        for (auto it = stacking.crbegin(); it != stacking.crend(); ++it) {
            auto *w = *it;
            if (w == session.windows[rail.index]) break;
            if (w->isVisible() && !w->isDesktop() && !session.windows.contains(w)
                && !liesOverPane(w, session.windows)
                && QRectF(w->frameGeometry()).contains(position)) { covered = true; break; }
        }
        if (covered) continue;
        m_railDrag = RailDrag{rail, session, session, m_applicationGuard.generation(),
            stageArea(outputForKey(rail.output)), position};
        m_railRevealed = false;
        return true;
    }
    return false;
}

void DesktopStageController::updateRail(QPointF position)
{
    if (!railValid()) { finishRail(false); return; }
    m_railRevealed = true;
    auto &drag = *m_railDrag;
    drag.preview = drag.original;
    const auto window = drag.original.windows[drag.rail.index];
    // From the pane's place, not its frame, which can stop short of it.
    const auto pixels = sessionPixels(drag.original, drag.area);
    if (drag.rail.index >= static_cast<int>(pixels.size())) { finishRail(false); return; }
    const auto &place = pixels.at(std::size_t(drag.rail.index));
    const KWin::RectF start(place.x, place.y, place.width, place.height);
    QRectF finish(start);
    if (drag.rail.vertical) finish.setRight(finish.right()+position.x()-drag.contact.x());
    else finish.setBottom(finish.bottom()+position.y()-drag.contact.y());
    adjustRail(drag.preview, window, start, KWin::RectF(finish));
    KWin::effects->addRepaintFull();
}

QList<QRectF> DesktopStageController::railPreview(const QString &output) const
{
    QList<QRectF> boxes;
    if (!m_railRevealed || !railValid() || m_railDrag->rail.output != output) return boxes;
    const auto &a = m_railDrag->area;
    for (const auto &r : makePixelBentoLayout(m_railDrag->preview.rects, a.x(), a.y(), a.width(), a.height()))
        boxes.append(QRectF(r.x,r.y,r.width,r.height));
    return boxes;
}

void DesktopStageController::finishRail(bool commit)
{
    if (!m_railDrag) return;
    const bool valid = commit && railValid();
    auto drag = std::move(*m_railDrag);
    m_railDrag.reset();
    m_railRevealed = false;
    if (valid) {
        m_applicationGuard.invalidate();
        m_sessions[drag.rail.output].rects = std::move(drag.preview.rects);
        if (applySession(m_sessions[drag.rail.output], false)) scheduleSettle();
    }
    KWin::effects->addRepaintFull();
}

void DesktopStageController::adjustRail(Session &session,
                             KWin::EffectWindow *window,
                             const KWin::RectF &start,
                             const KWin::RectF &finish)
{
    const int resizedIndex = session.windows.indexOf(window);
    KWin::LogicalOutput *output = outputForKey(session.outputName);
    if (resizedIndex < 0 || resizedIndex >= static_cast<int>(session.rects.size())
        || !output) {
        return;
    }
    const std::array<double, 4> deltas{
        std::abs(finish.x() - start.x()),
        std::abs(finish.right() - start.right()),
        std::abs(finish.y() - start.y()),
        std::abs(finish.bottom() - start.bottom()),
    };
    const int edge = static_cast<int>(std::distance(
        deltas.cbegin(), std::max_element(deltas.cbegin(), deltas.cend())));
    if (deltas.at(edge) < 1.0) {
        return;
    }
    const bool vertical = edge < 2;
    const BentoRect base = session.rects.at(resizedIndex);
    const double oldBoundary = edge == 0 ? base.x
        : edge == 1 ? base.x + base.width
        : edge == 2 ? base.y : base.y + base.height;
    if (oldBoundary < 0.001 || oldBoundary > 0.999) {
        return;
    }
    const double segmentStart = vertical ? base.y : base.x;
    const double segmentEnd = vertical
        ? base.y + base.height : base.x + base.width;
    const KWin::Rect area = stageArea(output);
    double boundary = vertical
        ? ((edge == 0 ? finish.x() : finish.right()) - area.x())
            / area.width()
        : ((edge == 2 ? finish.y() : finish.bottom()) - area.y())
            / area.height();

    constexpr double Epsilon = 0.002;
    std::vector<BentoRect> next = session.rects;
    double connectedStart = segmentStart;
    double connectedEnd = segmentEnd;
    bool expanded = true;
    while (expanded) {
        expanded = false;
        for (const BentoRect &rect : next) {
            const double crossStart = vertical ? rect.y : rect.x;
            const double crossEnd = vertical
                ? rect.y + rect.height : rect.x + rect.width;
            const double startEdge = vertical ? rect.x : rect.y;
            const double endEdge = vertical
                ? rect.x + rect.width : rect.y + rect.height;
            const bool attached = std::abs(endEdge - oldBoundary) < Epsilon
                || std::abs(startEdge - oldBoundary) < Epsilon;
            const bool connected = crossEnd >= connectedStart - Epsilon
                && crossStart <= connectedEnd + Epsilon;
            if (!attached || !connected) {
                continue;
            }
            const double newStart = std::min(connectedStart, crossStart);
            const double newEnd = std::max(connectedEnd, crossEnd);
            if (newStart < connectedStart - Epsilon
                || newEnd > connectedEnd + Epsilon) {
                connectedStart = newStart;
                connectedEnd = newEnd;
                expanded = true;
            }
        }
    }
    std::vector<int> before;
    std::vector<int> after;
    for (int index = 0; index < static_cast<int>(next.size()); ++index) {
        const BentoRect &rect = next.at(index);
        const double crossStart = vertical ? rect.y : rect.x;
        const double crossEnd = vertical
            ? rect.y + rect.height : rect.x + rect.width;
        if (crossEnd < connectedStart - Epsilon
            || crossStart > connectedEnd + Epsilon) {
            continue;
        }
        const double startEdge = vertical ? rect.x : rect.y;
        const double endEdge = vertical
            ? rect.x + rect.width : rect.y + rect.height;
        if (std::abs(endEdge - oldBoundary) < Epsilon) {
            before.push_back(index);
        }
        if (std::abs(startEdge - oldBoundary) < Epsilon) {
            after.push_back(index);
        }
    }
    if (before.empty() || after.empty()) {
        return;
    }
    const double fallbackMinimum = vertical
        ? std::max(0.09, 180.0 / area.width())
        : std::max(0.11, 130.0 / area.height());
    double lower = 0.0;
    double upper = 1.0;
    for (const int index : before) {
        const BentoRect &rect = next.at(index);
        const QSizeF minimum = session.windows.at(index)->window()->minSize();
        const double advertised = vertical
            ? (minimum.width() + bentoPaneGap()) / area.width()
            : (minimum.height() + bentoPaneGap()) / area.height();
        lower = std::max(lower, (vertical ? rect.x : rect.y)
                         + std::max(fallbackMinimum, advertised));
    }
    for (const int index : after) {
        const BentoRect &rect = next.at(index);
        const QSizeF minimum = session.windows.at(index)->window()->minSize();
        const double advertised = vertical
            ? (minimum.width() + bentoPaneGap()) / area.width()
            : (minimum.height() + bentoPaneGap()) / area.height();
        upper = std::min(upper,
                         (vertical ? rect.x + rect.width
                                   : rect.y + rect.height)
                         - std::max(fallbackMinimum, advertised));
    }
    if (lower > upper) {
        return;
    }
    boundary = std::clamp(boundary, lower, upper);
    for (const int index : before) {
        BentoRect &rect = next.at(index);
        if (vertical) {
            rect.width = boundary - rect.x;
        } else {
            rect.height = boundary - rect.y;
        }
    }
    for (const int index : after) {
        BentoRect &rect = next.at(index);
        if (vertical) {
            const double end = rect.x + rect.width;
            rect.x = boundary;
            rect.width = end - boundary;
        } else {
            const double end = rect.y + rect.height;
            rect.y = boundary;
            rect.height = end - boundary;
        }
    }
    session.rects = std::move(next);
}

} // namespace Kadunce
