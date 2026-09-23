/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QLineF>
#include <QPointF>

#include <chrono>
#include <optional>

namespace Kadunce {
// Recognizes a single-finger tap from raw touch events it only observes. A
// contact that travels or lingers is a gesture, not a tap, and a second
// finger makes the whole sequence one.
class KeyboardTap {
public:
    static constexpr double Slop = 16.0;
    static constexpr std::chrono::milliseconds Hold{500};

    void down(qint32 id, const QPointF &position, std::chrono::microseconds time) {
        if (m_contacts++ > 0) {
            m_valid = false;
            return;
        }
        m_id = id;
        m_start = position;
        m_last = position;
        m_startTime = time;
        m_valid = true;
    }
    void motion(qint32 id, const QPointF &position) {
        if (!m_valid || id != m_id) return;
        m_last = position;
        if (QLineF(m_start, m_last).length() > Slop) m_valid = false;
    }
    // The tap's position, when the contact that lifted completes one.
    std::optional<QPointF> up(qint32 id, std::chrono::microseconds time) {
        if (m_contacts > 0) --m_contacts;
        if (!m_valid || id != m_id || m_contacts > 0) {
            if (m_contacts == 0) m_valid = false;
            return std::nullopt;
        }
        m_valid = false;
        if (time - m_startTime > Hold) return std::nullopt;
        return m_last;
    }
    void cancel() { m_contacts = 0; m_valid = false; }

private:
    int m_contacts = 0;
    qint32 m_id = -1;
    QPointF m_start;
    QPointF m_last;
    std::chrono::microseconds m_startTime{0};
    bool m_valid = false;
};
}
