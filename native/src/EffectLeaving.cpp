/* SPDX-License-Identifier: GPL-2.0-or-later */

// A window closing in a shown layout stays still and dims under the pane
// growing across it (PaneLeaving.h).
#include "EffectInternal.h"
#include <utility>

namespace Kadunce
{

void Effect::grabPaneClosing(KWin::EffectWindow *window)
{
    // KDE's closing animation shrinks a window toward its centre; a pane's
    // closing is drawn here instead. It is claimed ahead of the close, as
    // KDE's effects may hear of the close before this one does.
    if (!window || window->isDeleted() || !window->window()) return;
    void *const grabbed = window->data(KWin::WindowClosedGrabRole).value<void *>();
    const bool pane = (m_desktopStage->managesWindow(window) || closesAsOnlyCard(window))
        && motion(PaneLeavingDuration) > 1;
    if (pane && !grabbed)
        window->setData(KWin::WindowClosedGrabRole, QVariant::fromValue(static_cast<void *>(this)));
    else if (!pane && grabbed == this)
        window->setData(KWin::WindowClosedGrabRole, QVariant());
}

bool Effect::closesAsOnlyCard(const KWin::EffectWindow *window) const
{
    // The one card a display shows closes as a pane left uncovered does: it
    // stays where it stood and fades out.
    return window && m_cardStage->isActive() && !m_cardStage->presentsBento()
        && m_cardStage->presentation() == CardPresentation::Active && m_cardStage->selectedWindow() == window;
}

void Effect::holdLeavingPane(KWin::EffectWindow *window, KWin::LogicalOutput *output)
{
    if (!window || !output || !window->isOnCurrentDesktop() || motion(PaneLeavingDuration) <= 1
        || window->data(KWin::WindowClosedGrabRole).value<void *>() != this) return;
    PaneLeaver leaver{KWin::EffectWindowDeletedRef(window), output, QRectF(window->frameGeometry()), {}, false};
    leaver.timer.start();
    m_paneLeavers.insert_or_assign(window, std::move(leaver));
    qInfo().nospace() << "Kadunce " << Revision << " lets " << logNameOf(window)
                      << " close where it stood, dimming under its neighbour";
    KWin::effects->addRepaintFull();
}

std::optional<double> Effect::paneLeaverOpacity(const KWin::EffectWindow *window) const
{
    const auto leaving = m_paneLeavers.find(const_cast<KWin::EffectWindow *>(window));
    if (leaving == m_paneLeavers.end()) return std::nullopt;
    const PaneLeaver &leaver = leaving->second;
    // Covered once a pane grows to cover it, and so to the end: the layout
    // may move only after the close has been handled.
    if (!leaver.covered && leaver.output) {
        QList<QRectF> growing;
        for (const auto &moving : m_bentoMotions)
            if (moving.output == leaver.output && moving.elevated) growing.append(moving.to);
        if (m_settlingWindow && m_settleFitted && m_settlingOutput == leaver.output) growing.append(m_settleTo);
        leaver.covered = paneLeavingCovered(leaver.rect, growing);
    }
    return paneLeavingOpacity(double(leaver.timer.elapsed()), double(motion(PaneLeavingDuration)), leaver.covered);
}

void Effect::settlePaneLeavers()
{
    const qint64 duration = motion(PaneLeavingDuration);
    for (auto it = m_paneLeavers.begin(); it != m_paneLeavers.end();) {
        if (it->second.timer.elapsed() < duration && it->second.output) { ++it; continue; }
        qInfo().nospace() << "Kadunce " << Revision << " lets the closed pane go"
                          << (it->second.covered ? ", covered by its neighbour" : ", faded where it stood");
        it = m_paneLeavers.erase(it);
    }
    if (!m_paneLeavers.empty()) KWin::effects->addRepaintFull();
}

} // namespace Kadunce
