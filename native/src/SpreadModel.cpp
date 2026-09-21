/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "SpreadModel.h"

#include <algorithm>
#include <cstdlib>
#include <numeric>

namespace Kadunce
{

SpreadModel::SpreadModel(int cardCount)
{
    cardCount = std::max(cardCount, 1);
    m_cardCount = cardCount;
    m_stacks.reserve(static_cast<std::size_t>(cardCount));
    for (int cardId = 1; cardId <= cardCount; ++cardId) {
        m_stacks.push_back(CardStack{{cardId}, 0});
    }
}

int SpreadModel::count() const
{
    return static_cast<int>(m_stacks.size());
}

int SpreadModel::cardCount() const
{
    return m_cardCount;
}

int SpreadModel::selectedIndex() const
{
    return m_selectedIndex;
}

int SpreadModel::selectedId() const
{
    return idAtOffset(0);
}

int SpreadModel::idAtOffset(int offset) const
{
    const CardStack &stack = m_stacks.at(static_cast<std::size_t>(
        wrappedIndex(m_selectedIndex + offset)));
    return stack.cards.at(static_cast<std::size_t>(stack.activeIndex));
}

std::array<int, 3> SpreadModel::visibleNeighborhood() const
{
    if (count() == 2) {
        return m_pairNeighborSide < 0
            ? std::array<int, 3>{idAtOffset(-1), selectedId(), 0}
            : std::array<int, 3>{0, selectedId(), idAtOffset(1)};
    }
    return {idAtOffset(-1), idAtOffset(0), idAtOffset(1)};
}

std::array<int, 3> SpreadModel::detachedNeighborhood(int pageOffset) const
{
    if (count() == 1) {
        return {0, 0, 0};
    }

    std::vector<int> remaining;
    remaining.reserve(static_cast<std::size_t>(count() - 1));
    for (int offset = 1; offset < count(); ++offset) {
        remaining.push_back(idAtOffset(offset));
    }

    const int remainingCount = static_cast<int>(remaining.size());
    const auto wrappedRemaining = [remainingCount](int index) {
        const int remainder = index % remainingCount;
        return remainder < 0 ? remainder + remainingCount : remainder;
    };
    const int center = wrappedRemaining(pageOffset);
    if (remainingCount == 1) {
        return {0, remaining[0], 0};
    }
    if (remainingCount == 2) {
        return {remaining[static_cast<std::size_t>(wrappedRemaining(center - 1))],
                remaining[static_cast<std::size_t>(center)], 0};
    }
    return {
        remaining[static_cast<std::size_t>(wrappedRemaining(center - 1))],
        remaining[static_cast<std::size_t>(center)],
        remaining[static_cast<std::size_t>(wrappedRemaining(center + 1))],
    };
}

bool SpreadModel::invariantHolds() const
{
    if (count() < 1 || m_selectedIndex < 0 || m_selectedIndex >= count()) {
        return false;
    }

    std::vector<int> expected(static_cast<std::size_t>(m_cardCount));
    std::iota(expected.begin(), expected.end(), 1);
    std::vector<int> actual;
    actual.reserve(static_cast<std::size_t>(m_cardCount));
    for (const CardStack &stack : m_stacks) {
        if (stack.cards.empty() || stack.activeIndex < 0
            || stack.activeIndex >= static_cast<int>(stack.cards.size())) {
            return false;
        }
        actual.insert(actual.end(), stack.cards.begin(), stack.cards.end());
    }
    std::sort(actual.begin(), actual.end());
    return actual == expected;
}

bool SpreadModel::sameStack(int firstId, int secondId) const
{
    const int first = stackIndexForId(firstId);
    return first >= 0 && first == stackIndexForId(secondId);
}

int SpreadModel::stackSizeForId(int cardId) const
{
    const int index = stackIndexForId(cardId);
    return index < 0 ? 0 : static_cast<int>(
        m_stacks.at(static_cast<std::size_t>(index)).cards.size());
}

int SpreadModel::rowPositionForId(int cardId) const
{
    return stackIndexForId(cardId);
}

int SpreadModel::stackPositionForId(int cardId) const
{
    const int stackIndex = stackIndexForId(cardId);
    if (stackIndex < 0) {
        return -1;
    }
    const std::vector<int> &cards =
        m_stacks.at(static_cast<std::size_t>(stackIndex)).cards;
    const auto it = std::find(cards.cbegin(), cards.cend(), cardId);
    return it == cards.cend() ? -1
                              : static_cast<int>(std::distance(cards.cbegin(), it));
}

int SpreadModel::stackActivePositionForId(int cardId) const
{
    const int index = stackIndexForId(cardId);
    return index < 0 ? -1
                     : m_stacks.at(static_cast<std::size_t>(index)).activeIndex;
}

std::vector<int> SpreadModel::stackMembersForId(int cardId) const
{
    const int index = stackIndexForId(cardId);
    return index < 0 ? std::vector<int>{}
                     : m_stacks.at(static_cast<std::size_t>(index)).cards;
}

std::vector<int> SpreadModel::stackPaintOrderForId(int cardId) const
{
    const int index = stackIndexForId(cardId);
    if (index < 0) {
        return {};
    }
    const CardStack &stack = m_stacks.at(static_cast<std::size_t>(index));
    const int size = static_cast<int>(stack.cards.size());
    if (size <= 1) {
        return stack.cards;
    }

    // makeOpenStackPose assigns the four preceding unique members to the reference layout's
    // fixed shoulder slots. Record them nearest-first, then reverse that list
    // so KWin raises farthest shoulder -> nearest shoulder -> active face.
    constexpr int MaximumRearShoulders = 3;
    std::vector<int> nearestFirst;
    nearestFirst.reserve(std::min(size - 1, MaximumRearShoulders));
    for (int distance = 1; distance <= MaximumRearShoulders; ++distance) {
        const int remainder = (stack.activeIndex - distance) % size;
        const int candidate = remainder < 0 ? remainder + size : remainder;
        if (candidate == stack.activeIndex) {
            continue;
        }
        const int candidateId = stack.cards.at(
            static_cast<std::size_t>(candidate));
        if (std::find(nearestFirst.cbegin(), nearestFirst.cend(), candidateId)
            == nearestFirst.cend()) {
            nearestFirst.push_back(candidateId);
        }
    }

    std::vector<int> paintOrder;
    paintOrder.reserve(nearestFirst.size() + 1);
    paintOrder.insert(paintOrder.end(), nearestFirst.crbegin(),
                      nearestFirst.crend());
    paintOrder.push_back(stack.cards.at(
        static_cast<std::size_t>(stack.activeIndex)));
    return paintOrder;
}

bool SpreadModel::selectedIsStandalone() const
{
    return stackSizeForId(selectedId()) == 1;
}

void SpreadModel::page(int delta)
{
    if (delta == 0) {
        return;
    }

    const int direction = delta < 0 ? -1 : 1;
    for (int step = 0; step < std::abs(delta); ++step) {
        m_selectedIndex = wrappedIndex(m_selectedIndex + direction);
        if (count() == 2) m_pairNeighborSide = -m_pairNeighborSide;
    }
}

void SpreadModel::pageStack(int delta)
{
    if (delta == 0) {
        return;
    }
    CardStack &stack =
        m_stacks.at(static_cast<std::size_t>(m_selectedIndex));
    const int size = static_cast<int>(stack.cards.size());
    if (size <= 1) {
        return;
    }
    const int remainder = (stack.activeIndex + delta) % size;
    stack.activeIndex = remainder < 0 ? remainder + size : remainder;
}

void SpreadModel::selectIndex(int index)
{
    if (count() == 2 && wrappedIndex(index) != m_selectedIndex)
        m_pairNeighborSide = -m_pairNeighborSide;
    m_selectedIndex = wrappedIndex(index);
}

int SpreadModel::appendCenteredCard()
{
    const int cardId = ++m_cardCount;
    // Keep the existing shoulder on its side; move the old center to the
    // opposite side of the newcomer. With one old group it moves left.
    const int insertion = m_selectedIndex + (count() == 2 && m_pairNeighborSide < 0 ? 0 : 1);
    m_stacks.insert(m_stacks.begin() + insertion, CardStack{{cardId}, 0});
    m_selectedIndex = insertion;
    if (count() == 2) m_pairNeighborSide = -1;
    return cardId;
}

int SpreadModel::appendCard(bool preserveSelection)
{
    const int cardId = ++m_cardCount;
    if (preserveSelection) {
        // With a pair, introduce the third group on the empty shoulder.
        const int insertion = count() == 2
            ? m_selectedIndex + (m_pairNeighborSide < 0 ? 1 : 0)
            : count();
        m_stacks.insert(m_stacks.begin() + insertion, CardStack{{cardId}, 0});
        if (insertion <= m_selectedIndex) ++m_selectedIndex;
        return cardId;
    }
    m_stacks.push_back(CardStack{{cardId}, 0});
    m_selectedIndex = count() - 1;
    return cardId;
}

bool SpreadModel::removeCard(int cardId)
{
    if (m_cardCount <= 1 || m_detachedMember.valid) {
        return false;
    }
    const int stackIndex = stackIndexForId(cardId);
    if (stackIndex < 0) {
        return false;
    }

    if (count() == 3 && stackSizeForId(cardId) == 1) {
        // Removing a shoulder must not teleport the surviving shoulder.
        m_pairNeighborSide = stackIndex == wrappedIndex(m_selectedIndex + 1)
            ? -1 : 1;
    }

    CardStack &stack = m_stacks.at(static_cast<std::size_t>(stackIndex));
    const auto member = std::find(stack.cards.begin(), stack.cards.end(), cardId);
    const int memberIndex = static_cast<int>(
        std::distance(stack.cards.begin(), member));
    stack.cards.erase(member);
    if (stack.cards.empty()) {
        m_stacks.erase(m_stacks.begin() + stackIndex);
        if (stackIndex < m_selectedIndex) {
            --m_selectedIndex;
        } else if (m_selectedIndex >= count()) {
            m_selectedIndex = count() - 1;
        }
    } else {
        if (stack.activeIndex > memberIndex) {
            --stack.activeIndex;
        }
        stack.activeIndex = std::min(
            stack.activeIndex, static_cast<int>(stack.cards.size()) - 1);
    }

    for (CardStack &remaining : m_stacks) {
        for (int &remainingId : remaining.cards) {
            if (remainingId > cardId) {
                --remainingId;
            }
        }
    }
    --m_cardCount;
    return invariantHolds();
}

void SpreadModel::moveSelected(int delta)
{
    if (count() < 2 || delta == 0) {
        return;
    }

    const int direction = delta < 0 ? -1 : 1;
    for (int step = 0; step < std::abs(delta); ++step) {
        const int target = wrappedIndex(m_selectedIndex + direction);
        std::swap(m_stacks.at(static_cast<std::size_t>(m_selectedIndex)),
                  m_stacks.at(static_cast<std::size_t>(target)));
        m_selectedIndex = target;
    }
}

bool SpreadModel::stackSelectedWith(int destinationId, int insertionIndex,
                                    InsertionSelection selection)
{
    if (!selectedIsStandalone()) {
        return false;
    }
    const int sourceIndex = m_selectedIndex;
    int destinationIndex = stackIndexForId(destinationId);
    if (destinationIndex < 0 || destinationIndex == sourceIndex) {
        return false;
    }

    const int sourceId = selectedId();
    if (sourceIndex < destinationIndex) {
        --destinationIndex;
    }
    m_stacks.erase(m_stacks.begin() + sourceIndex);
    CardStack &destination =
        m_stacks.at(static_cast<std::size_t>(destinationIndex));
    const int insertion = insertionIndex < 0
        ? static_cast<int>(destination.cards.size())
        : std::clamp(insertionIndex, 0,
                     static_cast<int>(destination.cards.size()));
    destination.cards.insert(destination.cards.begin() + insertion, sourceId);
    // Placement and selection are separate: inserting before the destination's
    // face moves its index, not its identity. Legacy direct commands may still
    // explicitly select the newcomer; prepared drag placement preserves the face.
    destination.activeIndex = selection == InsertionSelection::InsertedCard
        ? insertion : destination.activeIndex + (insertion <= destination.activeIndex ? 1 : 0);
    m_selectedIndex = destinationIndex;
    return invariantHolds();
}

bool SpreadModel::detachSelectedMember()
{
    if (m_detachedMember.valid || selectedIsStandalone()) {
        return false;
    }

    const int sourceStackIndex = m_selectedIndex;
    CardStack &source = m_stacks.at(
        static_cast<std::size_t>(sourceStackIndex));
    const int sourceMemberIndex = source.activeIndex;
    const int cardId = source.cards.at(
        static_cast<std::size_t>(sourceMemberIndex));
    m_detachedMember = {
        .cardId = cardId,
        .sourceStackIndex = sourceStackIndex + 1,
        .sourceMemberIndex = sourceMemberIndex,
        .sourceActiveIndex = source.activeIndex,
        .valid = true,
    };

    source.cards.erase(source.cards.begin() + sourceMemberIndex);
    // The fan exposes preceding members nearest-first. Removing its front
    // must reveal that same nearest shoulder, not the next storage-list item.
    source.activeIndex = sourceMemberIndex > 0 ? sourceMemberIndex - 1
        : static_cast<int>(source.cards.size()) - 1;
    m_stacks.insert(m_stacks.begin() + sourceStackIndex,
                    CardStack{{cardId}, 0});
    m_selectedIndex = sourceStackIndex;
    return invariantHolds();
}

bool SpreadModel::restoreDetachedMember()
{
    if (!m_detachedMember.valid) {
        return false;
    }

    const DetachedMember detached = m_detachedMember;
    const int standaloneIndex = stackIndexForId(detached.cardId);
    if (standaloneIndex < 0
        || m_stacks.at(static_cast<std::size_t>(standaloneIndex)).cards.size()
            != 1) {
        return false;
    }

    int sourceStackIndex = detached.sourceStackIndex;
    m_stacks.erase(m_stacks.begin() + standaloneIndex);
    if (standaloneIndex < sourceStackIndex) {
        --sourceStackIndex;
    }
    if (sourceStackIndex < 0 || sourceStackIndex >= count()) {
        return false;
    }

    CardStack &source = m_stacks.at(
        static_cast<std::size_t>(sourceStackIndex));
    const int memberIndex = std::clamp(
        detached.sourceMemberIndex, 0, static_cast<int>(source.cards.size()));
    source.cards.insert(source.cards.begin() + memberIndex, detached.cardId);
    source.activeIndex = std::clamp(
        detached.sourceActiveIndex, 0,
        static_cast<int>(source.cards.size()) - 1);
    m_selectedIndex = sourceStackIndex;
    m_detachedMember = DetachedMember{};
    return invariantHolds();
}

void SpreadModel::commitDetachedMember()
{
    m_detachedMember = DetachedMember{};
}

bool SpreadModel::hasDetachedMember() const
{
    return m_detachedMember.valid;
}

int SpreadModel::wrappedIndex(int index) const
{
    const int size = count();
    const int remainder = index % size;
    return remainder < 0 ? remainder + size : remainder;
}

int SpreadModel::stackIndexForId(int cardId) const
{
    for (int stackIndex = 0; stackIndex < count(); ++stackIndex) {
        const std::vector<int> &cards =
            m_stacks.at(static_cast<std::size_t>(stackIndex)).cards;
        if (std::find(cards.cbegin(), cards.cend(), cardId) != cards.cend()) {
            return stackIndex;
        }
    }
    return -1;
}

} // namespace Kadunce
