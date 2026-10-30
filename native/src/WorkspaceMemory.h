/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "LaunchIdentity.h"
#include "PlacementAim.h"
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <optional>

// Workspace memory: where every application window stood, kept across a
// sign-out so the apps can be brought back to their places, and which apps a
// person opens together after signing in. Values only: no KWin pointers,
// timers or files; the effect reads windows into these and writes them out.
//
// Only what places a window is kept: its application's desktop id, its
// display's name, its workspace's id, whether it was a card or a pane, its
// side, row position and size class, and whether it was in use. No titles,
// document names, paths or workspace names.
namespace Kadunce::Memory
{
enum class Role { Card, Pane, Window };

inline QString roleName(Role role)
{
    switch (role) {
    case Role::Card: return QStringLiteral("card");
    case Role::Pane: return QStringLiteral("pane");
    case Role::Window: break;
    }
    return QStringLiteral("window");
}

inline Role roleNamed(const QString &name)
{
    if (name == QLatin1String("card")) return Role::Card;
    if (name == QLatin1String("pane")) return Role::Pane;
    return Role::Window;
}

struct RememberedWindow {
    QString appId;
    QString output;
    QString desktopId;
    Role role = Role::Window;
    // A pane's side of its display: left, right, top or full.
    QString side;
    // A card's place in its row, from 1; 0 when it has none.
    int order = 0;
    // third, half, twoThirds or full, of its display's width.
    QString size;
    // The card or window in use.
    bool active = false;
    bool minimized = false;

    bool operator==(const RememberedWindow &) const = default;
};

struct RememberedSession {
    // Which sign-in wrote it (logind's session id); a record written by
    // another sign-in is the one offered for bringing back.
    QString sessionId;
    qint64 savedAt = 0;
    // Written as the session ended, not by the last periodic save.
    bool clean = false;
    QList<RememberedWindow> windows;

    bool operator==(const RememberedSession &) const = default;

    // Each application once, in the order its first window stood.
    [[nodiscard]] QStringList apps() const
    {
        QStringList result;
        QSet<QString> seen;
        for (const auto &window : windows) {
            const QString key = LaunchIdentity::normalized(window.appId);
            if (key.isEmpty() || seen.contains(key)) continue;
            seen.insert(key);
            result.append(window.appId);
        }
        return result;
    }
};

// The size class of a width on a display of `areaWidth`.
inline QString sizeClass(double width, double areaWidth)
{
    if (areaWidth <= 0.0 || width <= 0.0) return QStringLiteral("full");
    const double share = width / areaWidth;
    if (share < 0.42) return QStringLiteral("third");
    if (share < 0.58) return QStringLiteral("half");
    if (share < 0.83) return QStringLiteral("twoThirds");
    return QStringLiteral("full");
}

// Which side of its display a pane stands on.
inline QString paneSide(const QRectF &frame, const QRectF &area)
{
    if (area.isEmpty() || frame.isEmpty()) return QStringLiteral("full");
    const bool wide = frame.width() >= area.width() * 0.83;
    if (wide) return frame.height() < area.height() * 0.83 && frame.center().y() < area.center().y()
        ? QStringLiteral("top") : QStringLiteral("full");
    return frame.center().x() < area.center().x() ? QStringLiteral("left") : QStringLiteral("right");
}

inline QJsonObject toJson(const RememberedWindow &window)
{
    QJsonObject object{
        {QStringLiteral("app"), window.appId},
        {QStringLiteral("output"), window.output},
        {QStringLiteral("desktop"), window.desktopId},
        {QStringLiteral("role"), roleName(window.role)},
        {QStringLiteral("size"), window.size},
    };
    if (!window.side.isEmpty()) object.insert(QStringLiteral("side"), window.side);
    if (window.order > 0) object.insert(QStringLiteral("order"), window.order);
    if (window.active) object.insert(QStringLiteral("active"), true);
    if (window.minimized) object.insert(QStringLiteral("minimized"), true);
    return object;
}

inline RememberedWindow windowFromJson(const QJsonObject &object)
{
    RememberedWindow window;
    window.appId = object.value(QStringLiteral("app")).toString();
    window.output = object.value(QStringLiteral("output")).toString();
    window.desktopId = object.value(QStringLiteral("desktop")).toString();
    window.role = roleNamed(object.value(QStringLiteral("role")).toString());
    window.side = object.value(QStringLiteral("side")).toString();
    window.order = object.value(QStringLiteral("order")).toInt();
    window.size = object.value(QStringLiteral("size")).toString();
    window.active = object.value(QStringLiteral("active")).toBool();
    window.minimized = object.value(QStringLiteral("minimized")).toBool();
    return window;
}

inline QJsonObject toJson(const RememberedSession &session)
{
    QJsonArray windows;
    for (const auto &window : session.windows) windows.append(toJson(window));
    return {
        {QStringLiteral("session"), session.sessionId},
        {QStringLiteral("savedAt"), session.savedAt},
        {QStringLiteral("clean"), session.clean},
        {QStringLiteral("windows"), windows},
    };
}

inline RememberedSession sessionFromJson(const QJsonObject &object)
{
    RememberedSession session;
    session.sessionId = object.value(QStringLiteral("session")).toString();
    session.savedAt = object.value(QStringLiteral("savedAt")).toInteger();
    session.clean = object.value(QStringLiteral("clean")).toBool();
    // A record far larger than any desk is not one Kadunce wrote.
    const QJsonArray windows = object.value(QStringLiteral("windows")).toArray();
    for (qsizetype i = 0; i < windows.size() && i < 64; ++i) {
        auto window = windowFromJson(windows.at(i).toObject());
        if (!LaunchIdentity::normalized(window.appId).isEmpty()) session.windows.append(window);
    }
    return session;
}

// The memory file: this sign-in's record and the one offered from the last.
struct MemoryFile {
    static constexpr int Version = 1;
    RememberedSession current;
    std::optional<RememberedSession> offered;

    [[nodiscard]] QByteArray encode() const
    {
        QJsonObject object{
            {QStringLiteral("version"), Version},
            {QStringLiteral("current"), toJson(current)},
        };
        if (offered) object.insert(QStringLiteral("offered"), toJson(*offered));
        return QJsonDocument(object).toJson(QJsonDocument::Indented);
    }

    // An unreadable or newer file is treated as none.
    static MemoryFile decode(const QByteArray &bytes)
    {
        MemoryFile file;
        const QJsonObject object = QJsonDocument::fromJson(bytes).object();
        if (object.value(QStringLiteral("version")).toInt() != Version) return file;
        file.current = sessionFromJson(object.value(QStringLiteral("current")).toObject());
        if (object.contains(QStringLiteral("offered")))
            file.offered = sessionFromJson(object.value(QStringLiteral("offered")).toObject());
        return file;
    }

    // Cards starting in `sessionId`: a record another sign-in wrote becomes
    // the one offered, and this sign-in starts its own. Cards switched off and
    // on again in the same sign-in keep both as they were.
    void begin(const QString &sessionId)
    {
        if (!sessionId.isEmpty() && current.sessionId == sessionId) return;
        if (!current.sessionId.isEmpty() || !current.windows.isEmpty()) offered = current;
        current = RememberedSession{};
        current.sessionId = sessionId;
    }

    [[nodiscard]] bool hasOffer() const { return offered && !offered->windows.isEmpty(); }
};

// One remembered app's way back: the place its first window goes to, after
// the displays and workspaces that are gone have fallen back to the ones in
// use.
struct BringBackStep {
    QString appId;
    QString output;
    QString desktopId;
    Role role = Role::Card;
    QString side;
    int order = 0;
    bool active = false;
    // Panes standing together on one display of one workspace come back
    // together; empty for anything else.
    QString group;

    bool operator==(const BringBackStep &) const = default;
};

struct BringBackPlan {
    QList<BringBackStep> steps;
    // Desktop ids the requester launches, each once.
    QStringList launch;
    QStringList alreadyOpen;
    // The workspaces it fills, kept while it waits.
    QSet<QString> desktops;
};

inline bool namesApp(const QStringList &apps, const QString &appId)
{
    return std::any_of(apps.cbegin(), apps.cend(),
                       [&](const QString &app) { return LaunchIdentity::matches(app, appId); });
}

// How `session` comes back on the displays and workspaces there are now.
// An app already running stays where it is. A display that is gone falls
// back to `currentOutput`, a workspace that is gone to `currentDesktop`. A
// pane whose partners do not come back with it, or that stood on a workspace
// not shown now, stands as a card.
inline BringBackPlan planBringBack(const RememberedSession &session, const QStringList &outputs,
    const QStringList &desktops, const QString &currentOutput, const QString &currentDesktop,
    const QStringList &running)
{
    BringBackPlan plan;
    QSet<QString> taken;
    for (const auto &window : session.windows) {
        const QString key = LaunchIdentity::normalized(window.appId);
        if (key.isEmpty() || taken.contains(key)) continue;
        taken.insert(key);
        if (namesApp(running, window.appId)) {
            plan.alreadyOpen.append(window.appId);
            continue;
        }
        BringBackStep step;
        step.appId = window.appId;
        step.output = outputs.contains(window.output) ? window.output : currentOutput;
        step.desktopId = desktops.contains(window.desktopId) ? window.desktopId : currentDesktop;
        step.role = window.role == Role::Window ? Role::Window : window.role;
        step.side = window.role == Role::Pane ? window.side : QString();
        step.order = window.order;
        step.active = window.active;
        // A pair is rebuilt on the workspace shown; on another, its panes
        // wait there as cards.
        if (step.role == Role::Pane && step.desktopId == currentDesktop)
            step.group = step.desktopId + QLatin1Char('|') + step.output;
        plan.steps.append(step);
        plan.launch.append(window.appId);
        if (!step.desktopId.isEmpty()) plan.desktops.insert(step.desktopId);
    }
    QHash<QString, int> groupSizes;
    for (const auto &step : std::as_const(plan.steps))
        if (!step.group.isEmpty()) ++groupSizes[step.group];
    for (auto &step : plan.steps) {
        if (step.role != Role::Pane || groupSizes.value(step.group) > 1) continue;
        step.role = Role::Card;
        step.side.clear();
        step.group.clear();
    }
    return plan;
}

// A window ready to go to its place. `first` is the member of a group that
// goes first and stands alone, so the others pair with it; a group member
// whose partners never came stands as a card.
struct ReadyPlacement {
    BringBackStep step;
    QString windowId;
    bool asCard = false;
};

// The point a placement is aimed at to put a window back: a pane at the
// middle of its side's edge, anything else in the middle of its display, so
// the aim read there (PlacementAim.h) names the place it had. A pane's top
// side on the display that owns cards has no edge of its own, so it comes
// back as a card there.
inline QPointF bringBackPoint(const ReadyPlacement &ready, const QRectF &area, bool ownsCards)
{
    constexpr double inset = 4.0;
    const QPointF middle = area.center();
    if (ready.asCard || ready.step.role != Role::Pane) return middle;
    if (ready.step.side == QLatin1String("left")) return {area.left() + inset, middle.y()};
    if (ready.step.side == QLatin1String("right")) return {area.right() - inset, middle.y()};
    if (ready.step.side == QLatin1String("top") && !ownsCards) return {middle.x(), area.top() + inset};
    return middle;
}

// One bring-back while it waits for its apps' first windows.
class BringBackRun
{
public:
    BringBackRun(QString token, QList<BringBackStep> steps)
        : m_token(std::move(token))
        , m_waiting(std::move(steps))
    {
    }

    [[nodiscard]] const QString &token() const { return m_token; }

    // The step waiting for an arriving window of `appIdentity`, if any.
    [[nodiscard]] std::optional<BringBackStep> peek(const QString &appIdentity) const
    {
        for (const auto &step : m_waiting)
            if (LaunchIdentity::matches(step.appId, appIdentity)) return step;
        return std::nullopt;
    }

    // Takes the step for the app's first window; later windows find none.
    std::optional<BringBackStep> claim(const QString &appIdentity)
    {
        for (qsizetype i = 0; i < m_waiting.size(); ++i) {
            if (!LaunchIdentity::matches(m_waiting.at(i).appId, appIdentity)) continue;
            m_claimed.append(m_waiting.at(i));
            return m_waiting.takeAt(i);
        }
        return std::nullopt;
    }

    // A claimed step's window could not be admitted: it counts as missed.
    void lost(const BringBackStep &step)
    {
        if (m_claimed.removeOne(step)) ++m_missed;
    }

    // A claimed step's window is admitted. Anything not in a group goes at
    // once; a group goes once every member has come, left side first.
    QList<ReadyPlacement> arrived(const BringBackStep &step, const QString &windowId)
    {
        m_claimed.removeOne(step);
        if (step.group.isEmpty()) return {{step, windowId, true}};
        m_held[step.group].append({step, windowId, false});
        const auto inGroup = [&](const BringBackStep &other) { return other.group == step.group; };
        if (std::any_of(m_waiting.cbegin(), m_waiting.cend(), inGroup)
            || std::any_of(m_claimed.cbegin(), m_claimed.cend(), inGroup)) return {};
        return releaseGroup(step.group, false);
    }

    // The wait is over: what never came is missed, and members held for
    // partners that never came stand as cards.
    QList<ReadyPlacement> expire()
    {
        m_missed += m_waiting.size() + m_claimed.size();
        m_waiting.clear();
        m_claimed.clear();
        QList<ReadyPlacement> ready;
        const auto groups = m_held.keys();
        for (const auto &group : groups) ready += releaseGroup(group, true);
        return ready;
    }

    void notePlaced(bool placed)
    {
        if (placed) ++m_placed;
        else ++m_failed;
    }

    [[nodiscard]] bool finished() const
    {
        return m_waiting.isEmpty() && m_claimed.isEmpty() && m_held.isEmpty();
    }
    [[nodiscard]] int placed() const { return m_placed; }
    // Never came, or came and could not be placed.
    [[nodiscard]] int missed() const { return m_missed + m_failed; }
    [[nodiscard]] int waiting() const { return int(m_waiting.size()); }

    // Workspaces it is still filling.
    [[nodiscard]] QSet<QString> desktops() const
    {
        QSet<QString> result;
        for (const auto &step : m_waiting) result.insert(step.desktopId);
        for (const auto &step : m_claimed) result.insert(step.desktopId);
        for (const auto &held : m_held)
            for (const auto &ready : held) result.insert(ready.step.desktopId);
        result.remove(QString());
        return result;
    }

private:
    static int sideRank(const QString &side)
    {
        if (side == QLatin1String("left")) return 0;
        if (side == QLatin1String("top")) return 1;
        if (side == QLatin1String("full")) return 2;
        return 3;
    }

    QList<ReadyPlacement> releaseGroup(const QString &group, bool partial)
    {
        QList<ReadyPlacement> members = m_held.take(group);
        std::stable_sort(members.begin(), members.end(), [](const auto &a, const auto &b) {
            return sideRank(a.step.side) < sideRank(b.step.side);
        });
        for (qsizetype i = 0; i < members.size(); ++i)
            members[i].asCard = i == 0 || (partial && members.size() < 2);
        return members;
    }

    QString m_token;
    QList<BringBackStep> m_waiting;
    // Claimed by an arriving window not yet admitted.
    QList<BringBackStep> m_claimed;
    QHash<QString, QList<ReadyPlacement>> m_held;
    int m_placed = 0;
    int m_failed = 0;
    int m_missed = 0;
};

// The apps a person opens by hand after signing in, gathered from the first
// one's window for `SpanMs`. A sign-in that already had windows when Cards
// started gathers nothing: that is Cards switched on, not a sign-in.
class SignInGathering
{
public:
    static constexpr qint64 SpanMs = 5 * 60 * 1000;

    void begin(bool windowsAlreadyOpen)
    {
        m_open = !windowsAlreadyOpen;
        m_started = -1;
        m_apps.clear();
    }

    void noteOpened(const QString &appId, qint64 nowMs)
    {
        if (!m_open || LaunchIdentity::normalized(appId).isEmpty()) return;
        if (m_started < 0) m_started = nowMs;
        if (nowMs - m_started > SpanMs) return;
        const QString key = LaunchIdentity::normalized(appId);
        if (!m_apps.contains(key)) m_apps.append(key);
    }

    [[nodiscard]] bool gathering() const { return m_open; }
    [[nodiscard]] bool due(qint64 nowMs) const { return m_open && m_started >= 0 && nowMs - m_started > SpanMs; }

    // The set gathered, sorted; empty when fewer than two apps came. It ends
    // the gathering.
    QStringList close()
    {
        m_open = false;
        QStringList apps = m_apps;
        m_apps.clear();
        apps.sort();
        return apps.size() >= 2 ? apps : QStringList{};
    }

private:
    bool m_open = false;
    qint64 m_started = -1;
    QStringList m_apps;
};

struct OpenTogetherSet {
    QStringList apps;
    int count = 0;
    qint64 lastSeen = 0;
};

// How often each set of apps has been opened together after a sign-in.
class OpenTogetherLog
{
public:
    static constexpr int Version = 1;
    static constexpr int MaximumSets = 32;
    static constexpr int MaximumApps = 16;

    void add(QStringList apps, qint64 nowSeconds)
    {
        for (auto &app : apps) app = LaunchIdentity::normalized(app);
        apps.removeAll(QString());
        apps.removeDuplicates();
        apps.sort();
        if (apps.size() < 2 || apps.size() > MaximumApps) return;
        for (auto &set : m_sets) {
            if (set.apps != apps) continue;
            ++set.count;
            set.lastSeen = nowSeconds;
            return;
        }
        m_sets.append({apps, 1, nowSeconds});
        if (m_sets.size() <= MaximumSets) return;
        const auto oldest = std::min_element(m_sets.begin(), m_sets.end(),
            [](const auto &a, const auto &b) { return a.lastSeen < b.lastSeen; });
        m_sets.erase(oldest);
    }

    [[nodiscard]] const QList<OpenTogetherSet> &sets() const { return m_sets; }
    void clear() { m_sets.clear(); }

    [[nodiscard]] QByteArray encode() const
    {
        QJsonArray sets;
        for (const auto &set : m_sets)
            sets.append(QJsonObject{{QStringLiteral("apps"), QJsonArray::fromStringList(set.apps)},
                                    {QStringLiteral("count"), set.count},
                                    {QStringLiteral("lastSeen"), set.lastSeen}});
        return QJsonDocument(QJsonObject{{QStringLiteral("version"), Version}, {QStringLiteral("sets"), sets}})
            .toJson(QJsonDocument::Indented);
    }

    static OpenTogetherLog decode(const QByteArray &bytes)
    {
        OpenTogetherLog log;
        const QJsonObject object = QJsonDocument::fromJson(bytes).object();
        if (object.value(QStringLiteral("version")).toInt() != Version) return log;
        const QJsonArray sets = object.value(QStringLiteral("sets")).toArray();
        for (const auto &value : sets) {
            const QJsonObject set = value.toObject();
            QStringList apps;
            for (const auto &app : set.value(QStringLiteral("apps")).toArray()) apps.append(app.toString());
            const int count = set.value(QStringLiteral("count")).toInt();
            if (apps.size() < 2 || count < 1) continue;
            log.m_sets.append({apps, count, set.value(QStringLiteral("lastSeen")).toInteger()});
            if (log.m_sets.size() >= MaximumSets) break;
        }
        return log;
    }

private:
    QList<OpenTogetherSet> m_sets;
};
} // namespace Kadunce::Memory
