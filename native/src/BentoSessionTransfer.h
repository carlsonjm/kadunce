/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <algorithm>
#include <optional>
#include <utility>

namespace Kadunce {
// CARD-LIFECYCLE.md §5: a Bento session holds exactly the combination it
// shows. There is no hidden remainder, so a solve either shows every window
// the session owns awake or it is not a session. §7's sleeping windows stay
// owned without being shown and are the one thing the visible set may omit.
template<class Session>
bool showsEveryAwakeSnapshot(const Session &session)
{
    decltype(session.windows.size()) awake = 0;
    for (const auto &snapshot : session.snapshots) {
        const int shown = snapshot.userMinimized ? 0 : 1;
        if (session.windows.count(snapshot.window) != shown) return false;
        awake += shown;
    }
    return session.windows.size() == awake;
}

// CARD-LIFECYCLE.md §5: Bento ends when it falls to one visible pane, and the
// pane it ends on becomes an individual card. Ending gives everything the
// session holds to card ownership, so a session holding anything it is not
// showing is not one that can end: §7's sleeping window has no card to become
// yet, and any other unshown snapshot is the stranded state applySession
// reports rather than one to discard by ending.
template<class Session>
bool bentoEndsAtOnePane(const Session &session)
{
    return session.windows.size() <= 1
        && session.snapshots.size() == session.windows.size();
}

// Prepare first activation from a caller-collected display-owned batch. The
// collector owns eligibility/output policy and captures restore records before
// calling this. The planner changes placement only, never snapshots/native state.
// An edge arrival must be included exactly once and requirePreferred must be true.
// Ordinary shortcut activation retains the existing optional-preferred policy.
template<class Session, class Snapshots, class Handle, class Planner>
std::optional<Session> prepareBentoActivation(const Session &empty,
    const Snapshots &snapshots, const Handle &preferred, bool requirePreferred, Planner plan)
{
    if (empty.outputName.isEmpty() || !empty.snapshots.isEmpty()
        || !empty.windows.isEmpty() || !empty.rects.empty()
        || snapshots.isEmpty()) return std::nullopt;
    for (const auto &snapshot : snapshots) {
        if (!snapshot.valid || !snapshot.window
            || std::count_if(snapshots.begin(), snapshots.end(), [&](const auto &item) {
                return item.window == snapshot.window;
            }) != 1) return std::nullopt;
    }
    if (requirePreferred && std::none_of(snapshots.begin(), snapshots.end(),
        [&](const auto &item) { return item.window == preferred; })) return std::nullopt;
    Session result = empty;
    result.snapshots = snapshots;
    if (!plan(result, preferred, requirePreferred) || result.windows.isEmpty()
        || result.rects.size() != static_cast<size_t>(result.windows.size())
        || (requirePreferred && !result.windows.contains(preferred))) return std::nullopt;
    // A batch the planner cannot show in full is refused. The caller shortens
    // it first and gives what it drops to card ownership; nothing is parked.
    if (!showsEveryAwakeSnapshot(result)) return std::nullopt;
    return result;
}

// Merge an authoritative arrival restore record into the destination discovery
// batch. A window already on the monitor must appear only once; discovery must
// not overwrite the sender's restore record with its temporary carry geometry.
template<class Session, class Snapshots, class Handle, class Snapshot, class Planner>
std::optional<Session> prepareBentoActivationWithArrival(const Session &empty,
    const Snapshots &owned, const Handle &window, const Snapshot &arrival, Planner plan)
{
    if (!window || !arrival.valid || arrival.window != window) return std::nullopt;
    if (std::any_of(owned.begin(), owned.end(), [](const auto &item) {
        return !item.valid || !item.window;
    })) return std::nullopt;
    auto batch = owned;
    const auto matches = [&](const auto &item) { return item.window == window; };
    if (std::count_if(batch.begin(), batch.end(), matches) > 1) return std::nullopt;
    auto existing = std::find_if(batch.begin(), batch.end(), matches);
    if (existing == batch.end()) batch.append(arrival);
    else *existing = arrival;
    return prepareBentoActivation(empty, batch, window, true, plan);
}

// Works for both an empty first-layout candidate and an existing layout.
// The caller supplies output/restore policy; this never discovers or adopts
// unrelated desktop windows. Preparation is not permission to publish.
template<class Session, class Handle, class Snapshot, class Planner>
std::optional<Session> prepareBentoAdmission(const Session &destination,
    const Handle &window, const Snapshot &snapshot, Planner plan)
{
    if (destination.outputName.isEmpty() || !snapshot.valid || snapshot.window != window)
        return std::nullopt;
    if (std::any_of(destination.snapshots.begin(), destination.snapshots.end(),
        [&window](const auto &item) { return item.window == window; })) return std::nullopt;
    Session result = destination;
    result.snapshots.append(snapshot);
    // §8: a layout admits by growing to show the arrival beside everything it
    // already shows. One that cannot grow refuses, and the caller makes the
    // arrival an individual card rather than displacing a pane the user placed.
    if (!plan(result, window, true) || !result.windows.contains(window)
        || !showsEveryAwakeSnapshot(result)) return std::nullopt;
    return result;
}

template<class Session, class Handle, class Planner>
std::optional<Session> prepareBentoDeparture(const Session &source, const Handle &window, Planner plan)
{
    const auto matches = [&window](const auto &snapshot) { return snapshot.window == window; };
    if (std::count_if(source.snapshots.begin(), source.snapshots.end(), matches) != 1)
        return std::nullopt;
    Session result = source;
    auto &snapshots = result.snapshots;
    snapshots.erase(std::remove_if(snapshots.begin(), snapshots.end(), matches), snapshots.end());
    result.windows.removeAll(window);
    if (snapshots.isEmpty()) result.rects.clear();
    // No law here on purpose: a departure is how a session that owes an
    // eviction gets back under one, and the first of several would refuse.
    else if (!plan(result, Handle{}, false)) return std::nullopt;
    return result;
}

template<class Session>
struct BentoSessionTransfer {
    Session source;
    Session destination;
};

// Synchronous value preparation only. Planner must not mutate live sessions,
// pump events, or apply native geometry. The caller publishes both values
// together before making any native calls; never retain this across an event.
template<class Session, class Handle, class Snapshot, class Planner>
std::optional<BentoSessionTransfer<Session>> prepareBentoSessionTransfer(
    const Session &source, const Session &destination, const Handle &window,
    const Snapshot &destinationSnapshot, Planner plan)
{
    if (source.outputName == destination.outputName || !destinationSnapshot.valid
        || destinationSnapshot.window != window) return std::nullopt;
    const auto matches = [&window](const auto &snapshot) { return snapshot.window == window; };
    if (std::count_if(source.snapshots.begin(), source.snapshots.end(), matches) != 1
        || std::any_of(destination.snapshots.begin(), destination.snapshots.end(), matches))
        return std::nullopt;
    BentoSessionTransfer<Session> result{source, destination};
    // Require a visible destination before even preparing source removal.
    auto admission = prepareBentoAdmission(destination, window, destinationSnapshot, plan);
    if (!admission) return std::nullopt;
    result.destination = std::move(*admission);
    auto departure = prepareBentoDeparture(source, window, plan);
    if (!departure) return std::nullopt;
    result.source = std::move(*departure);
    return result;
}
} // namespace Kadunce
