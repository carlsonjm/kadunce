#include "BentoSessionTransfer.h"
#include "BentoLayout.h"
#include <QList>
#include <QString>
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
struct Snapshot { int window; bool valid = true; int restoreMarker = 0;
                  bool userMinimized = false; };
struct Session {
    QString outputName;
    QList<int> windows;
    QList<Snapshot> snapshots;
    std::vector<BentoRect> rects;
};
void require(bool condition, const char *message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    const Session source{QStringLiteral("left"), {1,2}, {{1},{2}}, makeBentoLayout(2,true)};
    const Session destination{QStringLiteral("right"), {3}, {{3}}, makeBentoLayout(1,true)};
    int calls = 0;
    auto planner = [&](Session &session, int required, bool mandatory) {
        ++calls;
        std::vector<BentoCandidate> candidates;
        QList<int> awake;
        int arriving = -1;
        // §7's sleeping windows stay owned without being offered a pane, which
        // is what planSession's own membership filter does.
        for (const auto &snapshot : session.snapshots) {
            if (snapshot.userMinimized) continue;
            if (snapshot.window == required) arriving = int(awake.size());
            candidates.push_back({100,100,500,500,snapshot.window == required});
            awake.append(snapshot.window);
        }
        auto admission = mandatory ? chooseBentoTransferAdmission(candidates, arriving, 1920,1080)
            : std::optional<BentoAdmission>{chooseBentoAdmission(candidates,1920,1080)};
        if (!admission) return false;
        session.windows.clear();
        for (int index : admission->candidateIndices) session.windows.append(awake[index]);
        session.rects = admission->rects;
        return true;
    };
    auto accepted = prepareBentoSessionTransfer(source,destination,1,Snapshot{1},planner);
    require(accepted && calls == 2, "Transfer preparation failed");
    require(source.windows == QList<int>({1,2}) && destination.windows == QList<int>({3}),
        "Preparation mutated originals");
    require(accepted->source.windows == QList<int>({2})
        && accepted->destination.windows.contains(1)
        && accepted->source.snapshots.size() == 1
        && accepted->destination.snapshots.size() == 2, "Card lost/duplicated");
    // CARD-LIFECYCLE.md §5: Bento ends when it falls to one visible pane. Two
    // panes is still a layout; one is not, and a departure that empties the
    // session ends it as well.
    require(!bentoEndsAtOnePane(source), "A two-pane layout was ended");
    require(bentoEndsAtOnePane(Session{QStringLiteral("left"), {2}, {{2}},
        makeBentoLayout(1,true)}), "A layout that fell to one pane did not end");
    require(bentoEndsAtOnePane(Session{QStringLiteral("left"), {}, {}, {}}),
        "An emptied layout did not end");
    // §7's sleeping window keeps the session, because ending would have to hand
    // that window to card ownership and it cannot hold a sleeping one yet.
    require(!bentoEndsAtOnePane(Session{QStringLiteral("left"), {2},
        {{2},{1,true,0,true}}, makeBentoLayout(1,true)}),
        "A layout still holding a sleeping window ended");
    // A session holding an awake window it is not showing is stranded, not
    // ended: ending would drop that window rather than give it an owner.
    require(!bentoEndsAtOnePane(Session{QStringLiteral("left"), {}, {{1}}, {}}),
        "A layout ended on a window it was not showing");
    calls = 0;
    auto reject = [&](Session &,int,bool) { ++calls; return false; };
    auto departure = prepareBentoDeparture(source,1,planner);
    require(departure && departure->windows == QList<int>{2}
        && source.windows == QList<int>({1,2}), "Departure must prepare without mutating source");
    require(!prepareBentoDeparture(source,9,planner)
        && !prepareBentoDeparture(source,1,reject), "Invalid departure accepted");
    auto ambiguous = source; ambiguous.snapshots.append(Snapshot{1});
    require(!prepareBentoDeparture(ambiguous,1,planner), "Ambiguous departure accepted");
    calls = 0;
    require(!prepareBentoSessionTransfer(source,destination,1,Snapshot{1},reject)
        && calls == 1, "Source prepared before destination rejection");
    auto showsNothing = [](Session &session,int,bool) { session.windows.clear(); return true; };
    require(!prepareBentoSessionTransfer(source,destination,1,Snapshot{1},showsNothing),
        "A plan that shows nothing was mistaken for acceptance");
    auto rejectSource = [&](Session &session,int window,bool mandatory) {
        return mandatory ? planner(session,window,true) : false;
    };
    require(!prepareBentoSessionTransfer(source,destination,1,Snapshot{1},rejectSource),
        "Source planning failure ignored");
    require(!prepareBentoSessionTransfer(source,source,1,Snapshot{1},planner)
        && !prepareBentoSessionTransfer(source,destination,9,Snapshot{9},planner)
        && !prepareBentoSessionTransfer(source,destination,1,Snapshot{2},planner)
        && !prepareBentoSessionTransfer(source,destination,1,Snapshot{1,false},planner),
        "Invalid transfer accepted");
    auto duplicate = destination; duplicate.snapshots.append(Snapshot{1});
    require(!prepareBentoSessionTransfer(source,duplicate,1,Snapshot{1},planner),
        "Duplicate destination accepted");
    Session last = source; last.windows = {1}; last.snapshots = {{1}};
    const auto lastDeparture = prepareBentoDeparture(last,1,planner);
    require(lastDeparture && lastDeparture->snapshots.isEmpty()
        && lastDeparture->windows.isEmpty() && lastDeparture->rects.empty(),
        "Ordinary-desktop last departure left a source pane");
    auto empty = prepareBentoSessionTransfer(last,destination,1,Snapshot{1},planner);
    require(empty && empty->source.snapshots.isEmpty() && empty->source.windows.isEmpty()
        && empty->source.rects.empty(), "Last-window source not empty");
    require(!prepareBentoSessionTransfer(accepted->source,accepted->destination,1,Snapshot{1},planner),
        "Repeated transfer accepted");

    // First layout: solve a value containing only the explicitly arriving card.
    const Session first{QStringLiteral("empty-monitor"), {}, {}, {}};
    calls = 0;
    auto firstAdmission = prepareBentoAdmission(first, 1, Snapshot{1}, planner);
    require(firstAdmission && calls == 1 && first.snapshots.isEmpty()
        && firstAdmission->windows == QList<int>{1} && firstAdmission->snapshots.size() == 1
        && firstAdmission->rects.size() == 1 && source.windows == QList<int>({1,2}),
        "First layout changed live state or adopted unrelated cards");
    require(!prepareBentoAdmission(first, 1, Snapshot{1}, reject)
        && !prepareBentoAdmission(first, 1, Snapshot{1}, showsNothing),
        "Rejected/hidden first card accepted");
    require(!prepareBentoAdmission(Session{}, 1, Snapshot{1}, planner)
        && !prepareBentoAdmission(first, 1, Snapshot{2}, planner)
        && !prepareBentoAdmission(first, 1, Snapshot{1,false}, planner)
        && !prepareBentoAdmission(*firstAdmission, 1, Snapshot{1}, planner),
        "Invalid first-layout identity or duplicate accepted");
    auto firstTransfer = prepareBentoSessionTransfer(source, first, 1, Snapshot{1}, planner);
    require(firstTransfer && firstTransfer->source.windows == QList<int>{2}
        && firstTransfer->destination.windows == QList<int>{1}
        && first.snapshots.isEmpty(), "First-layout transfer preparation failed");

    // First activation is one display-owned batch, not repeated arrival adds.
    const QList<Snapshot> monitor{{3},{4},{1}};
    calls = 0;
    const auto batch = prepareBentoActivation(first, monitor, 1, true, planner);
    require(batch && calls == 1 && batch->snapshots.size() == 3
        && batch->windows.size() == batch->snapshots.size()
        && batch->windows.contains(1) && batch->windows.contains(3) && batch->windows.contains(4)
        && first.snapshots.isEmpty() && source.windows == QList<int>({1,2})
        && monitor.size() == 3, "Batch must prepare once without source/destination mutation");
    const auto lone = prepareBentoActivation(first, QList<Snapshot>{{1}}, 1, true, planner);
    require(lone && lone->snapshots.size() == 1 && lone->windows == QList<int>{1}
        && lone->rects.size() == 1, "Lone Bento membership lost");
    require(!prepareBentoActivation(first, QList<Snapshot>{}, 1, true, planner)
        && !prepareBentoActivation(Session{}, monitor, 1, true, planner)
        && !prepareBentoActivation(destination, monitor, 1, true, planner)
        && !prepareBentoActivation(first, QList<Snapshot>{{1},{1}}, 1, true, planner)
        && !prepareBentoActivation(first, QList<Snapshot>{{1},{0}}, 1, true, planner)
        && !prepareBentoActivation(first, QList<Snapshot>{{1},{4,false}}, 1, true, planner)
        && !prepareBentoActivation(first, monitor, 9, true, planner),
        "Invalid activation batch accepted");
    require(!prepareBentoActivation(first, monitor, 1, true, reject)
        && !prepareBentoActivation(first, monitor, 1, true, showsNothing),
        "Rejected or hidden arrival published");
    auto lostMember = [&](Session &s, int lead, bool required) {
        if (!planner(s, lead, required)) return false;
        s.windows.removeAll(4); return true;
    };
    require(!prepareBentoActivation(first, monitor, 1, true, lostMember),
        "Batch silently lost a display-owned member");
    auto duplicateMember = [&](Session &s, int lead, bool required) {
        if (!planner(s, lead, required)) return false;
        s.windows.append(lead); return true;
    };
    require(!prepareBentoActivation(first, monitor, 1, true, duplicateMember),
        "Batch duplicated a member");
    // A batch the planner cannot show in full is refused rather than parked:
    // twelve windows against a curated cap of eight has no hidden remainder to
    // put the other four in. The caller shortens the batch and gives what it
    // drops to card ownership, and the shortened batch shows in full.
    QList<Snapshot> crowded;
    for (int i = 1; i <= 12; ++i) crowded.append(Snapshot{i});
    require(!prepareBentoActivation(first, crowded, 12, true, planner),
        "A batch the layout cannot show in full was accepted");
    QList<Snapshot> shortened;
    for (int i = 1; i <= BentoCuratedPaneCap; ++i) shortened.append(Snapshot{i});
    const auto fitted = prepareBentoActivation(first, shortened, BentoCuratedPaneCap,
        true, planner);
    require(fitted && fitted->windows.size() == fitted->snapshots.size()
        && fitted->windows.size() == BentoCuratedPaneCap
        && fitted->windows.contains(BentoCuratedPaneCap),
        "Pre-shortened batch refused or lost a member");

    // §7: a sleeping window is owned without being shown, so it is the one
    // thing the visible combination may omit, and it is not an eviction.
    QList<Snapshot> withSleeper{{1},{2},{3,true,0,true}};
    const auto sleeping = prepareBentoActivation(first, withSleeper, 1, true, planner);
    require(sleeping && sleeping->snapshots.size() == 3
        && sleeping->windows == QList<int>({1,2}),
        "A sleeping window was mistaken for a member of the visible combination");

    // §8: admission grows the layout or refuses. A destination already at the
    // cap cannot take a ninth window, and the caller makes it a card instead.
    require(fitted && !prepareBentoAdmission(*fitted, 99, Snapshot{99}, planner),
        "Growth-only admission accepted a window the layout cannot show");
    auto yielded = *fitted;
    yielded.windows.removeAll(BentoCuratedPaneCap);
    yielded.snapshots.removeIf([](const auto &saved) {
        return saved.window == BentoCuratedPaneCap; });
    const auto grown = prepareBentoAdmission(yielded, 99, Snapshot{99}, planner);
    require(grown && grown->windows.size() == grown->snapshots.size()
        && grown->windows.contains(99),
        "Admission refused a batch the caller had already shortened");
    require(prepareBentoActivation(first, monitor, 9, false, planner).has_value(),
        "Shortcut activation incorrectly requires an off-output preferred window");
    const QList<Snapshot> residents{{3,true,30},{4,true,40}};
    const auto arrivingBatch = prepareBentoActivationWithArrival(first, residents, 1,
        Snapshot{1,true,99}, planner);
    require(arrivingBatch && arrivingBatch->snapshots.size() == 3
        && arrivingBatch->snapshots.last().restoreMarker == 99
        && residents.size() == 2 && first.snapshots.isEmpty(),
        "Arrival merge must preserve residents and use the authoritative restore record");
    auto alreadyResident = residents;
    alreadyResident.append(Snapshot{1,true,12});
    const auto merged = prepareBentoActivationWithArrival(first, alreadyResident, 1,
        Snapshot{1,true,99}, planner);
    require(merged && merged->snapshots.size() == 3
        && merged->snapshots.last().restoreMarker == 99
        && merged->snapshots.first().restoreMarker == 30
        && alreadyResident.last().restoreMarker == 12,
        "Same-output arrival duplicated or original snapshot changed during prepare");
    alreadyResident.append(Snapshot{1});
    require(!prepareBentoActivationWithArrival(first, alreadyResident, 1, Snapshot{1}, planner)
        && !prepareBentoActivationWithArrival(first, residents, 1, Snapshot{2}, planner)
        && !prepareBentoActivationWithArrival(first, residents, 1, Snapshot{1,false}, planner)
        && !prepareBentoActivationWithArrival(first, residents, 1, Snapshot{1}, reject),
        "Ambiguous, invalid or infeasible arrival batch accepted");
}
