/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "StaleTouchRelease.h"
#include <input.h>
#include <input_event.h>
#include <window.h>
#include <QPointer>
#include <QDebug>
#include <QHash>
#include <QLineF>
#include <QPointF>
#include <QSet>
#include <QString>
#include <chrono>
#include <functional>

namespace Kadunce {
// Passive: sees every contact ahead of KWin's own edges and Kadunce's other
// filters, and never takes one. The workspace router learns from it which
// contacts are really down, so a release another filter took cannot leave it
// counting a finger that lifted. A contact in the bottom swipe's band that
// never reaches the router says so when it lifts. A contact on a window's
// title while Cards is up says what lay under it and where it went. Every
// contact says where it came down, on what, and how far it travelled.
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
    // Where each contact came down, before anything takes it.
    std::function<void(const QPointF &)> touchedAt;
    // Runs as each contact lifts, before KWin's own handling sees the lift.
    std::function<void(qint32)> onTouchUp;
    // Runs as each contact lifts, after it is logged (EffectTouchCarry.cpp).
    std::function<void(qint32)> onLift;
    // Names the window under a point, for each contact's down and lift lines.
    std::function<QString(const QPointF &)> describeUnder;
    // The application of the window under a point, or nothing.
    std::function<QString(const QPointF &)> applicationAt;
    // A contact: where it came down and on what, how far it has gone, and
    // when it last moved, on the steady clock in milliseconds.
    struct Contact {
        qint32 id = -1;
        QPointF start;
        QPointF last;
        QString under;
        QString application;
        QPointer<KWin::Window> window;
        double travelled = 0.0;
        qint64 lastMoved = 0;
    };
    [[nodiscard]] static qint64 now() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    // Installed after the other filters of its order, so KWin runs it first.
    TouchWitness() : InputEventFilter(KWin::InputFilterOrder::ScreenEdge) {
        KWin::input()->installInputEventFilter(this);
    }
    bool touchDown(KWin::TouchDownEvent *event) override {
        if (beforeTouchDown) beforeTouchDown(event->id);
        if (touchedAt) touchedAt(event->pos);
        m_down.insert(event->id);
        const QString on = describeUnder ? describeUnder(event->pos) : QString();
        m_contacts.insert(event->id, {event->id, event->pos, event->pos, on,
            applicationAt ? applicationAt(event->pos) : QString(),
            KWin::input()->findToplevel(event->pos), 0.0, now()});
        qInfo().noquote() << "Kadunce touch" << event->id << "down at" << event->pos << "on" << on;
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
        const auto contact = m_contacts.find(event->id);
        if (contact != m_contacts.end()) {
            contact->last = event->pos;
            contact->travelled = std::max(contact->travelled, QLineF(contact->start, event->pos).length());
            contact->lastMoved = now();
        }
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
        if (event->id == StaleTouchRelease::Id) return false;
        m_down.remove(event->id);
        if (const auto contact = m_contacts.constFind(event->id); contact != m_contacts.cend()) {
            m_lifted = *contact;
            m_liftedAt = now();
            qInfo().noquote().nospace() << "Kadunce touch " << event->id << " lifted at " << contact->last
                                        << " after travelling " << qRound(contact->travelled) << " px, last moved "
                                        << m_liftedAt - contact->lastMoved << " ms before, from "
                                        << contact->under << "; now on "
                                        << (describeUnder ? describeUnder(contact->last) : QString());
            m_contacts.erase(contact);
        }
        if (onLift) onLift(event->id);
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
        m_contacts.clear();
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
    [[nodiscard]] const QHash<qint32, Contact> &contacts() const { return m_contacts; }
    // The contact that lifted last, and how long ago, in milliseconds.
    [[nodiscard]] const Contact &lifted() const { return m_lifted; }
    [[nodiscard]] qint64 sinceLift() const { return m_lifted.id < 0 ? -1 : now() - m_liftedAt; }

private:
    QSet<qint32> m_down;
    QHash<qint32, Contact> m_contacts;
    Contact m_lifted;
    qint64 m_liftedAt = 0;
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
