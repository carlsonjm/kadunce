/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "LaunchIdentity.h"
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <algorithm>
#include <optional>

// Saved layouts: a named desk a driving program keeps, made of groups of
// applications on one workspace, each standing alone or as one Stack.
// Kadunce reads the desk it shows into groups, and arranges a desk from
// groups as the requester launches what is not running. Values only: no
// KWin pointers or timers; the effect reads windows into these.
namespace Kadunce::SavedLayouts
{
// One group: a lone application, or a Stack's applications front first.
struct Group {
    QStringList apps;
    bool stack = false;

    bool operator==(const Group &) const = default;
};

// No desk has more applications than this; a longer request is cut.
constexpr int MaximumApps = 64;

// The groups of a desk read from its entries, in order. Each entry is a
// lone card, a Stack front first, or a window that is not a card, as the
// desktop ids of its windows; a window with no desktop id reads as empty
// and is left out. An application appears once, in its first group, and a
// Stack left with one application stands alone.
inline QList<Group> deskGroups(const QList<QStringList> &entries)
{
    QList<Group> groups;
    QSet<QString> seen;
    for (const auto &entry : entries) {
        Group group;
        for (const auto &app : entry) {
            const QString key = LaunchIdentity::normalized(app);
            if (key.isEmpty() || seen.contains(key)) continue;
            seen.insert(key);
            group.apps.append(app);
        }
        if (group.apps.isEmpty()) continue;
        group.stack = group.apps.size() > 1;
        groups.append(group);
    }
    return groups;
}

inline QVariantMap toVariant(const Group &group)
{
    return {{QStringLiteral("apps"), group.apps}, {QStringLiteral("stack"), group.stack}};
}

// The groups a requester sent. Blank ids and an application already named
// in an earlier group are dropped, and a group that is not a Stack but names
// several applications stands each of them alone, in order.
inline QList<Group> groupsFrom(const QList<QVariantMap> &requested)
{
    QList<Group> groups;
    QSet<QString> seen;
    int count = 0;
    for (const auto &value : requested) {
        Group group;
        for (const auto &app : value.value(QStringLiteral("apps")).toStringList()) {
            const QString key = LaunchIdentity::normalized(app);
            if (key.isEmpty() || seen.contains(key) || count >= MaximumApps) continue;
            seen.insert(key);
            ++count;
            group.apps.append(app.trimmed());
        }
        if (group.apps.isEmpty()) continue;
        if (value.value(QStringLiteral("stack")).toBool() && group.apps.size() > 1) {
            group.stack = true;
            groups.append(group);
            continue;
        }
        for (const auto &app : std::as_const(group.apps)) groups.append({{app}, false});
    }
    return groups;
}

// An application window already open, as the requester's desk is matched
// against the windows there are.
struct OpenWindow {
    QString appId;
    QString windowId;
};

// One application of the layout and the window that stands for it.
struct Slot {
    enum class State {
        // Waiting for the requester to launch it.
        Waiting,
        // Its first window has opened and is not yet admitted.
        Arriving,
        // A window to bring to its place.
        Ready,
        Placed,
        // Its window came and could not be placed.
        Failed,
        // Never came.
        Missed,
    };

    QString appId;
    int group = 0;
    // Its place in its group's Stack, from the front.
    int position = 0;
    QString windowId;
    State state = State::Waiting;

    bool operator==(const Slot &) const = default;
};

struct Plan {
    QList<Group> groups;
    QList<Slot> places;
    // Desktop ids the requester launches, each once.
    QStringList launch;
    QStringList alreadyOpen;
};

// How `groups` is arranged from the windows open now, `open` front-most
// first on any workspace. An application with a window open uses its
// front-most one; any other is launched and waited for.
inline Plan planArrangement(const QList<Group> &groups, const QList<OpenWindow> &open)
{
    Plan plan;
    plan.groups = groups;
    QSet<QString> taken;
    for (int g = 0; g < groups.size(); ++g) {
        for (int position = 0; position < groups.at(g).apps.size(); ++position) {
            Slot slot;
            slot.appId = groups.at(g).apps.at(position);
            slot.group = g;
            slot.position = position;
            for (const auto &window : open) {
                if (window.windowId.isEmpty() || taken.contains(window.windowId)
                    || !LaunchIdentity::matches(slot.appId, window.appId)) continue;
                slot.windowId = window.windowId;
                slot.state = Slot::State::Ready;
                taken.insert(window.windowId);
                break;
            }
            (slot.state == Slot::State::Ready ? plan.alreadyOpen : plan.launch).append(slot.appId);
            plan.places.append(slot);
        }
    }
    return plan;
}

// One arrangement while it brings its windows to their places.
class LayoutRun
{
public:
    LayoutRun(QString token, Plan plan)
        : m_token(std::move(token))
        , m_groups(std::move(plan.groups))
        , m_slots(std::move(plan.places))
    {
    }

    [[nodiscard]] const QString &token() const { return m_token; }
    [[nodiscard]] const QList<Slot> &places() const { return m_slots; }

    // A launched application's first window has opened: it stands for its
    // application, and later windows find nothing to claim.
    bool claim(const QString &appIdentity, const QString &windowId)
    {
        if (windowId.isEmpty() || holds(windowId)) return false;
        for (auto &slot : m_slots) {
            if (slot.state != Slot::State::Waiting || !LaunchIdentity::matches(slot.appId, appIdentity)) continue;
            slot.windowId = windowId;
            slot.state = Slot::State::Arriving;
            return true;
        }
        return false;
    }

    // A claimed window is admitted and can go to its place.
    bool admitted(const QString &windowId)
    {
        Slot *slot = find(windowId);
        if (!slot || slot->state != Slot::State::Arriving) return false;
        slot->state = Slot::State::Ready;
        return true;
    }

    // The windows still to bring to their places.
    [[nodiscard]] QList<Slot> ready() const
    {
        QList<Slot> result;
        for (const auto &slot : m_slots)
            if (slot.state == Slot::State::Ready) result.append(slot);
        return result;
    }

    // A ready window went to its place, or could not; true the first time,
    // so each application is reported once.
    bool settle(const QString &windowId, bool placed)
    {
        Slot *slot = find(windowId);
        if (!slot || slot->state != Slot::State::Ready) return false;
        slot->state = placed ? Slot::State::Placed : Slot::State::Failed;
        return true;
    }

    // The wait is over, or was cancelled: what has not come is missed.
    // Windows already ready still go to their places.
    void expire()
    {
        for (auto &slot : m_slots)
            if (slot.state == Slot::State::Waiting || slot.state == Slot::State::Arriving)
                slot.state = Slot::State::Missed;
    }

    [[nodiscard]] bool finished() const
    {
        return std::none_of(m_slots.cbegin(), m_slots.cend(), [](const Slot &slot) {
            return slot.state == Slot::State::Waiting || slot.state == Slot::State::Arriving
                || slot.state == Slot::State::Ready;
        });
    }

    [[nodiscard]] int placed() const { return countOf({Slot::State::Placed}); }
    // Never came, or came and could not be placed.
    [[nodiscard]] int missed() const { return countOf({Slot::State::Missed, Slot::State::Failed}); }

    // The placed windows as the row should hold them, group by group: a
    // Stack's windows front first in one entry, a lone window in its own.
    // A Stack with one window placed so far is an entry of one.
    [[nodiscard]] QList<QStringList> entries() const
    {
        QList<QStringList> result;
        for (int g = 0; g < m_groups.size(); ++g) {
            QList<Slot> members;
            for (const auto &slot : m_slots)
                if (slot.group == g && slot.state == Slot::State::Placed) members.append(slot);
            std::sort(members.begin(), members.end(),
                      [](const Slot &a, const Slot &b) { return a.position < b.position; });
            QStringList ids;
            for (const auto &member : std::as_const(members)) ids.append(member.windowId);
            if (ids.isEmpty()) continue;
            if (m_groups.at(g).stack) result.append(ids);
            else for (const auto &id : std::as_const(ids)) result.append(QStringList{id});
        }
        return result;
    }

    [[nodiscard]] std::optional<Slot> slotFor(const QString &windowId) const
    {
        for (const auto &slot : m_slots)
            if (slot.windowId == windowId) return slot;
        return std::nullopt;
    }

private:
    [[nodiscard]] bool holds(const QString &windowId) const
    {
        return std::any_of(m_slots.cbegin(), m_slots.cend(),
                           [&](const Slot &slot) { return slot.windowId == windowId; });
    }

    Slot *find(const QString &windowId)
    {
        for (auto &slot : m_slots)
            if (!windowId.isEmpty() && slot.windowId == windowId) return &slot;
        return nullptr;
    }

    [[nodiscard]] int countOf(std::initializer_list<Slot::State> states) const
    {
        return int(std::count_if(m_slots.cbegin(), m_slots.cend(), [&](const Slot &slot) {
            return std::find(states.begin(), states.end(), slot.state) != states.end();
        }));
    }

    QString m_token;
    QList<Group> m_groups;
    QList<Slot> m_slots;
};
} // namespace Kadunce::SavedLayouts
