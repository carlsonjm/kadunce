/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "CarrySession.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace Kadunce {

// Observed held contacts, NOT proof that one initiated a native window move.
// The platform adapter must correlate the move before adopting a candidate.
// Device IDs are adapter-assigned lifetime tokens, never reusable raw addresses.
// A platform with seat-scoped touch IDs must supply an explicit seat token.
class CarryContacts {
public:
    struct Contact {
        CarryOwner owner;
        QPointF position;
        bool trustworthy = true;
    };
    class Candidate {
    public:
        CarryOwner owner() const { return m_owner; }
    private:
        friend class CarryContacts;
        CarryOwner m_owner;
        quint64 m_revision;
        std::weak_ptr<const int> m_identity;
        Candidate(CarryOwner owner, quint64 revision, const std::shared_ptr<const int> &identity)
            : m_owner(owner), m_revision(revision), m_identity(identity) {}
    };

    CarryContacts() = default;
    CarryContacts(const CarryContacts &) = delete;
    CarryContacts &operator=(const CarryContacts &) = delete;

    bool press(CarryOwner owner, QPointF position)
    {
        // Duplicate/invalid downs invalidate reservations too: missing events
        // must never silently preserve an apparently trustworthy candidate.
        ++m_revision;
        if (!validOwner(owner) || !finite(position)) return false;
        auto found = find(owner);
        if (found != m_contacts.end()) {
            found->trustworthy = false;
            return false;
        }
        m_contacts.push_back({owner, position});
        return true;
    }
    bool motion(CarryOwner owner, QPointF position)
    {
        auto found = find(owner);
        if (found == m_contacts.end()) return false; // Never invent a missing down.
        if (!finite(position)) {
            ++m_revision;
            found->trustworthy = false;
            return false;
        }
        found->position = position;
        return true;
    }
    void release(CarryOwner owner)
    {
        ++m_revision;
        std::erase_if(m_contacts, [&](const Contact &c) { return c.owner == owner; });
    }
    void cancel(CarryDevice kind)
    {
        ++m_revision;
        std::erase_if(m_contacts, [&](const Contact &c) { return c.owner.kind == kind; });
    }
    void removeDevice(quint64 device)
    {
        ++m_revision;
        std::erase_if(m_contacts, [&](const Contact &c) { return c.owner.device == device; });
    }
    void clear()
    {
        ++m_revision;
        m_contacts.clear();
    }
    std::optional<Candidate> soleCandidate() const
    {
        if (m_contacts.size() != 1 || !m_contacts.front().trustworthy) return std::nullopt;
        return Candidate(m_contacts.front().owner, m_revision, m_identity);
    }
    std::optional<Contact> resolve(const Candidate &candidate) const
    {
        if (candidate.m_identity.lock() != m_identity || candidate.m_revision != m_revision
            || m_contacts.size() != 1 || !m_contacts.front().trustworthy
            || m_contacts.front().owner != candidate.m_owner)
            return std::nullopt;
        return m_contacts.front();
    }

private:
    static bool finite(QPointF p) { return std::isfinite(p.x()) && std::isfinite(p.y()); }
    static bool validOwner(CarryOwner owner)
    {
        return owner.device != 0
            && ((owner.kind == CarryDevice::Touch && owner.contact >= 0)
                || (owner.kind == CarryDevice::Pointer && owner.contact > 0));
    }
    auto find(CarryOwner owner) -> std::vector<Contact>::iterator
    {
        return std::find_if(m_contacts.begin(), m_contacts.end(),
                            [&](const Contact &c) { return c.owner == owner; });
    }
    const std::shared_ptr<const int> m_identity = std::make_shared<const int>(0);
    quint64 m_revision = 0;
    std::vector<Contact> m_contacts;
};
} // namespace Kadunce
