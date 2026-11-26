/* SPDX-License-Identifier: GPL-2.0-or-later */

// A card sent to the workspace or the display beside slides out by the side
// it went, and on another display in from the edge they share.
#include "EffectInternal.h"
#include <cmath>

namespace Kadunce
{

void Effect::sendAway(KWin::EffectWindow *window, int step, KWin::LogicalOutput *to)
{
    if (!window || window->isDeleted() || !window->screen() || window->isMinimized() || motion(220) <= 1) return;
    m_sentAway.remove(window);
    SentAway away{KWin::EffectWindowVisibleRef(window, KWin::EffectWindow::PAINT_DISABLED_BY_DESKTOP),
                  window->screen(), to, QRectF(window->frameGeometry()), step, {}};
    away.timer.start();
    m_sentAway.insert(window, std::move(away));
    KWin::effects->addRepaintFull();
}

std::optional<QPointF> Effect::sentAwayShift(KWin::EffectWindow *window, KWin::LogicalOutput *painting) const
{
    // Where a card sent away is drawn on `painting`, against where it stands
    // now: on the display it left, from where it stood to just past the edge
    // toward where it went; on the display beside, in from the edge they
    // share to its place. 220 ms, OutCubic, each on the one clock.
    const auto it = m_sentAway.constFind(window);
    if (it == m_sentAway.constEnd() || !window || window->isDeleted() || !it->output) return std::nullopt;
    const int duration = motion(220);
    if (it->timer.elapsed() >= duration) return std::nullopt;
    const double t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(double(it->timer.elapsed()) / duration);
    const QRectF now(window->frameGeometry());
    if (painting && painting == it->output) {
        const QRectF screen(it->output->geometry());
        const double reach = it->step > 0 ? screen.right() - it->from.x() : -(it->from.right() - screen.x());
        return QPointF(it->from.x() - now.x() + reach * t, it->from.y() - now.y());
    }
    if (painting && painting == it->to) {
        const QRectF screen(it->to->geometry());
        const double reach = it->step > 0 ? -(now.right() - screen.x()) : screen.right() - now.x();
        return QPointF(reach * (1.0 - t), 0.0);
    }
    return QPointF(std::nan(""), 0.0);
}

void Effect::settleSentAway(KWin::ScreenPrePaintData &data)
{
    for (auto it = m_sentAway.begin(); it != m_sentAway.end();) {
        if (!sentAwayShift(it.key(), nullptr)) {
            KWin::effects->addRepaintFull();
            it = m_sentAway.erase(it);
            continue;
        }
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        m_continueRepaint = true;
        ++it;
    }
}

std::optional<bool> Effect::paintSentAway(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
    KWin::EffectWindow *window, int mask, const KWin::Region &deviceRegion, KWin::WindowPaintData &data)
{
    const auto shift = sentAwayShift(window, m_paintingOutput);
    if (!shift) return std::nullopt;
    if (!m_paintingOutput || std::isnan(shift->x())) return true;
    data.setXTranslation(data.xTranslation() + shift->x());
    data.setYTranslation(data.yTranslation() + shift->y());
    const auto clip = deviceRegion
        & KWin::Region(viewport.mapToDeviceCoordinatesAligned(KWin::RectF(m_paintingOutput->geometry())));
    return painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window,
        mask | PAINT_WINDOW_TRANSFORMED, clip, data); });
}

void Effect::showSentAhead(KWin::VirtualDesktop *current)
{
    // Entering a workspace a card was sent to shows it by the rule that holds
    // there, before the first frame: a layout takes it behind its panes, to
    // come in when called; cards make it the Active card; a display holding
    // nothing yet starts its cards with it (holdDisplaysInCards).
    const auto sent = current ? m_sentAhead.take(current->id()) : QList<QPointer<KWin::EffectWindow>>();
    for (const auto &card : sent) {
        if (!card || card->isDeleted() || card->isMinimized() || !card->isOnDesktop(current)) continue;
        KWin::LogicalOutput *output = card->screen();
        if (!output || !isCardDisplay(output)) continue;
        SessionScope here(this, m_currentSession, stageKeyFor(output));
        const char *rule = "as the display's first card";
        if (m_cardStage->presentsBento() && m_desktopStage->hasSessionOnOutput(output->name())) {
            // A layout takes it in as a window opening there does, never
            // moving it from a frame drawn on another workspace.
            if (m_cardStage->liveCardIndex(card) < 0) (void)m_cardStage->admitArrivalAsCard(card);
            noteOpened(card);
            const QPointer<KWin::EffectWindow> sent = card;
            rule = m_cardStage->liveCardIndex(card) >= 0 && m_desktopStage->admitCardToLiveBento(card,
                       [this, sent] { return sent && m_cardStage->releaseCardToLiveBento(sent); })
                ? "in the layout, coming in as a window opening there does"
                : "behind the layout, as a card the layout takes in when called";
        } else if (m_cardStage->presentsBento()) rule = "behind the layout, as a card the layout takes in when called";
        else if (m_cardStage->isActive()) {
            if (m_cardStage->liveCardIndex(card) < 0) (void)m_cardStage->admitArrivalAsCard(card);
            rule = m_cardStage->promoteToActive(card) ? "as the Active card" : "as a card it could not make Active";
        }
        qInfo().nospace() << "Kadunce " << Revision << " shows " << logNameOf(card) << ", sent here before, on "
                          << output->name() << " " << rule;
    }
}

} // namespace Kadunce
