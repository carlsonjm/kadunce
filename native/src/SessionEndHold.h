/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QSet>
#include <QString>

namespace Kadunce
{
// Whether the session may be ending, read from KWin's session states. Plasma
// saves the session, steps back to normal for a few milliseconds and then
// quits, and closes every window while it quits; a cancel steps back to
// normal for good. A workspace emptied in that time was not emptied by the
// person, so nothing is tidied away from the first state that is not normal
// until the session has stayed normal for `GoesOnMs`.
class SessionEndHold
{
public:
    static constexpr qint64 GoesOnMs = 2000;

    void noteState(bool normal, qint64 nowMs)
    {
        if (!normal) {
            m_ending = true;
            m_normalSince = -1;
            return;
        }
        if (m_ending && m_normalSince < 0) m_normalSince = nowMs;
    }

    // True while windows may be closing for a sign-out.
    [[nodiscard]] bool holds(qint64 nowMs)
    {
        if (!m_ending) return false;
        if (m_normalSince >= 0 && nowMs - m_normalSince >= GoesOnMs) {
            m_ending = false;
            m_normalSince = -1;
            return false;
        }
        return true;
    }

    // True once a state other than normal has been seen and not yet outlived.
    [[nodiscard]] bool ending() const { return m_ending; }

private:
    bool m_ending = false;
    qint64 m_normalSince = -1;
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
