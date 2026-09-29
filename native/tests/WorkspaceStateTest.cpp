#include "CardWorkspaceState.h"
#include <QString>
#include <cstdlib>
#include <iostream>
#include <functional>
using namespace Kadunce;
using namespace Qt::StringLiterals;
void require(bool value, const char *message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    // Initial ownership is a complete membership commit, not a sequence of
    // selecting/activating individual faces. Restore owners can enumerate it once.
    for (int selected = 0; selected < 3; ++selected) {
        CardWorkspaceState<QString> entry;
        const QList<QString> original{u"first"_s, u"second"_s, u"third"_s};
        entry.reset(original, selected);
        require(entry.windows() == original && entry.count() == 3
            && entry.selectedWindow() == original[selected], "Initial entry omitted unselected ownership");
        const auto admission = entry.prepareAdmission(u"incoming"_s, false);
        require(admission && !entry.commitAdmission(*admission, [] { return false; })
            && entry.windows() == original, "Rejected adoption changed existing membership");
    }

    // Bento projection is one transactional stack with the large pane selected.
    for (int count = 1; count <= 12; ++count) {
        CardWorkspaceState<QString> stack;
        QList<QString> windows;
        for (int i = 0; i < count; ++i) windows.append(QString::number(i));
        const auto plan = stack.prepareStackAdmission(windows);
        require(plan && !stack.commitAdmission(*plan, [] { return false; })
            && stack.windows().isEmpty(), "Rejected stack import mutated ownership");
        require(stack.commitAdmission(*plan, [] { return true; })
            && stack.count() == 1 && stack.cardCount() == count
            && stack.selectedWindow() == windows.first() && stack.invariantHolds(),
            "Bento import lost membership or selected large pane");
        for (int i = 0; i < count; ++i) {
            require(stack.selectedWindow() == windows[i], "Imported stack order changed");
            stack.pageStack(1);
        }
        const auto order = stack.stackMembersForId(1);
        require(!stack.prepareStackAdmission(windows), "Occupied owner accepted replacement stack");
        if (count > 1) {
            require(stack.detachSelectedMember() && stack.restoreDetachedMember()
                && stack.stackMembersForId(1) == order, "Projection detach/cancel lost stack order");
            require(stack.removeAt(count - 1) && stack.cardCount() == count - 1
                && stack.selectedWindow() == windows.first(), "Projected closure lost lead");
        }
    }
    CardWorkspaceState<QString> duplicateStack;
    require(!duplicateStack.prepareStackAdmission({u"same"_s, u"same"_s}),
        "Duplicate stack identity accepted");

    for (int count = 1; count <= 8; ++count) {
        for (int active = 0; active < count; ++active) {
            for (int depth = 0; depth <= count; ++depth) {
                CardWorkspaceState<QString> state;
                QList<QString> handles;
                for (int i = 0; i <= count; ++i) handles.append(QString::number(i));
                state.reset(handles, 0);
                for (int i = 1; i < count; ++i) {
                    state.page(1);
                    require(state.stackSelectedWith(1), "Depth setup failed");
                }
                state.pageStack(active - state.model().stackActivePositionForId(1));
                const auto members = state.stackMembersForId(1);
                std::vector<int> expected;
                for (int d = 0; d < count; ++d)
                    expected.push_back(members[(active - d + count) % count]);
                state.page(1);
                const auto plan = state.prepareStackInsertionAtDepth(handles[0], depth);
                require(plan && state.commitStackInsertion(*plan), "Depth insertion failed");
                expected.insert(expected.begin() + depth, count + 1);
                const auto committed = state.stackMembersForId(1);
                const int face = state.model().stackActivePositionForId(1);
                for (int d = 0; d <= count; ++d)
                    require(committed[(face - d + count + 1) % (count + 1)] == expected[d],
                        "Front-first visual depth disagrees with committed order");
            }
        }
    }
    for (int slot = 0; slot <= 2; ++slot) {
        CardWorkspaceState<QString> stack;
        stack.reset({u"a"_s, u"b"_s, u"c"_s}, 0);
        require(stack.stackSelectedWith(2), "Insertion setup failed");
        stack.page(1); // c into the two-member a/b group.
        const auto order = stack.stackMembersForId(1);
        const auto revision = stack.revision();
        const auto insertion = stack.prepareStackInsertion(u"a"_s, slot);
        require(insertion.has_value() && stack.revision() == revision
            && stack.stackMembersForId(1) == order, "Preparation mutated stack");
        CardWorkspaceState<QString> other;
        other.reset(stack.windows(), 0);
        require(!other.commitStackInsertion(*insertion), "Foreign insertion accepted");
        require(stack.commitStackInsertion(*insertion), "Prepared insertion rejected");
        auto expected = order;
        expected.insert(expected.begin() + slot, 3);
        require(stack.stackMembersForId(3) == expected && stack.selectedWindow() == u"a"_s,
            "Insertion changed destination selection or placement order");
        require(!stack.commitStackInsertion(*insertion), "Insertion replay accepted");
        stack.pageStack(slot - stack.model().stackActivePositionForId(3));
        require(stack.selectedWindow() == u"c"_s, "Placed member cannot be explicitly selected");
        require(stack.detachSelectedMember(), "Detach for rollback failed");
        const auto retry = stack.prepareStackInsertion(u"a"_s, slot);
        require(retry.has_value(), "Detached insertion not prepared");
        require(stack.restoreDetachedMember(), "Rollback failed");
        require(!stack.commitStackInsertion(*retry) && stack.stackMembersForId(3) == expected,
            "Canceled insertion changed restored order");
        require(!stack.prepareStackInsertion(u"missing"_s, 0)
            && !stack.prepareStackInsertion(u"a"_s, -1)
            && !stack.prepareStackInsertion(u"a"_s, 99), "Invalid insertion accepted");
    }
    CardWorkspaceState<QString> state;
    require(state.windows().isEmpty() && state.invariantHolds(), "Empty registry invalid");
    state.reset({u"a"_s, u"b"_s, u"c"_s}, 0);
    SpreadModel reference(3);
    auto equivalent = [&] {
        require(state.invariantHolds() && state.count() == reference.count()
            && state.cardCount() == reference.cardCount()
            && state.selectedId() == reference.selectedId()
            && state.visibleNeighborhood() == reference.visibleNeighborhood(), "State diverged from baseline model");
        for (int id = 1; id <= reference.cardCount(); ++id)
            require(state.stackMembersForId(id) == reference.stackMembersForId(id)
                && state.stackPaintOrderForId(id) == reference.stackPaintOrderForId(id), "Stack order changed");
    };
    require(state.stackSelectedWith(2) == reference.stackSelectedWith(2), "Stack commit changed");
    equivalent();
    for (int i = 0; i < 100; ++i) {
        state.page(i % 2 ? -1 : 1); reference.page(i % 2 ? -1 : 1);
        state.pageStack(-1); reference.pageStack(-1); equivalent();
    }
    state.selectIndex(0); reference.selectIndex(0);
    require(state.detachSelectedMember() == reference.detachSelectedMember(), "Detach changed");
    equivalent();
    require(state.restoreDetachedMember() == reference.restoreDetachedMember(), "Cancel changed");
    equivalent();
    require(state.append(u"d"_s, true) == reference.appendCenteredCard(), "Centered admission changed");
    equivalent();
    require(state.selectedWindow() == u"d"_s, "New identity not selected");
    require(state.append(u"d"_s, false) == 0 && state.windows().size() == 4, "Duplicate identity admitted");
    require(!state.removeAt(-1) && !state.removeAt(4), "Invalid removal accepted");
    equivalent();
    require(state.removeAt(0) == reference.removeCard(1), "Removal changed");
    equivalent();
    require(state.indexOf(u"a"_s) == -1 && state.selectedWindow() == u"d"_s
        && state.windows().size() == 3, "Renumbering lost selected window identity");
    while (!state.windows().isEmpty()) require(state.removeAt(0), "Last-window removal failed");
    require(state.invariantHolds() && state.selectedWindow().isEmpty(), "Empty registry retained a window");
    require(state.append(u"new"_s, true) == 1 && state.selectedWindow() == u"new"_s,
        "Empty-to-first admission used stale model");
    state.clear();
    state.reset({u"x"_s, u"y"_s}, 1);
    require(state.selectedWindow() == u"y"_s && state.count() == 2
        && state.invariantHolds(), "New session inherited old membership");

    // Prepare against the real state owner, not a separate fake registry.
    state.reset({u"a"_s, u"b"_s, u"c"_s}, 0);
    require(state.stackSelectedWith(2), "Transfer setup stack failed");
    const auto originalOrder = state.stackMembersForId(1);
    const auto originalSelection = state.selectedWindow();
    auto rejected = state.prepareRemoval(u"a"_s);
    require(rejected && state.windows().size() == 3
        && state.stackMembersForId(1) == originalOrder
        && state.selectedWindow() == originalSelection, "Preparation mutated source");
    rejected.reset(); // Destination rejection/cancel requires no source undo.
    require(state.stackMembersForId(1) == originalOrder, "Rejected transfer changed order");
    require(!state.prepareRemoval(u"absent"_s), "Missing card prepared");
    require(state.detachSelectedMember(), "Transfer detach setup failed");
    auto detached = state.prepareRemoval(originalSelection);
    require(detached && state.hasDetachedMember(), "Preparation consumed detach rollback");
    detached.reset();
    require(state.restoreDetachedMember() && state.stackMembersForId(1) == originalOrder,
        "Rejected detached transfer cannot restore original stack");
    auto accepted = state.prepareRemoval(u"a"_s);
    auto duplicate = *accepted;
    CardWorkspaceState<QString> foreign;
    foreign.reset({u"a"_s, u"b"_s, u"c"_s}, 0);
    require(!foreign.commitRemoval(*accepted), "Foreign source accepted ticket");
    require(state.commitRemoval(*accepted) && state.indexOf(u"a"_s) < 0
        && state.invariantHolds(), "Accepted removal failed");
    require(!state.commitRemoval(duplicate), "Duplicate removal committed");

    // Every membership, order or grouping command invalidates prepared source
    // state, even when it happens to leave the same visible selection (ABA
    // protection).
    const std::vector<std::function<void()>> ownershipChanges{
        [&] { state.clear(); },
        [&] { state.reset({u"a"_s, u"b"_s, u"c"_s}, 0); },
        [&] { state.append(u"d"_s, true); },
        [&] { state.removeAt(2); },
        [&] { state.moveSelected(1); },
        [&] { state.stackSelectedWith(2); },
        [&] { state.detachSelectedMember(); },
        [&] { state.restoreDetachedMember(); },
        [&] { state.commitDetachedMember(); },
    };
    for (const auto &change : ownershipChanges) {
        state.reset({u"a"_s, u"b"_s, u"c"_s}, 0);
        auto pending = state.prepareRemoval(u"a"_s);
        auto incoming = state.prepareAdmission(u"incoming"_s, true);
        change();
        const auto current = state.windows();
        require(!state.commitRemoval(*pending) && state.windows() == current
            && state.invariantHolds(), "Stale removal overwrote intervening state");
        int sourceCalls = 0;
        require(!state.commitAdmission(*incoming, [&] { ++sourceCalls; return true; })
            && sourceCalls == 0 && state.windows() == current,
            "Stale destination touched source or replaced state");
    }

    // A reservation is held across the user's own browsing. Paging, selecting a
    // face and changing the neighbour side are presentation, so they must not
    // void a ticket, and committing one afterwards must leave the view the user
    // is looking at exactly as they left it.
    const std::vector<std::function<void()>> presentationChanges{
        [&] { state.page(1); },
        [&] { state.page(1); state.page(-1); },
        [&] { state.pageStack(1); },
        [&] { state.selectIndex(2); },
        [&] { state.setPairNeighborSide(-1); },
    };
    for (const auto &change : presentationChanges) {
        // Admission held across the user's browsing.
        state.reset({u"a"_s, u"b"_s, u"c"_s}, 0);
        require(state.stackSelectedWith(2), "Presentation setup stack failed");
        auto incoming = state.prepareAdmission(u"incoming"_s, false);
        require(incoming.has_value(), "Admission reservation was not prepared");
        change();
        // The browsing state the held ticket must not disturb: the grouped
        // stack's order and visible face, plus the neighbour side.
        int grouped = state.indexOf(u"a"_s) + 1;
        const auto members = state.stackMembersForId(grouped);
        const int face = state.stackActivePositionForId(grouped);
        int side = state.pairNeighborSide();
        const auto ownership = state.ownershipRevision();
        require(state.commitAdmission(*incoming, [] { return true; }),
            "Ordinary paging or selection voided a held admission");
        require(state.indexOf(u"incoming"_s) >= 0 && state.invariantHolds(),
            "Committed admission did not publish the newcomer");
        require(state.ownershipRevision() != ownership,
            "Committed admission did not advance the ownership revision");
        // A snapshot payload would have written the prepare-time model back and
        // discarded the browsing above. An intent payload cannot.
        grouped = state.indexOf(u"a"_s) + 1;
        require(state.stackMembersForId(grouped) == members
            && state.stackActivePositionForId(grouped) == face
            && state.pairNeighborSide() == side,
            "Committed admission reverted the live stack face or neighbour side");

        // Removal held across the same browsing, against its own reservation.
        state.reset({u"a"_s, u"b"_s, u"c"_s}, 0);
        require(state.stackSelectedWith(2), "Presentation setup stack failed");
        auto pending = state.prepareRemoval(u"c"_s);
        require(pending.has_value(), "Removal reservation was not prepared");
        change();
        side = state.pairNeighborSide();
        require(state.commitRemoval(*pending) && state.indexOf(u"c"_s) < 0
            && state.invariantHolds(),
            "Ordinary paging or selection voided a held removal");
        require(state.pairNeighborSide() == side,
            "Committed removal reverted the live neighbour side");
    }

    // A presentation change does not license a stale ticket: the membership
    // guard still applies underneath it.
    state.reset({u"a"_s, u"b"_s, u"c"_s}, 0);
    {
        auto pending = state.prepareRemoval(u"a"_s);
        state.page(1);
        state.append(u"d"_s, false);
        state.page(-1);
        const auto current = state.windows();
        require(!state.commitRemoval(*pending) && state.windows() == current,
            "Paging concealed a membership change from a held removal");
    }
    // A membership ticket names which of the six directed transitions it
    // performs, so the owner a commit produces is stated rather than inferred.
    // Grouping is not one of them: a card joining a stack stays a card.
    {
        state.reset({u"a"_s, u"b"_s, u"c"_s}, 0);
        const auto release = state.prepareRemoval(u"a"_s);
        require(release && release->transition() == OwnershipTransition::ReleaseCard,
            "A removal did not default to releasing a card to Plasma");
        const auto toBento = state.prepareRemoval(u"a"_s, OwnershipTransition::CardToBento);
        require(toBento && toBento->transition() == OwnershipTransition::CardToBento,
            "A removal into Bento did not carry its transition");

        const auto admit = state.prepareAdmission(u"incoming"_s, false);
        require(admit && admit->transition() == OwnershipTransition::AdmitToCard,
            "An admission did not default to adopting a native window");
        const auto fromBento = state.prepareAdmission(u"incoming"_s, false,
            OwnershipTransition::BentoToCard);
        require(fromBento && fromBento->transition() == OwnershipTransition::BentoToCard,
            "An admission from Bento did not carry its transition");
        // A whole Bento composition entering Spread is the same directed step.
        CardWorkspaceState<QString> importer;
        const auto imported = importer.prepareStackAdmission({u"p"_s, u"q"_s});
        require(imported && imported->transition() == OwnershipTransition::BentoToCard,
            "A Bento import did not carry the BentoToCard transition");
        // Naming the transition changes nothing about what a commit does.
        require(state.commitRemoval(*release) && state.indexOf(u"a"_s) < 0,
            "A transition-typed removal stopped committing");
    }

    state.reset({u"only"_s}, 0);
    auto last = state.prepareRemoval(u"only"_s);
    require(state.commitRemoval(*last) && state.windows().isEmpty()
        && state.invariantHolds(), "Last-card prepared removal failed");
    std::optional<CardWorkspaceState<QString>::PreparedRemoval> expired;
    {
        CardWorkspaceState<QString> oldSource;
        oldSource.reset({u"only"_s}, 0);
        expired = oldSource.prepareRemoval(u"only"_s);
    }
    state.reset({u"only"_s}, 0);
    require(!state.commitRemoval(*expired), "Destroyed source ticket accepted");

    // The same append semantics serve immediate and prepared admissions.
    for (bool centered : {false, true}) {
        for (int size : {0, 1, 2, 3}) {
            CardWorkspaceState<QString> destination, expected, source;
            QList<QString> members;
            for (int i = 0; i < size; ++i) members.append(QString::number(i));
            destination.reset(members, 0);
            expected.reset(members, 0);
            if (size == 3) {
                require(destination.stackSelectedWith(2) && expected.stackSelectedWith(2),
                    "Admission stack setup failed");
            }
            source.reset({u"incoming"_s}, 0);
            const auto removal = source.prepareRemoval(u"incoming"_s);
            const auto admission = destination.prepareAdmission(u"incoming"_s, centered);
            require(admission && destination.windows() == members
                && source.selectedWindow() == u"incoming"_s, "Preparation changed ownership");
            require(!destination.commitAdmission(*admission, [] { return false; })
                && destination.windows() == members, "Source rejection admitted destination");
            int calls = 0;
            require(destination.commitAdmission(*admission, [&] {
                ++calls;
                require(destination.indexOf(u"incoming"_s) < 0, "Destination published too early");
                return source.commitRemoval(*removal);
            }), "Prepared transfer failed");
            expected.append(u"incoming"_s, centered);
            require(calls == 1 && source.windows().isEmpty() && destination.invariantHolds()
                && destination.windows() == expected.windows()
                && destination.selectedWindow() == expected.selectedWindow()
                && destination.visibleNeighborhood() == expected.visibleNeighborhood(),
                "Prepared admission changed normal append semantics");
            for (int id = 1; id <= destination.cardCount(); ++id)
                require(destination.stackMembersForId(id) == expected.stackMembersForId(id),
                    "Prepared admission changed stack order");
            require(!destination.commitAdmission(*admission, [&] { ++calls; return true; })
                && calls == 1, "Copied/duplicate admission touched source twice");
            require(!destination.prepareAdmission(u"incoming"_s, centered), "Duplicate admission prepared");
        }
    }
    state.reset({u"a"_s, u"b"_s}, 0);
    auto incoming = state.prepareAdmission(u"incoming"_s, true);
    int foreignCalls = 0;
    require(!foreign.commitAdmission(*incoming, [&] { ++foreignCalls; return true; })
        && foreignCalls == 0, "Foreign destination touched source");
    require(state.stackSelectedWith(2) && state.detachSelectedMember(), "Detached admission setup failed");
    require(!state.prepareAdmission(u"incoming"_s, true), "Admission replaced pending carry rollback");
    std::optional<CardWorkspaceState<QString>::PreparedAdmission> expiredAdmission;
    {
        CardWorkspaceState<QString> oldDestination;
        expiredAdmission = oldDestination.prepareAdmission(u"incoming"_s, true);
    }
    require(!state.commitAdmission(*expiredAdmission, [&] { ++foreignCalls; return true; })
        && foreignCalls == 0, "Destroyed destination ticket touched source");

    // CARD-LIFECYCLE.md §2 and §6: a Bento group is one Spread entry beside the
    // individual cards, and selecting it resumes only the group. Both halves are
    // membership, so both are decided here: a group must be able to arrive while
    // the stage already holds cards, and to leave without taking them along.
    {
        CardWorkspaceState<QString> stage;
        const QList<QString> neighbours{u"a"_s, u"b"_s, u"c"_s};
        stage.reset(neighbours, 1);
        const QList<QString> group{u"pane"_s, u"second pane"_s};
        const auto arrival = stage.prepareStackAdmission(group);
        require(arrival && arrival->transition() == OwnershipTransition::BentoToCard,
            "A Bento group could not join a stage that already owns cards");
        require(!stage.commitAdmission(*arrival, [] { return false; })
            && stage.windows() == neighbours, "Refused group arrival changed membership");
        require(stage.commitAdmission(*arrival, [] { return true; }),
            "Group arrival beside existing cards failed");
        require(stage.invariantHolds() && stage.cardCount() == 5 && stage.count() == 4,
            "The arriving group was not one Spread entry beside three cards");
        require(stage.sameStack(4, 5) && !stage.sameStack(1, 4),
            "The group's panes did not arrive as one entry");
        require(stage.stackSizeForId(4) == 2 && stage.windows().mid(0, 3) == neighbours,
            "Group arrival disturbed the cards already owned");

        const auto departure = stage.prepareGroupRemoval(group);
        require(departure && departure->transition() == OwnershipTransition::CardToBento,
            "The group could not be prepared to leave");
        require(stage.windows().size() == 5, "Preparing the departure changed membership");
        require(stage.commitGroupRemoval(*departure), "The group could not leave");
        require(stage.windows() == neighbours && stage.count() == 3
            && stage.cardCount() == 3 && stage.invariantHolds(),
            "Resuming the group released the cards around it");
        require(!stage.commitGroupRemoval(*departure),
            "A committed group departure could be replayed");
    }
    {
        // A departure naming anything the stage does not own changes nothing.
        CardWorkspaceState<QString> stage;
        const QList<QString> members{u"a"_s, u"b"_s};
        stage.reset(members, 0);
        require(!stage.prepareGroupRemoval({u"a"_s, u"stranger"_s}),
            "A departure naming an unowned window was prepared");
        require(!stage.prepareGroupRemoval({u"a"_s, u"a"_s}),
            "A departure naming the same window twice was prepared");
        require(stage.windows() == members, "A refused departure changed membership");
        const auto departure = stage.prepareGroupRemoval(members);
        require(departure && stage.commitGroupRemoval(*departure)
            && stage.windows().isEmpty() && stage.invariantHolds(),
            "A whole-stage departure left membership behind");
    }
}
