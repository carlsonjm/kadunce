/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <input.h>
#include <input_event.h>
#include <QDebug>
#include <QHash>
#include <QLineF>
#include <QPointF>
#include <QSet>
#include <QString>
#include <functional>

namespace Kadunce {
// Passive: sees every contact ahead of KWin's own edges and Kadunce's other
// filters, and never takes one. The workspace router learns from it which
// contacts are really down, so a release another filter took cannot leave it
// counting a finger that lifted. A contact in the bottom swipe's band that
// never reaches the router says so when it lifts. A contact on a window's
// title while Cards is up says what lay under it and where it went.
class TouchWitness final : public KWin::InputEventFilter {
public:
    std::function<bool(const QPointF &)> inBottomBezel;
    // What lies under a contact on a title while Cards is up, or nothing.
    std::function<QString(const QPointF &)> describeTitleTouch;
    // KWin's own touch state: which window it focuses and which contact its
    // title bar handling holds.
    std::function<QString()> describeTouchState;
    // Runs first for every contact, before KWin's own handling sees it.
    std::function<void(qint32)> beforeTouchDown;
    // Runs as each contact lifts, before KWin's own handling sees the lift.
    std::function<void(qint32)> onTouchUp;
    // Installed after the other filters of its order, so KWin runs it first.
    TouchWitness() : InputEventFilter(KWin::InputFilterOrder::ScreenEdge) {
        KWin::input()->installInputEventFilter(this);
    }
    bool touchDown(KWin::TouchDownEvent *event) override {
        if (beforeTouchDown) beforeTouchDown(event->id);
        m_down.insert(event->id);
        if (inBottomBezel && inBottomBezel(event->pos)) m_unseenBottom.insert(event->id, event->pos);
        else m_unseenBottom.remove(event->id);
        m_titles.remove(event->id);
        const QString title = describeTitleTouch ? describeTitleTouch(event->pos) : QString();
        if (!title.isEmpty()) {
            qInfo().noquote() << "Kadunce title touch" << event->id << "down at" << event->pos << title;
            m_titles.insert(event->id, {event->pos, event->pos});
        }
        return false;
    }
    bool touchMotion(KWin::TouchMotionEvent *event) override {
        const auto title = m_titles.find(event->id);
        if (title != m_titles.end()) {
            title->last = event->pos;
            // By the first motion KWin has handled the press, so its title
            // bar handling says whether it took it.
            if (!title->probed && describeTouchState) {
                title->probed = true;
                qInfo().noquote() << "Kadunce title touch" << event->id << "first moved; KWin"
                                  << describeTouchState();
            }
        }
        return false;
    }
    bool touchUp(KWin::TouchUpEvent *event) override {
        m_down.remove(event->id);
        if (onTouchUp) onTouchUp(event->id);
        const auto unseen = m_unseenBottom.constFind(event->id);
        if (unseen != m_unseenBottom.cend()) {
            qInfo() << "Kadunce bottom-edge contact" << *unseen
                    << "never reached workspace input: KWin handled it first";
            m_unseenBottom.erase(unseen);
        }
        const auto title = m_titles.constFind(event->id);
        if (title != m_titles.cend()) {
            qInfo().nospace() << "Kadunce title touch " << event->id << " lifted at " << title->last
                              << " after travelling " << QLineF(title->start, title->last).length()
                              << "; reached workspace input " << title->reached
                              << ", kept by Kadunce " << title->kept
                              << ", KWin began a move or resize " << title->moved;
            if (describeTouchState) qInfo().noquote() << "Kadunce title touch" << event->id
                                                      << "lifting; KWin" << describeTouchState();
            m_titles.erase(title);
        }
        return false;
    }
    bool touchCancel() override {
        m_down.clear();
        m_unseenBottom.clear();
        for (auto title = m_titles.cbegin(); title != m_titles.cend(); ++title)
            qInfo().nospace() << "Kadunce title touch " << title.key() << " was cancelled at " << title->last
                              << "; reached workspace input " << title->reached
                              << ", kept by Kadunce " << title->kept
                              << ", KWin began a move or resize " << title->moved;
        m_titles.clear();
        return false;
    }
    void reachedRouter(qint32 id) {
        m_unseenBottom.remove(id);
        const auto title = m_titles.find(id);
        if (title != m_titles.end()) title->reached = true;
    }
    void routed(qint32 id, bool kept) {
        const auto title = m_titles.find(id);
        if (title != m_titles.end()) title->kept = kept;
    }
    // KWin began moving or resizing a window while these contacts were down.
    void moveStarted() {
        for (auto &title : m_titles) title.moved = true;
    }
    [[nodiscard]] const QSet<qint32> &down() const { return m_down; }

private:
    QSet<qint32> m_down;
    QHash<qint32, QPointF> m_unseenBottom;
    struct Title {
        QPointF start;
        QPointF last;
        bool reached = false;
        bool kept = false;
        bool moved = false;
        bool probed = false;
    };
    QHash<qint32, Title> m_titles;
};
}
