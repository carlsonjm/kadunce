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
        qInfo().nospace() << "Kadunce " << Revision << " opened " << logNameOf(window) << " comes up as a card over "
                          << (opening.over ? logNameOf(opening.over.data()) : QStringLiteral("nothing"));
        KWin::effects->addRepaintFull();
    }
}

KWin::EffectWindow *Effect::openingKeepsKeys(KWin::EffectWindow *activated)
{
    // A window that opens is given the keys, and is then out of sight for a
    // moment. The keys going to another window in that moment, or as it
    // comes into view, is nobody's pick: the window used before is the only
    // one in sight to take them. A few times at most, so two windows that
    // each insist cannot pass the keys back and forth.
    if (!isApplicationWindow(activated)) return nullptr;
    for (Opening &opening : m_opening) {
        KWin::EffectWindow *window = opening.window.data();
        if (!window || window->isDeleted()) continue;
        if (window == activated) {
            opening.keys = true;
            continue;
        }
        if (!opening.keys || !opening.asked || window->isMinimized()
            || opening.since.elapsed() >= OpenedUnseenCap + OpenedKeepsKeys
            || (opening.seenAt >= 0 && opening.since.elapsed() - opening.seenAt >= OpenedKeepsKeys)) continue;
        qInfo().nospace() << "Kadunce " << Revision << " the keys left opened " << logNameOf(window) << " for "
                                    << logNameOf(activated) << " by way of: " << qPrintable(callersNamed());
        if (opening.kept >= 3) continue;
        ++opening.kept;
        qInfo().nospace() << "Kadunce " << Revision << " kept the keys on opened " << logNameOf(window) << ", not "
                          << logNameOf(activated) << ", " << opening.since.elapsed() << " ms after it was placed ("
                          << (opening.seenAt < 0 ? "still unseen" : "coming into view") << ")";
        return window;
    }
    return nullptr;
}

bool Effect::openingCardDrawn(const KWin::EffectWindow *window) const
{
    return window && std::any_of(m_opening.cbegin(), m_opening.cend(), [window](const Opening &opening) {
        return opening.card && (opening.window == window || opening.over == window)
            && (opening.seenAt < 0 || opening.since.elapsed() - opening.seenAt < OpenedCardRise);
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
        if (risen >= OpenedCardRise || m_previewDesktop || !opening.window || opening.window->isDeleted()
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
        const double rise = openedCardRise(risen);
        const double scale = openedCardScale(rise);
        const QSizeF frame = window->frameGeometry().size();
        data.setOpacity(rise);
        data.setXScale(scale);
        data.setYScale(scale);
        data.setXTranslation(frame.width() * (1.0 - scale) / 2.0);
        data.setYTranslation(frame.height() * (1.0 - scale) / 2.0);
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
        if (it->card && it->seenAt >= 0 && it->since.elapsed() - it->seenAt < OpenedCardRise) waits = true;
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
