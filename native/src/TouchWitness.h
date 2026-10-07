/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <input.h>
#include <input_event.h>
#include <QDebug>
#include <QHash>
#include <QPointF>
#include <QSet>
#include <functional>

namespace Kadunce {
// Passive: sees every contact ahead of KWin's own edges and Kadunce's other
// filters, and never takes one. The workspace router learns from it which
// contacts are really down, so a release another filter took cannot leave it
// counting a finger that lifted. A contact in the bottom swipe's band that
// never reaches the router says so when it lifts.
class TouchWitness final : public KWin::InputEventFilter {
public:
    std::function<bool(const QPointF &)> inBottomBezel;
    // Installed after the other filters of its order, so KWin runs it first.
    TouchWitness() : InputEventFilter(KWin::InputFilterOrder::ScreenEdge) {
        KWin::input()->installInputEventFilter(this);
    }
    bool touchDown(KWin::TouchDownEvent *event) override {
        m_down.insert(event->id);
        if (inBottomBezel && inBottomBezel(event->pos)) m_unseenBottom.insert(event->id, event->pos);
        else m_unseenBottom.remove(event->id);
        return false;
    }
    bool touchUp(KWin::TouchUpEvent *event) override {
        m_down.remove(event->id);
        const auto unseen = m_unseenBottom.constFind(event->id);
        if (unseen != m_unseenBottom.cend()) {
            qInfo() << "Kadunce bottom-edge contact" << *unseen
                    << "never reached workspace input: KWin handled it first";
            m_unseenBottom.erase(unseen);
        }
        return false;
    }
    bool touchCancel() override {
        m_down.clear();
        m_unseenBottom.clear();
        return false;
    }
    void reachedRouter(qint32 id) { m_unseenBottom.remove(id); }
    [[nodiscard]] const QSet<qint32> &down() const { return m_down; }

private:
    QSet<qint32> m_down;
    QHash<qint32, QPointF> m_unseenBottom;
};
}
