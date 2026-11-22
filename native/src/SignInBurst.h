/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>

namespace Kadunce
{
// The windows a session brings back as it starts. Plasma reopens the apps
// left open at sign-out a few hundred milliseconds apart, just after the
// splash goes, and each one taken in as the Active card would show full
// screen in turn. Those arriving close together at the start join one burst
// and wait out of sight; once no more have come for `QuietMs`, or the burst
// has run `LongestMs`, they all become cards at once and one is shown. A
// window opened later, or after a gap, is the person's own and is shown as
// it comes.
class SignInBurst
{
public:
    // Arrivals only start a burst this soon after the session starts.
    static constexpr qint64 OpeningMs = 20000;
    static constexpr qint64 QuietMs = 1500;
    static constexpr qint64 LongestMs = 8000;

    // A window arrived `nowMs` after the session started. True when it is
    // part of the burst.
    [[nodiscard]] bool arrive(qint64 nowMs)
    {
        if (m_over) return false;
        if (m_firstAt < 0) {
            if (nowMs > OpeningMs) return end();
            m_firstAt = nowMs;
        } else if (nowMs - m_lastAt > QuietMs || nowMs - m_firstAt > LongestMs) {
            return end();
        }
        m_lastAt = nowMs;
        ++m_count;
        return true;
    }

    // When the burst settles, on the same clock; negative before any arrival
    // and once it is over.
    [[nodiscard]] qint64 settlesAt() const
    {
        if (m_over || m_firstAt < 0) return -1;
        return std::min(m_lastAt + QuietMs, m_firstAt + LongestMs);
    }

    // True while windows arriving now would still join it.
    [[nodiscard]] bool open(qint64 nowMs) const
    {
        const qint64 at = settlesAt();
        return at >= 0 && nowMs < at;
    }

    [[nodiscard]] int count() const { return m_count; }
    [[nodiscard]] bool over() const { return m_over; }

    // The burst has been shown; nothing joins it again this session.
    bool end()
    {
        m_over = true;
        return false;
    }

private:
    qint64 m_firstAt = -1;
    qint64 m_lastAt = -1;
    int m_count = 0;
    bool m_over = false;
};
// How many apps a session starting will reopen, from the text of KDE's
// ksmserverrc: none when the person starts with an empty session, else the
// count of the session saved at sign-out or, when they chose so, the one
// they saved by hand.
[[nodiscard]] inline int sessionReopens(const QString &ksmserverrc)
{
    QString group, mode = QStringLiteral("restorePreviousLogout");
    int previous = 0, saved = 0;
    for (const QString &raw : ksmserverrc.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1Char('['))) { group = line; continue; }
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq < 0) continue;
        const QString key = line.left(eq).trimmed(), value = line.mid(eq + 1).trimmed();
        if (group == QLatin1String("[General]") && key == QLatin1String("loginMode")) mode = value;
        else if (key == QLatin1String("count") && group == QLatin1String("[Session: saved at previous logout]")) previous = value.toInt();
        else if (key == QLatin1String("count") && group == QLatin1String("[Session: saved by user]")) saved = value.toInt();
    }
    if (mode == QLatin1String("emptySession")) return 0;
    return std::max(0, mode == QLatin1String("restoreSavedSession") ? saved : previous);
}

// A session that saved apps keeps its glass until the first of them comes,
// up to this long after the session starts: cold apps can take seconds to
// draw their first window after the splash goes.
inline constexpr qint64 SignInReopenAwaitMs = 8000;

} // namespace Kadunce
