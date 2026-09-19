#include "BentoSessionTransfer.h"
#include "BentoLayout.h"
#include <QList>
#include <QString>
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
struct Snapshot { int window; bool valid = true; int restoreMarker = 0; };
struct Session {
    QString outputName;
    QList<int> windows, overflow;
    QList<Snapshot> snapshots;
    std::vector<BentoRect> rects;
};
void require(bool condition, const char *message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    const Session source{QStringLiteral("left"), {1,2}, {}, {{1},{2}}, makeBentoLayout(2,true)};
    const Session destination{QStringLiteral("right"), {3}, {}, {{3}}, makeBentoLayout(1,true)};
    int calls = 0;
    auto planner = [&](Session &session, int required, bool mandatory) {
        ++calls;
        std::vector<BentoCandidate> candidates;
        int arriving = -1;
        for (int i = 0; i < session.snapshots.size(); ++i) {
            candidates.push_back({100,100,500,500,session.snapshots[i].window == required});
            if (session.snapshots[i].window == required) arriving = i;
        }
        auto admission = mandatory ? chooseBentoTransferAdmission(candidates, arriving, 1920,1080)
            : std::optional<BentoAdmission>{chooseBentoAdmission(candidates,1920,1080)};
        if (!admission) return false;
        session.windows.clear(); session.overflow.clear();
        for (int index : admission->candidateIndices) session.windows.append(session.snapshots[index].window);
        for (const auto &snapshot : session.snapshots)
            if (!session.windows.contains(snapshot.window)) session.overflow.append(snapshot.window);
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
    auto overflow = [](Session &session,int,bool) { session.windows.clear(); return true; };
    require(!prepareBentoSessionTransfer(source,destination,1,Snapshot{1},overflow),
        "Overflow mistaken for acceptance");
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
    const Session first{QStringLiteral("empty-monitor"), {}, {}, {}, {}};
    calls = 0;
    auto firstAdmission = prepareBentoAdmission(first, 1, Snapshot{1}, planner);
    require(firstAdmission && calls == 1 && first.snapshots.isEmpty()
        && firstAdmission->windows == QList<int>{1} && firstAdmission->snapshots.size() == 1
        && firstAdmission->rects.size() == 1 && source.windows == QList<int>({1,2}),
        "First layout changed live state or adopted unrelated cards");
    require(!prepareBentoAdmission(first, 1, Snapshot{1}, reject)
        && !prepareBentoAdmission(first, 1, Snapshot{1}, overflow),
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
        && !prepareBentoActivation(first, monitor, 1, true, overflow),
        "Rejected or hidden arrival published");
    auto lostMember = [&](Session &s, int lead, bool required) {
        if (!planner(s, lead, required)) return false;
        s.windows.removeAll(4); return true;
    };
    require(!prepareBentoActivation(first, monitor, 1, true, lostMember),
        "Batch silently lost a display-owned member");
    auto duplicateMember = [&](Session &s, int lead, bool required) {
        if (!planner(s, lead, required)) return false;
        s.overflow.append(lead); return true;
    };
    require(!prepareBentoActivation(first, monitor, 1, true, duplicateMember),
        "Batch duplicated a member across visibility states");
    QList<Snapshot> crowded;
    for (int i = 1; i <= 12; ++i) crowded.append(Snapshot{i});
    const auto crowd = prepareBentoActivation(first, crowded, 12, true, planner);
    require(crowd && crowd->snapshots.size() == 12 && crowd->windows.contains(12)
        && !crowd->overflow.isEmpty()
        && crowd->windows.size() + crowd->overflow.size() == 12,
        "Batch lost overflow membership or hid mandatory arrival");
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
