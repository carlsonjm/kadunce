/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// A window opening into a layout: unseen until it has drawn its pane, then
// shown where the panes that made room have drawn back (PaneReveal.h).
#include "EffectInternal.h"
#include <cxxabi.h>
#include <execinfo.h>
#include <utility>

namespace Kadunce
{

namespace
{
RevealBox revealBox(const QRectF &rect)
{
    return RevealBox{rect.x(), rect.y(), rect.width(), rect.height()};
}

// The calls that led here, nearest first, each as its library and function,
// so a line about the keys names what moved them.
QString callersNamed()
{
    void *frames[40];
    const int count = backtrace(frames, 40);
    char **symbols = backtrace_symbols(frames, count);
    QStringList named;
    for (int i = 1; symbols && i < count; ++i) {
        const QString symbol = QString::fromLocal8Bit(symbols[i]);
        const int open = symbol.indexOf(QLatin1Char('(')), plus = symbol.indexOf(QLatin1Char('+'), open);
        const QString library = symbol.left(open).section(QLatin1Char('/'), -1);
        const QByteArray mangled = open >= 0 && plus > open ? symbol.mid(open + 1, plus - open - 1).toLocal8Bit() : QByteArray();
        int status = -1;
        char *plain = mangled.isEmpty() ? nullptr : abi::__cxa_demangle(mangled.constData(), nullptr, nullptr, &status);
        named.append(library + QLatin1Char(' ') + (status == 0 && plain ? QString::fromLocal8Bit(plain)
            : mangled.isEmpty() ? symbol.mid(open) : QString::fromLocal8Bit(mangled)));
        free(plain);
    }
    free(symbols);
    return named.join(QStringLiteral(" < "));
}
}

void Effect::noteOpened(KWin::EffectWindow *window)
{
    m_opening.removeIf([window](const Opening &opening) {
        return !opening.window || opening.window->isDeleted() || opening.window == window
            || opening.since.elapsed() > OpenedStill;
    });
    if (!window) return;
    Opening opening;
    opening.window = window;
    opening.since.start();
    m_opening.append(opening);
}

void Effect::noteWoken(KWin::EffectWindow *window, const QRectF &icon)
{
    // A card waking into a layout from a frame never drawn there comes up in
    // its own place as one opening does, unseen until it has drawn its pane:
    // out of its dock icon when its place stands on the dock's edge, else
    // from 90% of its size about its middle. It never crosses another pane.
    noteOpened(window);
    if (!window || m_opening.isEmpty() || m_opening.last().window != window) return;
    m_opening.last().woken = true;
    m_opening.last().from = icon;
}

std::optional<QRectF> Effect::wokenBox(const KWin::EffectWindow *window, const QRectF &to, const QRectF &screen,
                                       double *opacity) const
{
    for (const Opening &opening : m_opening) {
        if (opening.window != window || !opening.woken) continue;
        if (opening.seenAt < 0) return to;
        const qint64 risen = opening.since.elapsed() - opening.seenAt;
        // Out of the icon only into a place on the dock's edge, so nothing
        // stands between them.
        const bool fromIcon = opening.from.isValid() && to.bottom() >= screen.bottom() - 2.0 * bentoPaneGap() - 64.0;
        const double rise = openedCardRise(risen, motion(fromIcon ? OpenedCardGrow : OpenedCardRise));
        if (rise >= 1.0) return std::nullopt;
        if (fromIcon) {
            if (opacity) *opacity = openedCardRise(risen, motion(OpenedCardGrowFade));
            const RevealBox box = openedCardFromIcon(revealBox(opening.from), revealBox(to),
                QEasingCurve(QEasingCurve::OutCubic).valueForProgress(rise));
            return QRectF(box.x, box.y, box.width, box.height);
        }
        if (opacity) *opacity = rise;
        const double scale = openedCardScale(rise);
        return QRectF(to.center() - QPointF(to.width(), to.height()) * scale / 2.0, to.size() * scale);
    }
    return std::nullopt;
}

KWin::EffectWindow *Effect::openingAmong(const QList<QPointer<KWin::EffectWindow>> &windows)
{
    // The layout that takes a window just opened asks it for its pane, and
    // its wait is counted from then: it stood at the size it opened with.
    for (Opening &opening : m_opening) {
        KWin::EffectWindow *window = opening.window.data();
        if (!window || window->isDeleted() || opening.seenAt >= 0 || !windows.contains(opening.window)) continue;
        if (!opening.asked) {
            opening.asked = true;
            opening.keys = opening.keys || KWin::effects->activeWindow() == window;
            opening.size = window->frameGeometry().size();
            opening.since.restart();
        }
        return window;
    }
    return nullptr;
}

void Effect::openedAsCard(KWin::EffectWindow *window, KWin::EffectWindow *over)
{
    // Only as the one card its display shows: a card that joined a layout
    // or Spread's row arrives as those do.
    if (!window || window->isDeleted() || !m_cardStage->isActive() || m_cardStage->presentsBento()
        || m_cardStage->presentation() != CardPresentation::Active || m_cardStage->selectedWindow() != window
        || m_desktopStage->managesWindow(window) || !isTabletOutput(window->screen())) return;
    for (Opening &opening : m_opening) {
        if (opening.window != window || opening.asked) continue;
        opening.asked = true;
        opening.card = true;
        opening.keys = opening.keys || KWin::effects->activeWindow() == window;
        opening.size = window->frameGeometry().size();
        opening.since.restart();
        if (over && over != window && !over->isDeleted() && !over->isMinimized() && over->screen() == window->screen()
            && !m_desktopStage->managesWindow(over)) opening.over = over;
        // Opened from the dock, it grows out of the icon it was opened from.
        opening.from = takeLaunchOrigin(window);
        qInfo().nospace() << "Kadunce " << Revision << " opened " << logNameOf(window) << " comes up as a card over "
                          << (opening.over ? logNameOf(opening.over.data()) : QStringLiteral("nothing"))
                          << (opening.from.isValid() ? ", growing from its dock icon at " : ", rising from 90%");
        if (opening.from.isValid()) qInfo() << "Kadunce" << Revision << "  dock icon" << opening.from;
        KWin::effects->addRepaintFull();
    }
}

void Effect::keysLeftOpening(KWin::EffectWindow *activated)
{
    // A window that opens is given the keys. Where they go to another window
    // before it is in view, or as it comes into view, the journal names the
    // calls that moved them: a client asking, the compositor handing them
    // on, or this effect. A few lines an opening at most.
    if (!isApplicationWindow(activated)) return;
    for (Opening &opening : m_opening) {
        KWin::EffectWindow *window = opening.window.data();
        if (!window || window->isDeleted()) continue;
        if (window == activated) {
            opening.keys = true;
            continue;
        }
        if (!opening.keys || !opening.asked || opening.kept >= 4 || window->isMinimized()
            || opening.since.elapsed() >= OpenedUnseenCap + OpenedKeysWatched
            || (opening.seenAt >= 0 && opening.since.elapsed() - opening.seenAt >= OpenedKeysWatched)) continue;
        ++opening.kept;
        qInfo().nospace() << "Kadunce " << Revision << " the keys left opened " << logNameOf(window) << " for "
                          << logNameOf(activated) << ", " << opening.since.elapsed() << " ms after it was placed ("
                          << (opening.seenAt < 0 ? "still unseen" : "coming into view") << "), by way of: "
                          << qPrintable(callersNamed());
    }
}

int Effect::Opening::rise() const
{
    return motion(from.isValid() ? OpenedCardGrow : OpenedCardRise);
}

bool Effect::openingCardDrawn(const KWin::EffectWindow *window) const
{
    return window && std::any_of(m_opening.cbegin(), m_opening.cend(), [window](const Opening &opening) {
        return opening.card && (opening.window == window || opening.over == window)
            && (opening.seenAt < 0 || opening.since.elapsed() - opening.seenAt < opening.rise());
    });
}

std::optional<bool> Effect::paintOpeningCard(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
    KWin::EffectWindow *window, int mask, const KWin::Region &deviceRegion, KWin::WindowPaintData &data)
{
    for (Opening &opening : m_opening) {
        if (!opening.card || (window != opening.window && window != opening.over)) continue;
        const qint64 risen = opening.seenAt < 0 ? -1 : opening.since.elapsed() - opening.seenAt;
        // Only while its display still shows it as its one card, where it
        // stands, with no keys up that it must end above.
        if (risen >= opening.rise() || m_previewDesktop || !opening.window || opening.window->isDeleted()
            || !m_paintingOutput || window->screen() != m_paintingOutput || hiddenOnOtherDesktop(window)
            || !m_cardStage->isActive() || m_cardStage->presentsBento()
            || m_cardStage->presentation() != CardPresentation::Active
            || m_cardStage->selectedWindow() != opening.window
            || m_cardStage->keyboardRoomEdge(opening.window.data())) return std::nullopt;
        // The card shown before stays as it stood until the new one is whole.
        if (window == opening.over)
            return painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window, mask, deviceRegion, data); });
        if (risen < 0) return true;
        if (!std::exchange(opening.reported, true))
            qInfo().nospace() << "Kadunce " << Revision << " opened " << logNameOf(window)
                              << " is first drawn as a card, with scale " << data.xScale() << "x" << data.yScale()
                              << " and opacity " << data.opacity() << " from the effects before this one";
        // Drawn past the effects after this one: the desktop's opening
        // animation would grow it from its middle a second time.
        const double rise = openedCardRise(risen, opening.rise());
        const QRectF frame(window->frameGeometry());
        if (opening.from.isValid() && frame.width() > 0.0 && frame.height() > 0.0) {
            const double t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(rise);
            const RevealBox box = openedCardFromIcon(revealBox(opening.from), revealBox(frame), t);
            data.setOpacity(openedCardRise(risen, motion(OpenedCardGrowFade)));
            data.setXScale(box.width / frame.width());
            data.setYScale(box.height / frame.height());
            data.setXTranslation(box.x - frame.x());
            data.setYTranslation(box.y - frame.y());
        } else {
            const double scale = openedCardScale(rise);
            data.setOpacity(rise);
            data.setXScale(scale);
            data.setYScale(scale);
            data.setXTranslation(frame.width() * (1.0 - scale) / 2.0);
            data.setYTranslation(frame.height() * (1.0 - scale) / 2.0);
        }
        return painted([&] { return KWin::effects->drawWindow(renderTarget, viewport, window,
            mask | PAINT_WINDOW_TRANSFORMED, deviceRegion, data); });
    }
    return std::nullopt;
}

bool Effect::openingsWait()
{
    bool waits = false;
    for (auto it = m_opening.begin(); it != m_opening.end();) {
        KWin::EffectWindow *window = it->window.data();
        if (!window || window->isDeleted() || !window->window() || it->since.elapsed() > OpenedStill) {
            it = m_opening.erase(it);
            continue;
        }
        // Frames come while a card that opened is coming up, too.
        if ((it->card || it->woken) && it->seenAt >= 0 && it->since.elapsed() - it->seenAt < it->rise()) waits = true;
        if (it->asked && it->seenAt < 0) {
            const QRectF frame(window->frameGeometry());
            const QRectF asked(window->window()->moveResizeGeometry());
            const bool inPlace = openedStandsInPlace(it->size.width(), it->size.height(),
                                                     revealBox(frame), revealBox(asked));
            if (openedStaysUnseen(inPlace, it->since.elapsed())) {
                waits = true;
            } else {
                it->seenAt = it->since.elapsed();
                // One line per opening, so a window that showed before it
                // had drawn its pane names how long it was given.
                qInfo().nospace() << "Kadunce " << Revision << " shows opened " << logNameOf(window) << " after "
                                  << it->seenAt << " ms (" << (inPlace ? "drew its pane" : "not drawn yet")
                                  << "): opened " << it->size << ", frame " << frame << ", asked " << asked;
            }
        }
        ++it;
    }
    return waits;
}

bool Effect::openingUnseen(const KWin::EffectWindow *window) const
{
    return window && std::any_of(m_opening.cbegin(), m_opening.cend(), [window](const Opening &opening) {
        return opening.window == window && opening.asked && opening.seenAt < 0;
    });
}

Effect::OpeningArrival Effect::openingArrival(const KWin::EffectWindow *window) const
{
    // How a window just opened into a layout comes into its pane: pushed in
    // beside the panes drawing back from over it, opened from the seam
    // between them, or pushed in from the nearest screen edge over a pane
    // that gives way to it (PaneReveal.h). Asked as each frame is drawn.
    OpeningArrival arrival;
    const BentoMotion *own = bentoMotionOf(window);
    if (!own || own->waitsFor != window) return arrival;
    arrival.opening = true;
    // A card woken into its place comes up there, where nothing else stands.
    for (const Opening &opening : m_opening)
        if (opening.window == window && opening.woken) {
            arrival.shown = own->to;
            arrival.how = opening.from.isValid() && own->to.bottom() >= own->outputGeometry.bottom() - 2.0 * bentoPaneGap() - 64.0
                ? "growing from its dock icon into its place" : "coming up in its place";
            return arrival;
        }
    // Only panes that began over its place count; one that barely moves
    // beside it does not make room for it.
    const RevealBox pane = revealBox(own->to);
    RevealBox shown = pane;
    QList<PushSide> sides;
    double over = 0.0;
    for (const BentoMotion &m : std::as_const(m_bentoMotions)) {
        if (m.window == window || m.waitsFor != window || m.output != own->output) continue;
        const auto box = bentoMotionRect(m.window.data());
        if (!box) continue;
        const NeighbourPush push = neighbourPush(pane, revealBox(m.from), revealBox(*box), revealBox(m.to), double(bentoPaneGap()));
        if (!push.covered) continue;
        arrival.beside = true;
        if (!sides.contains(push.side)) sides.append(push.side);
        over = std::max(over, push.over);
        shown = revealedBeside(shown, revealBox(*box), double(bentoPaneGap()));
    }
    arrival.shown = QRectF(shown.x, shown.y, shown.width, shown.height);
    if (arrival.beside) {
        // From one side it pushes in whole; from two, it opens from the seam.
        const OpenedPush push = sides.size() == 1 ? pushedFrom(sides.first(), over) : OpenedPush{};
        arrival.offset = QPointF(push.dx, push.dy);
        if (push.pushes) arrival.shown = QRectF(own->to).translated(arrival.offset).intersected(own->to);
        arrival.how = push.pushes ? "pushing in beside the panes" : "opening from the seam";
        return arrival;
    }
    // Over a pane that gives way to it, or into open room, it pushes in from
    // the screen edge nearest its place.
    const double t = own->timer.isValid() ? QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
        std::clamp(double(own->timer.elapsed()) / motion(220), 0.0, 1.0)) : 0.0;
    const OpenedPush push = openedFromNearestEdge(revealBox(own->to), revealBox(own->outputGeometry), t);
    arrival.offset = QPointF(push.dx, push.dy);
    arrival.shown = QRectF(own->to).translated(arrival.offset).intersected(own->to);
    arrival.how = "pushing in from its edge into open room";
    for (const auto &yielding : m_yieldingPanes) {
        if (yielding.output != own->output || !yielding.rect.intersects(own->to)) continue;
        const QRectF common = yielding.rect.intersected(own->to);
        if (common.width() * common.height() < 0.5 * own->to.width() * own->to.height()) continue;
        arrival.over = yielding.window;
        arrival.how = "pushing in over the pane it takes";
    }
    return arrival;
}

bool Effect::revealOpening(KWin::EffectWindow *window, QRectF &clip, KWin::WindowPaintData &data)
{
    // The opened window shows only where the panes have drawn back, a gutter
    // from each, coming in as `openingArrival` says. It is drawn as it
    // stands: the desktop's opening animation, which grows a window from its
    // middle, has no part in a pane.
    const OpeningArrival arrival = openingArrival(window);
    if (!arrival.opening) return false;
    clip = clip.intersected(arrival.shown);
    double opacity = 1.0;
    for (Opening &opening : m_opening) {
        if (opening.window != window) continue;

        if (std::exchange(opening.reported, true)) continue;
        qInfo().nospace() << "Kadunce " << Revision << " opened " << logNameOf(window)
                          << " is first drawn " << arrival.how << ", offset " << arrival.offset
                          << ", with scale " << data.xScale() << "x" << data.yScale()
                          << " and opacity " << data.opacity() << " from the effects before this one";
    }
    if (const auto own = bentoMotionOf(window)) (void)wokenBox(window, own->to, own->outputGeometry, &opacity);
    data.setOpacity(opacity);
    return true;
}

} // namespace Kadunce
