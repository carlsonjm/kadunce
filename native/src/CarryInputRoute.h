/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "CarrySession.h"
#include <algorithm>
#include <vector>

namespace Kadunce {
// Physical stream ownership, separate from CarrySession's destination transaction.
// Acquire ONLY after native cancellation, with a still-held, correlated owner.
// An interrupted adoption acquires with active=false to drain without a drop.
// No callbacks: state retires before the caller dispatches the returned action.
class CarryInputRoute {
public:
    enum class Action { Pass, Consume, Move, Release, Cancel };
    bool acquire(CarryOwner owner, bool active = true)
    {
        if (!m_held.empty() || !owner.device || owner.contact < 0
            || (owner.kind != CarryDevice::Pointer && owner.kind != CarryDevice::Touch)
            || (owner.kind == CarryDevice::Pointer && !owner.contact)) return false;
        m_owner = owner;
        m_held.push_back(owner);
        m_active = active;
        return true;
    }
    Action down(CarryOwner contact)
    {
        if (!sameStream(contact)) return Action::Pass;
        if (!contains(contact)) m_held.push_back(contact);
        // A second contact or duplicate down invalidates one-contact intent.
        // Keep every swallowed down until its release, including after cancel.
        return cancel();
    }
    Action motion(CarryOwner contact)
    {
        if (!sameStream(contact)) return Action::Pass;
        return m_active && contact == m_owner ? Action::Move : Action::Consume;
    }
    Action up(CarryOwner contact)
    {
        if (!contains(contact)) return Action::Pass;
        const bool release = m_active && contact == m_owner;
        std::erase(m_held, contact);
        if (contact == m_owner) m_active = false;
        return release ? Action::Release : Action::Consume;
    }
    Action cancel()
    {
        const bool active = m_active;
        m_active = false;
        return active ? Action::Cancel : (m_held.empty() ? Action::Pass : Action::Consume);
    }
    Action streamGone(CarryDevice kind, quint64 device)
    {
        if (m_held.empty() || m_owner.kind != kind || m_owner.device != device) return Action::Pass;
        const bool active = m_active;
        m_active = false;
        m_held.clear(); // Physical cancel/device loss promises no matching release.
        return active ? Action::Cancel : Action::Consume;
    }
    bool active() const { return m_active; }
    bool draining() const { return !m_active && !m_held.empty(); }
    bool busy() const { return !m_held.empty(); }
    CarryOwner owner() const { return m_owner; }
private:
    bool contains(CarryOwner contact) const
    { return std::find(m_held.begin(), m_held.end(), contact) != m_held.end(); }
    bool sameStream(CarryOwner contact) const
    { return busy() && m_owner.kind == contact.kind && m_owner.device == contact.device; }
    CarryOwner m_owner{CarryDevice::Pointer, 0, 0};
    std::vector<CarryOwner> m_held;
    bool m_active = false;
};
} // namespace Kadunce
