/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// A window opening into a layout: unseen until it has drawn its pane, then
// shown where the panes that made room have drawn back (PaneReveal.h).
#include "EffectInternal.h"
#include <utility>

namespace Kadunce
{

namespace
{
RevealBox revealBox(const QRectF &rect)
{
    return RevealBox{rect.x(), rect.y(), rect.width(), rect.height()};
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

KWin::EffectWindow *Effect::openingAmong(const QList<QPointer<KWin::EffectWindow>> &windows)
{
    // The layout that takes a window just opened asks it for its pane, and
    // its wait is counted from then: it stood at the size it opened with.
    for (Opening &opening : m_opening) {
        KWin::EffectWindow *window = opening.window.data();
        if (!window || window->isDeleted() || opening.seenAt >= 0 || !windows.contains(opening.window)) continue;
        if (!opening.asked) {
            opening.asked = true;
            opening.size = window->frameGeometry().size();
            opening.since.restart();
        }
        return window;
    }
    return nullptr;
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

bool Effect::revealOpening(KWin::EffectWindow *window, QRectF &clip, KWin::WindowPaintData &data)
{
    // The opened window stands in its pane behind the panes drawing back
    // from over it, and shows only where they have left, a gutter from each.
    const BentoMotion *own = bentoMotionOf(window);
    if (!own || own->waitsFor != window) return false;
    RevealBox shown = revealBox(own->to);
    bool beside = false;
    for (const BentoMotion &m : std::as_const(m_bentoMotions)) {
        if (m.window == window || m.waitsFor != window || m.output != own->output) continue;
        beside = true;
        if (const auto box = bentoMotionRect(m.window.data()))
            shown = revealedBeside(shown, revealBox(*box), double(bentoPaneGap()));
    }
    clip = clip.intersected(QRectF(shown.x, shown.y, shown.width, shown.height));
    // It is drawn as it stands: the desktop's opening animation, which grows
    // a window from its middle, has no part in a pane. With no pane to draw
    // back from over it, it comes up in its place instead.
    double opacity = 1.0;
    for (Opening &opening : m_opening) {
        if (opening.window != window) continue;
        if (!beside && opening.seenAt >= 0) opacity = openedAloneOpacity(opening.since.elapsed() - opening.seenAt);
        if (std::exchange(opening.reported, true)) continue;
        qInfo().nospace() << "Kadunce " << Revision << " opened " << logNameOf(window)
                          << " is first drawn " << (beside ? "behind the panes beside it" : "alone")
                          << ", with scale " << data.xScale() << "x" << data.yScale()
                          << " and opacity " << data.opacity() << " from the effects before this one";
    }
    data.setOpacity(opacity);
    return true;
}

} // namespace Kadunce
