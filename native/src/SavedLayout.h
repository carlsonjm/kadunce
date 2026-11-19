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
// A pane's shape in its layout, in shares of the room its display gives
// panes (BentoRect's own numbers).
struct Shape {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    bool operator==(const Shape &) const = default;
    // A shape a pane can stand in: some room, all of it on the display.
    [[nodiscard]] bool stands() const
    {
        return width > 0.0 && height > 0.0 && x >= -0.0001 && y >= -0.0001
            && x + width <= 1.0001 && y + height <= 1.0001;
    }
    [[nodiscard]] bool overlaps(const Shape &other) const
    {
        return x < other.x + other.width - 0.002 && other.x < x + width - 0.002
            && y < other.y + other.height - 0.002 && other.y < y + height - 0.002;
    }
};

// One group: a lone application, or a Stack's applications front first.
// Where it stood is kept with it: its display by name, and for a pane of a
// layout its shape there. A group kept before places were has neither, and
// stands on the display in use as a card.
struct Group {
    QStringList apps;
    bool stack = false;
    QString display;
    bool pane = false;
    Shape shape;
    // Where its front window was drawn, in shares of its display: for a
    // picture of the desk only, and no part of where anything is put.
    Shape picture;
    // Whether it was the one seen on its display: a pane, the card in use
    // where the display showed a card, a window in view anywhere else.
    bool shown = false;

    bool operator==(const Group &) const = default;
};

// One entry of a desk as it is read: its windows' desktop ids, and where
// the entry stands.
struct Entry {
    QStringList apps;
    QString display;
    bool pane = false;
    Shape shape;
    Shape picture;
    bool shown = false;
};

// No desk has more applications than this; a longer request is cut.
constexpr int MaximumApps = 64;

// The groups of a desk read from its entries, in order. Each entry is a
// lone card, a Stack front first, or a window that is not a card, as the
// desktop ids of its windows; a window with no desktop id reads as empty
// and is left out. An application appears once, in its first group, and a
// Stack left with one application stands alone.
inline QList<Group> deskGroups(const QList<Entry> &entries)
{
    QList<Group> groups;
    QSet<QString> seen;
    for (const auto &entry : entries) {
        Group group;
        group.display = entry.display;
        group.pane = entry.pane && entry.shape.stands();
        if (group.pane) group.shape = entry.shape;
        if (entry.picture.width > 0.0 && entry.picture.height > 0.0) group.picture = entry.picture;
        group.shown = entry.shown;
        for (const auto &app : entry.apps) {
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

inline QList<Group> deskGroups(const QList<QStringList> &entries)
{
    QList<Entry> placed;
    for (const auto &apps : entries) placed.append({apps, QString(), false, {}, {}});
    return deskGroups(placed);
}

inline QVariantMap toVariant(const Group &group)
{
    QVariantMap map{{QStringLiteral("apps"), group.apps}, {QStringLiteral("stack"), group.stack}};
    if (!group.display.isEmpty()) map.insert(QStringLiteral("display"), group.display);
    if (group.pane) {
        map.insert(QStringLiteral("pane"), true);
        map.insert(QStringLiteral("x"), group.shape.x);
        map.insert(QStringLiteral("y"), group.shape.y);
        map.insert(QStringLiteral("width"), group.shape.width);
        map.insert(QStringLiteral("height"), group.shape.height);
    }
    if (group.picture.width > 0.0 && group.picture.height > 0.0) {
        map.insert(QStringLiteral("pictureX"), group.picture.x);
        map.insert(QStringLiteral("pictureY"), group.picture.y);
        map.insert(QStringLiteral("pictureWidth"), group.picture.width);
        map.insert(QStringLiteral("pictureHeight"), group.picture.height);
    }
    if (group.shown) map.insert(QStringLiteral("shown"), true);
    return map;
}

// The groups a requester sent. Blank ids and an application already named
// in an earlier group are dropped, and a group that is not a Stack but names
// several applications stands each of them alone, in order. A group keeps
// its display, and its shape when it is a pane with one it can stand in;
// applications stood alone out of one group share its display and are cards.
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
        group.display = value.value(QStringLiteral("display")).toString().trimmed();
        const Shape shape{value.value(QStringLiteral("x")).toDouble(), value.value(QStringLiteral("y")).toDouble(),
                          value.value(QStringLiteral("width")).toDouble(), value.value(QStringLiteral("height")).toDouble()};
        if (value.value(QStringLiteral("pane")).toBool() && shape.stands()) {
            group.pane = true;
            group.shape = shape;
        }
        const Shape picture{value.value(QStringLiteral("pictureX")).toDouble(), value.value(QStringLiteral("pictureY")).toDouble(),
                            value.value(QStringLiteral("pictureWidth")).toDouble(),
                            value.value(QStringLiteral("pictureHeight")).toDouble()};
        if (picture.width > 0.0 && picture.height > 0.0) group.picture = picture;
        group.shown = value.value(QStringLiteral("shown")).toBool();
        if (group.apps.size() == 1 || (value.value(QStringLiteral("stack")).toBool() && group.apps.size() > 1)) {
            group.stack = group.apps.size() > 1;
            groups.append(group);
            continue;
        }
        for (const auto &app : std::as_const(group.apps)) groups.append({{app}, false, group.display, false, {}, {}});
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

// The shapes a display's panes take: each pane group of `onDisplay` whose
// front window is placed, with its shape. Nothing when no pane group of it
// came, or when two of the shapes would lie over each other, so that a
// layout is stood whole or left as its display laid it out.
struct PaneShape {
    QString windowId;
    Shape shape;

    bool operator==(const PaneShape &) const = default;
};

inline QList<PaneShape> paneShapes(const QList<Group> &groups, const QSet<int> &onDisplay,
                                   const QHash<int, QString> &fronts)
{
    QList<PaneShape> shapes;
    for (int g = 0; g < groups.size(); ++g) {
        if (!onDisplay.contains(g) || !groups.at(g).pane) continue;
        const QString window = fronts.value(g);
        // A pane that never came leaves a hole no other pane was asked to fill.
        if (window.isEmpty()) return {};
        for (const auto &other : std::as_const(shapes))
            if (other.shape.overlaps(groups.at(g).shape)) return {};
        shapes.append({window, groups.at(g).shape});
    }
    return shapes;
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
    [[nodiscard]] const QList<Group> &groups() const { return m_groups; }
    // The window standing in front for a group, once it is placed.
    [[nodiscard]] QString frontOf(int group) const
    {
        const Slot *front = nullptr;
        for (const auto &slot : m_slots)
            if (slot.group == group && slot.state == Slot::State::Placed && (!front || slot.position < front->position))
                front = &slot;
        return front ? front->windowId : QString();
    }
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
    // `only`, when given, names the groups wanted: one display's.
    [[nodiscard]] QList<QStringList> entries(const QSet<int> *only = nullptr) const
    {
        QList<QStringList> result;
        for (int g = 0; g < m_groups.size(); ++g) {
            if (only && !only->contains(g)) continue;
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
