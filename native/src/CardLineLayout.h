/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <array>

namespace Kadunce
{

struct CardRect {
    double x;
    double y;
    double width;
    double height;

    [[nodiscard]] double right() const { return x + width; }
    [[nodiscard]] double bottom() const { return y + height; }
};

struct CardLineLayout {
    std::array<CardRect, 3> cards;
    double gutter;
};

struct CardStackPose {
    double x;
    double y;
    double rotation;
    bool visible = true;
};

struct CardStackEnvelope {
    // Horizontal extensions beyond the ordinary single-card slot. left is
    // zero or negative; right is zero or positive.
    double left;
    double right;
};

enum class CardLineAction {
    None,
    Previous,
    Next,
    Activate,
};

[[nodiscard]] CardLineLayout makeCardLineLayout(
    double workX, double workY, double workWidth, double workHeight);

// Scale a source proportionally until it covers the entire fixed Card Line
// slot. The compositor clips this expanded paint rectangle back to the slot,
// so source geometry can never shrink or stretch a card.
[[nodiscard]] CardRect makeCoverPaintRect(
    const CardRect &slot, double sourceWidth, double sourceHeight);

[[nodiscard]] CardLineAction classifyCardLineGesture(
    double startX, double startY, double endX, double endY,
    double centerLeft, double centerRight);

// Vertical motion belongs to the selected deck only. Return -1 for the
// previous member, +1 for the next member, and zero for every other gesture.
[[nodiscard]] int classifyStackGesture(
    double startX, double startY, double endX, double endY,
    bool startedOnSelectedStack);

// A held card may browse destinations only from an explicit physical edge
// zone. Ordinary travel over cards and gaps always returns zero.
[[nodiscard]] int classifyCardEdge(
    double positionX, double workX, double workWidth, double edgeZone);

// the reference layout's stacked-card geometry adapted to a stable PC Card Line: the
// active member stays centered while at most three members occupy fixed,
// compressed fan poses behind it. A closed stack exposes only its last three
// seven-pixel steps. The visual cap is never a membership cap.
[[nodiscard]] CardStackPose makeOpenStackPose(
    int cardIndex, int cardCount, int activeIndex, double cardWidth);
[[nodiscard]] CardStackPose makeClosedStackPose(
    int cardIndex, int cardCount, double outputWidth);

// During a reorder, the carried card owns insertionIndex and the destination
// members occupy their ordered positions on either side of that seam.
[[nodiscard]] CardStackPose makeInsertionStackPose(
    int memberIndex, int memberCount, int insertionIndex, double cardWidth);

// Measure the transformed bounds of an open deck. Neighboring Card Line
// groups reserve these extensions so a stack remains one logical slot without
// allowing either adjacent group to paint through it.
[[nodiscard]] CardStackEnvelope makeOpenStackEnvelope(
    int cardCount, int activeIndex, double cardWidth, double cardHeight);
[[nodiscard]] CardStackEnvelope makeInsertionStackEnvelope(
    int cardCount, double cardWidth, double cardHeight);

[[nodiscard]] CardRect makeReservedCardTarget(
    const CardLineLayout &layout, int slot,
    const CardStackEnvelope &centerEnvelope);

[[nodiscard]] CardRect makeActiveTarget(
    double workX, double workY, double workWidth, double workHeight,
    double requestedGutter = 10.0, double bottomClearance = 0.0);

} // namespace Kadunce
