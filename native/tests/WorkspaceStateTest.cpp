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
        require(stack.stackMembersForId(3) == expected && stack.selectedWindow() == u"c"_s,
            "Preview slot/selected identity changed at commit");
        require(!stack.commitStackInsertion(*insertion), "Insertion replay accepted");
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
    CardLineModel reference(3);
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

    // Every semantic command invalidates prepared source state, even when
    // it happens to leave the same visible selection (ABA protection).
    const std::vector<std::function<void()>> changes{
        [&] { state.clear(); },
        [&] { state.reset({u"a"_s, u"b"_s, u"c"_s}, 0); },
        [&] { state.append(u"d"_s, true); },
        [&] { state.removeAt(2); },
        [&] { state.page(1); state.page(-1); },
        [&] { state.pageStack(1); },
        [&] { state.selectIndex(0); },
        [&] { state.moveSelected(1); },
        [&] { state.setPairNeighborSide(-1); },
        [&] { state.stackSelectedWith(2); },
        [&] { state.detachSelectedMember(); },
        [&] { state.restoreDetachedMember(); },
        [&] { state.commitDetachedMember(); },
    };
    for (const auto &change : changes) {
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
}
