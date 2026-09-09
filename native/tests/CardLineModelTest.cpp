/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CardLineModel.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main()
{
    Kadunce::CardLineModel oneCard(1);
    require(oneCard.count() == 1, "One-card line was padded with fake cards");
    oneCard.page(99);
    require(oneCard.selectedId() == 1 && oneCard.invariantHolds(),
            "One-card line did not remain stable");

    Kadunce::CardLineModel twoCards(2);
    require(twoCards.count() == 2, "Two-card line was padded with fake cards");
    twoCards.selectIndex(1);
    require(twoCards.selectedId() == 2, "Explicit selection chose the wrong card");

    Kadunce::CardLineModel admitted(3);
    require(admitted.appendCard() == 4
                && admitted.cardCount() == 4
                && admitted.count() == 4
                && admitted.selectedId() == 4
                && admitted.invariantHolds(),
            "A newly admitted app did not become the selected standalone card");
    require(admitted.removeCard(2)
                && admitted.cardCount() == 3
                && admitted.count() == 3
                && admitted.selectedId() == 3
                && admitted.invariantHolds(),
            "Closing a helper window rebuilt or corrupted the live Card Line");
    require(twoCards.invariantHolds(), "Two-card selection broke invariants");

    Kadunce::CardLineModel line(20);

    require(line.count() == 20, "Card count changed at construction");
    require(line.selectedId() == 1, "Initial card is not card 1");
    require(line.visibleNeighborhood() == std::array<int, 3>{20, 1, 2},
            "Initial neighborhood does not wrap cleanly");

    for (int step = 1; step <= 10000; ++step) {
        line.page(1);
        require(line.count() == 20, "Right paging changed line length");
        require(line.selectedId() == (step % 20) + 1,
                "Right paging selected the wrong card");
        require(line.invariantHolds(), "Right paging broke model invariants");
    }

    require(line.selectedId() == 1,
            "Full right-paging cycles did not return to card 1");

    for (int step = 1; step <= 10000; ++step) {
        line.page(-1);
        const int expected = 20 - ((step - 1) % 20);
        require(line.selectedId() == expected,
                "Left paging selected the wrong card");
        require(line.count() == 20, "Left paging changed line length");
        require(line.invariantHolds(), "Left paging broke model invariants");
    }

    require(line.selectedId() == 1,
            "Full left-paging cycles did not return to card 1");
    require(line.visibleNeighborhood() == std::array<int, 3>{20, 1, 2},
            "Neighborhood changed after repeated paging");
    require(line.detachedNeighborhood(0) == std::array<int, 3>{20, 2, 3},
            "Detached row did not close the lifted card's source gap");
    require(line.detachedNeighborhood(1) == std::array<int, 3>{2, 3, 4},
            "One deliberate edge page did not advance one destination");
    require(line.detachedNeighborhood(-1) == std::array<int, 3>{19, 20, 2},
            "Reverse edge page did not expose the previous destination");

    line.moveSelected(1);
    require(line.selectedId() == 1,
            "Moving right changed the grabbed card identity");
    require(line.visibleNeighborhood() == std::array<int, 3>{2, 1, 3},
            "Moving right did not place the grabbed card after its neighbor");
    require(line.invariantHolds(), "Moving right duplicated or lost a card");

    line.moveSelected(-1);
    require(line.selectedId() == 1,
            "Moving left changed the grabbed card identity");
    require(line.visibleNeighborhood() == std::array<int, 3>{20, 1, 2},
            "Moving left did not restore the original order");

    line.moveSelected(-1);
    require(line.selectedId() == 1,
            "Wrapped reorder changed the grabbed card identity");
    require(line.visibleNeighborhood() == std::array<int, 3>{19, 1, 20},
            "Wrapped reorder did not cross the Card Line seam cleanly");
    require(line.count() == 20 && line.invariantHolds(),
            "Reordering changed Card Line membership");

    Kadunce::CardLineModel threeCards(3);
    require(threeCards.detachedNeighborhood(0)
                == std::array<int, 3>{3, 2, 0},
            "Three-card detached row duplicated a destination");
    require(threeCards.detachedNeighborhood(1)
                == std::array<int, 3>{2, 3, 0},
            "Three-card edge page did not remain deterministic");

    Kadunce::CardLineModel stacks(5);
    require(stacks.stackSelectedWith(2),
            "A standalone card could not join its destination stack");
    require(stacks.count() == 4 && stacks.cardCount() == 5,
            "Stacking confused group count with live-card count");
    require(stacks.selectedId() == 1
                && stacks.stackMembersForId(1) == std::vector<int>{2, 1},
            "The carried card was not placed on top of its destination");
    require(stacks.stackPaintOrderForId(1) == std::vector<int>({2, 1}),
            "A two-card fan did not paint its rear member before its face");
    require(stacks.sameStack(1, 2)
                && stacks.stackSizeForId(1) == 2
                && stacks.stackPositionForId(2) == 0
                && stacks.stackPositionForId(1) == 1
                && stacks.stackActivePositionForId(2) == 1,
            "Stack membership or active position is inconsistent");
    require(!stacks.selectedIsStandalone(),
            "A committed stack still reports as standalone");
    require(!stacks.stackSelectedWith(3),
            "The first stack gate unexpectedly moved a whole stack");
    stacks.page(-1);
    require(stacks.selectedId() == 5,
            "Horizontal paging did not treat a stack as one group");
    stacks.page(1);
    require(stacks.selectedId() == 1,
            "Horizontal paging did not return to the remembered stack face");
    stacks.page(1);
    require(stacks.selectedId() == 3,
            "Horizontal paging did not advance into the next group");
    stacks.page(-1);
    stacks.pageStack(-1);
    require(stacks.selectedId() == 2,
            "Vertical paging could not select the lower stack member");
    require(stacks.stackPaintOrderForId(2) == std::vector<int>({1, 2}),
            "Cycling a stack did not keep the selected face on top");
    stacks.page(1);
    stacks.page(-1);
    require(stacks.selectedId() == 2,
            "A stack forgot its active member after horizontal paging");
    stacks.pageStack(1);
    require(stacks.selectedId() == 1,
            "Vertical paging could not return to the top stack member");
    require(stacks.invariantHolds(),
            "Stack commit duplicated or lost a live card");

    require(stacks.detachSelectedMember(),
            "The active stack member could not be lifted into a transaction");
    require(stacks.hasDetachedMember() && stacks.selectedIsStandalone()
                && stacks.selectedId() == 1
                && stacks.count() == 5
                && stacks.stackMembersForId(2) == std::vector<int>{2},
            "A lifted stack member did not become a reversible standalone group");
    require(stacks.detachedNeighborhood(0)[1] == 2,
            "The source stack did not remain centered beneath its lifted member");
    require(stacks.restoreDetachedMember()
                && !stacks.hasDetachedMember()
                && stacks.stackMembersForId(1) == std::vector<int>({2, 1})
                && stacks.stackActivePositionForId(1) == 1
                && stacks.count() == 4,
            "Cancelling a stack lift did not restore exact membership and face");

    require(stacks.detachSelectedMember(),
            "The restored stack could not begin a second lift");
    stacks.commitDetachedMember();
    require(!stacks.hasDetachedMember() && stacks.selectedIsStandalone()
                && stacks.stackMembersForId(2) == std::vector<int>{2}
                && stacks.count() == 5 && stacks.invariantHolds(),
            "Committing a stack lift did not leave one standalone card");

    Kadunce::CardLineModel threeMemberStack(3);
    require(threeMemberStack.stackSelectedWith(2),
            "Three-member lift test could not create its first pair");
    threeMemberStack.page(1);
    require(threeMemberStack.stackSelectedWith(1)
                && threeMemberStack.stackMembersForId(3)
                    == std::vector<int>({2, 1, 3}),
            "Three-member lift test could not create its source stack");
    threeMemberStack.pageStack(-1);
    require(threeMemberStack.selectedId() == 1
                && threeMemberStack.detachSelectedMember()
                && threeMemberStack.stackMembersForId(2)
                    == std::vector<int>({2, 3}),
            "Lifting the middle face corrupted a three-member stack");
    require(threeMemberStack.restoreDetachedMember()
                && threeMemberStack.stackMembersForId(1)
                    == std::vector<int>({2, 1, 3})
                && threeMemberStack.selectedId() == 1,
            "A cancelled middle-face lift did not restore its exact position");

    Kadunce::CardLineModel insertedStack(4);
    require(insertedStack.stackSelectedWith(2),
            "Insertion test could not create its destination stack");
    insertedStack.page(1);
    require(insertedStack.stackSelectedWith(1, 1)
                && insertedStack.stackMembersForId(3)
                    == std::vector<int>({2, 3, 1})
                && insertedStack.stackActivePositionForId(3) == 1,
            "An explicit insertion seam did not place the carried card in order");

    Kadunce::CardLineModel largeStack(20);
    require(largeStack.stackSelectedWith(2),
            "Large-stack seed failed");
    for (int cardId = 3; cardId <= 20; ++cardId) {
        largeStack.page(1);
        require(largeStack.selectedId() == cardId,
                "Large-stack test could not select the next standalone card");
        require(largeStack.stackSelectedWith(1),
                "Large stack rejected a valid additional card");
    }
    require(largeStack.count() == 1 && largeStack.cardCount() == 20
                && largeStack.stackSizeForId(1) == 20
                && largeStack.invariantHolds(),
            "Card Line imposed a four- or five-card stack limit");
    require(largeStack.stackPaintOrderForId(largeStack.selectedId())
                == std::vector<int>({17, 18, 19, 20}),
            "A large fan did not expose a deterministic back-to-front deck");
    std::vector<bool> visited(21, false);
    for (int step = 0; step < 20; ++step) {
        largeStack.pageStack(1);
        visited.at(static_cast<std::size_t>(largeStack.selectedId())) = true;
    }
    require(std::all_of(visited.cbegin() + 1, visited.cend(),
                        [](bool seen) { return seen; }),
            "A large stack hid cards from vertical member paging");

    std::cout << "Card Line group and vertical stack paging are deterministic\n";
    return EXIT_SUCCESS;
}
