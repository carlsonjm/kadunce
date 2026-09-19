/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <array>
#include <vector>

namespace Kadunce
{

class SpreadModel
{
public:
    explicit SpreadModel(int cardCount = 20);

    [[nodiscard]] int count() const;
    [[nodiscard]] int cardCount() const;
    [[nodiscard]] int selectedIndex() const;
    [[nodiscard]] int selectedId() const;
    [[nodiscard]] int pairNeighborSide() const { return m_pairNeighborSide; }
    void setPairNeighborSide(int side) { m_pairNeighborSide = side < 0 ? -1 : 1; }
    [[nodiscard]] int idAtOffset(int offset) const;
    [[nodiscard]] std::array<int, 3> visibleNeighborhood() const;
    [[nodiscard]] std::array<int, 3> detachedNeighborhood(int pageOffset) const;
    [[nodiscard]] bool sameStack(int firstId, int secondId) const;
    [[nodiscard]] int stackSizeForId(int cardId) const;
    [[nodiscard]] int stackPositionForId(int cardId) const;
    [[nodiscard]] int stackActivePositionForId(int cardId) const;
    [[nodiscard]] std::vector<int> stackMembersForId(int cardId) const;
    [[nodiscard]] std::vector<int> stackPaintOrderForId(int cardId) const;
    [[nodiscard]] bool selectedIsStandalone() const;
    [[nodiscard]] bool invariantHolds() const;

    void page(int delta);
    void pageStack(int delta);
    void selectIndex(int index);
    int appendCard(bool preserveSelection = false);
    int appendCenteredCard();
    bool removeCard(int cardId);
    void moveSelected(int delta);
    enum class InsertionSelection { InsertedCard, DestinationCard };
    bool stackSelectedWith(int destinationId, int insertionIndex = -1,
        InsertionSelection selection = InsertionSelection::InsertedCard);
    bool detachSelectedMember();
    bool restoreDetachedMember();
    void commitDetachedMember();
    [[nodiscard]] bool hasDetachedMember() const;

private:
    struct CardStack {
        std::vector<int> cards;
        int activeIndex = 0;
    };

    struct DetachedMember {
        int cardId = 0;
        int sourceStackIndex = -1;
        int sourceMemberIndex = -1;
        int sourceActiveIndex = -1;
        bool valid = false;
    };

    [[nodiscard]] int wrappedIndex(int index) const;
    [[nodiscard]] int stackIndexForId(int cardId) const;

    std::vector<CardStack> m_stacks;
    DetachedMember m_detachedMember;
    int m_cardCount = 1;
    int m_selectedIndex = 0;
    int m_pairNeighborSide = 1;
};

} // namespace Kadunce
