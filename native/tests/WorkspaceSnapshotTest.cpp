#include "CardWorkspaceSnapshot.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
using namespace Qt::StringLiterals;
void require(bool value, const char *message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    CardLineModel model(3);
    QStringList ids{u"window-a"_s, u"window-b"_s, u"window-c"_s};
    require(model.stackSelectedWith(2), "Stack setup failed");
    const auto before = makeCardWorkspaceSnapshot(model, ids, true);
    require(before.selectedCardId == ids[0]
        && before.selectedStack == QStringList{ids[1], ids[0]}, "Selected stack order changed");
    const auto *a = before.find(ids[0]);
    require(a && a->cardIndex == 1 && a->stackId == ids[1]
        && a->stackPosition == 2 && a->stackSize == 2 && a->selected,
        "Compatibility fields changed");
    require(!before.find(u"missing"_s) && !before.find({}), "Unknown identity resolved");
    model.pageStack(-1);
    require(before.selectedCardId == ids[0] && before.find(ids[0])->selected,
        "Snapshot changed with live selection");
    require(model.removeCard(1), "Close failed");
    ids.removeAt(0);
    const auto after = makeCardWorkspaceSnapshot(model, ids, true);
    require(!after.find(u"window-a"_s) && after.find(u"window-b"_s)
        && after.find(u"window-b"_s)->cardIndex == 1,
        "Index renumbering changed window identity");
    const auto inactive = makeCardWorkspaceSnapshot(model, ids, false);
    require(inactive.selectedCardId.isEmpty() && inactive.selectedStack.isEmpty(),
        "Inactive context exposed an active selection");
    const auto empty = makeCardWorkspaceSnapshot(CardLineModel(1), {}, false);
    require(empty.cards.isEmpty() && empty.selectedCardId.isEmpty(), "Empty workspace invented a card");
    // A null native handle must not renumber the remaining window metadata.
    const auto missing = makeCardWorkspaceSnapshot(CardLineModel(2), {QString(), u"survivor"_s}, true);
    require(!missing.find({}) && missing.find(u"survivor"_s)->cardIndex == 2,
        "Missing native handle shifted a surviving compatibility index");
}
