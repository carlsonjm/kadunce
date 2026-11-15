/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QSet>
#include <QString>

#include <algorithm>

namespace Kadunce
{
// Whether the session may be ending, read from KWin's session states and
// from what Plasma's sign-out leaves on the session bus. Plasma saves the
// session, steps back to normal within a few milliseconds, and only then
// records and closes every window, which takes seconds; the compositor is
// stopped at the end without another state. So a normal state alone says
// little:
//  - the program that runs the sign-out is on the bus from the answer until
//    the sign-out is done or cancelled. While it is, the session has gone on
//    only once it waits on the person: a window asked something, or nothing
//    has closed for `WaitingMs`;
//  - a service of the session leaving the bus says the session is being
//    stopped, and nothing goes on after that;
//  - the program gone and no service stopped, the sign-out was cancelled.
// A workspace emptied in that time was not emptied by the person, so nothing
// is tidied away and nothing is given back from the first state that is not
// normal until the session has gone on. Where no such program is seen, the
// states alone decide: normal for `GoesOnMs`.
class SessionEndHold
{
public:
    static constexpr qint64 GoesOnMs = 2000;
    // Recording the windows takes Plasma under two seconds before the first
    // is asked to close; quiet for this long while the program runs, it waits
    // on a window that asks inside itself.
    static constexpr qint64 WaitingMs = 4000;
    // A window that opened to ask is given this long to be the last word.
    static constexpr qint64 AskedMs = 500;
    // The compositor is stopped within seconds of the session's services; a
    // session still here this long after is given back.
    static constexpr qint64 StoppingLongestMs = 15000;
    // A sign-out program still there this long after the session began to
    // end is no longer trusted to mean one is under way: KWin itself gives a
    // closing round two minutes.
    static constexpr qint64 ProgramLongestMs = 150000;

    void noteState(bool normal, qint64 nowMs)
    {
        if (!normal) {
            if (!m_ending) {
                m_endingSince = nowMs;
                m_programLeftAt = m_closedAt = m_askedAt = m_stoppingAt = -1;
            }
            m_ending = true;
            m_normalSince = -1;
            return;
        }
        if (m_ending && m_normalSince < 0) m_normalSince = nowMs;
    }

    // Plasma's sign-out program came to the session bus or left it.
    void noteSignOutProgram(bool running, qint64 nowMs)
    {
        if (m_ending && m_programRunning && !running) m_programLeftAt = nowMs;
        m_programRunning = running;
    }

    // An application's window closed.
    void noteWindowClosed(qint64 nowMs)
    {
        if (m_ending) m_closedAt = nowMs;
    }

    // A window opened to ask something, such as whether to save. True the
    // first time in an ending.
    bool noteAsked(qint64 nowMs)
    {
        if (!m_ending || m_askedAt >= 0) return false;
        m_askedAt = nowMs;
        return true;
    }

    // A service of the session left the bus: the session is being stopped.
    void noteSessionStopping(qint64 nowMs)
    {
        if (m_ending && m_stoppingAt < 0) m_stoppingAt = nowMs;
    }

    // True while windows may be closing for a sign-out.
    [[nodiscard]] bool holds(qint64 nowMs)
    {
        if (!m_ending) return false;
        const qint64 at = goesOnAt(nowMs);
        if (at >= 0 && nowMs >= at) {
            m_ending = false;
            m_normalSince = m_endingSince = -1;
            return false;
        }
        return true;
    }

    // When the session will have gone on if nothing else happens, on the
    // same clock; negative while that cannot be told, as the session is not
    // normal or not ending.
    [[nodiscard]] qint64 goesOnAt(qint64 nowMs) const
    {
        if (!m_ending || m_normalSince < 0) return -1;
        if (m_stoppingAt >= 0) return m_stoppingAt + StoppingLongestMs;
        if (m_programRunning && nowMs < m_endingSince + ProgramLongestMs) {
            if (m_askedAt >= 0) return std::max(m_normalSince, m_askedAt + AskedMs);
            return std::max(m_normalSince, m_closedAt) + WaitingMs;
        }
        return std::max(m_normalSince, m_programLeftAt) + GoesOnMs;
    }

    // True once a state other than normal has been seen and not yet outlived.
    [[nodiscard]] bool ending() const { return m_ending; }

private:
    bool m_ending = false;
    bool m_programRunning = false;
    qint64 m_endingSince = -1;
    qint64 m_normalSince = -1;
    qint64 m_programLeftAt = -1;
    qint64 m_closedAt = -1;
    qint64 m_askedAt = -1;
    qint64 m_stoppingAt = -1;
};

// Whether an empty, unnamed workspace may be removed now. The person's own
// emptying is tidied; a sign-out's, Table's open tabs and a workspace a
// bring-back is still filling, or may yet be asked to fill, are not.
[[nodiscard]] inline bool mayDissolveWorkspace(const QString &desktopId, bool tableOpen,
    bool sessionEnding, const QSet<QString> &awaitedByBringBack)
{
    if (tableOpen || sessionEnding) return false;
    return !awaitedByBringBack.contains(desktopId);
}
} // namespace Kadunce
