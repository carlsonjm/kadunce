// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "KadunceKeys.h"
#include <input.h>
#include <input_event.h>
#include <QSet>
#include <functional>

namespace Kadunce {

// Kadunce's keys, taken ahead of KDE's global shortcuts and only while Kadunce
// runs: nothing is written to the person's shortcut settings, so a stop or a
// crash gives every key back. A key Kadunce takes is renamed to no key at all
// before KDE's shortcuts see it, so KDE neither acts on it nor reads Meta as
// pressed alone, which would open the launcher when Meta is let go; the
// renamed key is then dropped, press and release, before any window sees it.
class KeyRoute
{
public:
    // `answer` acts on a key and says whether it did.
    using Answer = std::function<bool(int key, Qt::KeyboardModifiers modifiers, bool repeat)>;

    explicit KeyRoute(Answer answer)
        : m_ahead(this)
        , m_behind(this)
        , m_answer(std::move(answer))
    {
        KWin::input()->installInputEventFilter(&m_ahead);
        KWin::input()->installInputEventFilter(&m_behind);
    }
    KeyRoute(const KeyRoute &) = delete;
    KeyRoute &operator=(const KeyRoute &) = delete;

private:
    class Ahead final : public KWin::InputEventFilter
    {
    public:
        explicit Ahead(KeyRoute *route)
            : InputEventFilter(KWin::InputFilterOrder::GlobalShortcut)
            , m_route(route)
        {
        }
        bool keyboardKey(KWin::KeyboardKeyEvent *event) override
        {
            auto &taken = m_route->m_taken;
            if (event->state == KWin::KeyboardKeyState::Released) {
                if (taken.contains(event->nativeScanCode)) event->key = Qt::Key_unknown;
                return false;
            }
            const bool repeat = event->state == KWin::KeyboardKeyState::Repeated;
            if (repeat && !taken.contains(event->nativeScanCode)) return false;
            if (!m_route->m_answer(event->key, event->modifiersRelevantForGlobalShortcuts, repeat)) {
                // A key held down that no longer means anything here is still
                // Kadunce's until it is let go.
                if (repeat) event->key = Qt::Key_unknown;
                return false;
            }
            taken.insert(event->nativeScanCode);
            event->key = Qt::Key_unknown;
            return false;
        }

    private:
        KeyRoute *m_route;
    };

    class Behind final : public KWin::InputEventFilter
    {
    public:
        explicit Behind(KeyRoute *route)
            : InputEventFilter(KWin::InputFilterOrder::Effects)
            , m_route(route)
        {
        }
        bool keyboardKey(KWin::KeyboardKeyEvent *event) override
        {
            auto &taken = m_route->m_taken;
            if (event->key != Qt::Key_unknown || !taken.contains(event->nativeScanCode)) return false;
            if (event->state == KWin::KeyboardKeyState::Released) taken.remove(event->nativeScanCode);
            return true;
        }

    private:
        KeyRoute *m_route;
    };

    Ahead m_ahead;
    Behind m_behind;
    Answer m_answer;
    QSet<quint32> m_taken;
};

} // namespace Kadunce
