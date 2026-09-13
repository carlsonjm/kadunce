/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CardLineLayout.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Kadunce
{

namespace
{
// Shared rigid fan pose. Positive slots mirror the same shallow fan around
// the face; all bottom-right pivots share a baseline.
CardStackPose fanPose(int slot, double width)
{
    constexpr std::array<double, 5> angles{0.6, 0.2, -0.2, -0.4, -0.6};
    const int depth = std::min(std::abs(slot), 4);
    const double angle = slot > 0 ? 1.2 - angles[depth] : angles[depth];
    return {slot * width * 0.24 / 3.0, 0.0, angle, true};
}
CardStackEnvelope bottomRightRotationEnvelope(
    const CardStackPose &pose, double cardWidth, double cardHeight)
{
    const double radians = pose.rotation * std::numbers::pi / 180.0;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    double minimumX = cardWidth;
    double maximumX = 0.0;
    const std::array<std::array<double, 2>, 4> corners{
        std::array<double, 2>{0.0, 0.0},
        std::array<double, 2>{cardWidth, 0.0},
        std::array<double, 2>{cardWidth, cardHeight},
        std::array<double, 2>{0.0, cardHeight},
    };
    for (const auto &corner : corners) {
        const double dx = corner[0] - cardWidth;
        const double dy = corner[1] - cardHeight;
        const double transformedX = cardWidth
            + cosine * dx - sine * dy;
        minimumX = std::min(minimumX, transformedX);
        maximumX = std::max(maximumX, transformedX);
    }
    return {
        pose.x + minimumX,
        pose.x + maximumX - cardWidth,
    };
}
}

CardLineLayout makeCardLineLayout(double workX, double workY,
                                  double workWidth, double workHeight)
{
    const double cardWidth = workWidth * 0.54;
    const double cardHeight = workHeight * 0.54;
    const double gutter = workWidth * 0.056;
    const double centerX = workX + (workWidth - cardWidth) / 2.0;
    const double cardY = workY + (workHeight - cardHeight) / 2.0;

    return {
        .cards = {
            CardRect{centerX - cardWidth - gutter, cardY,
                     cardWidth, cardHeight},
            CardRect{centerX, cardY, cardWidth, cardHeight},
            CardRect{centerX + cardWidth + gutter, cardY,
                     cardWidth, cardHeight},
        },
        .gutter = gutter,
    };
}

CardRect makeCoverPaintRect(const CardRect &slot,
                            double sourceWidth,
                            double sourceHeight)
{
    if (sourceWidth <= 0.0 || sourceHeight <= 0.0) {
        return slot;
    }

    const double scale = std::max(slot.width / sourceWidth,
                                  slot.height / sourceHeight);
    const double paintedWidth = sourceWidth * scale;
    const double paintedHeight = sourceHeight * scale;
    return CardRect{
        slot.x + (slot.width - paintedWidth) / 2.0,
        slot.y + (slot.height - paintedHeight) / 2.0,
        paintedWidth,
        paintedHeight,
    };
}

CardLineAction classifyCardLineGesture(
    double startX, double startY, double endX, double endY,
    double centerLeft, double centerRight)
{
    const double dx = endX - startX;
    const double dy = endY - startY;
    if (std::abs(dx) > 58.0 && std::abs(dx) > std::abs(dy) * 1.2) {
        return dx < 0.0 ? CardLineAction::Next
                        : CardLineAction::Previous;
    }
    if (endX < centerLeft) {
        return CardLineAction::Previous;
    }
    if (endX >= centerRight) {
        return CardLineAction::Next;
    }
    return CardLineAction::Activate;
}

int classifyStackGesture(double startX, double startY,
                         double endX, double endY,
                         bool startedOnSelectedStack)
{
    if (!startedOnSelectedStack) {
        return 0;
    }
    const double dx = endX - startX;
    const double dy = endY - startY;
    if (std::abs(dy) <= 38.0 || std::abs(dy) <= std::abs(dx) * 1.15) {
        return 0;
    }
    return dy < 0.0 ? -1 : 1;
}

int classifyCardEdge(double positionX, double workX,
                     double workWidth, double edgeZone)
{
    if (workWidth <= 0.0 || edgeZone <= 0.0) {
        return 0;
    }
    const double boundedZone = std::min(edgeZone, workWidth / 2.0);
    if (positionX <= workX + boundedZone) {
        return -1;
    }
    if (positionX >= workX + workWidth - boundedZone) {
        return 1;
    }
    return 0;
}

CardStackPose makeOpenStackPose(int cardIndex, int cardCount, int activeIndex,
                                double cardWidth)
{
    if (cardCount <= 1 || cardIndex < 0 || cardIndex >= cardCount) {
        return {0.0, 0.0, 0.0, true};
    }

    constexpr double StackFaceRotation = 0.6;
    const int currentPosition = std::clamp(activeIndex, 0, cardCount - 1);
    if (cardIndex == currentPosition) {
        return {0.0, 0.0, StackFaceRotation, true};
    }

    // The TouchPad fans the exposed deck behind the face. Keep three compressed
    // fan poses fixed while identities cycle through them; this makes group
    // width independent of activeIndex without flattening the deck.
    constexpr std::array<int, 3> RelativeMembers{-1, -2, -3};
    constexpr std::array<int, 3> VisualSlots{-1, -2, -3};
    // Nearest shoulder falls slightly; the next two rise. Together with the
    // +0.6 degree face this reproduces the reference layout's shallow crossing fan instead of
    // rotating every layer progressively in one direction.
    int visualSlot = 0;
    bool found = false;
    for (std::size_t index = 0; index < RelativeMembers.size(); ++index) {
        const int relative = RelativeMembers[index];
        const int remainder = (currentPosition + relative) % cardCount;
        const int candidate = remainder < 0 ? remainder + cardCount : remainder;
        if (candidate == currentPosition) {
            continue;
        }
        bool duplicate = false;
        for (std::size_t previous = 0; previous < index; ++previous) {
            const int previousRemainder =
                (currentPosition + RelativeMembers[previous]) % cardCount;
            const int previousCandidate = previousRemainder < 0
                ? previousRemainder + cardCount : previousRemainder;
            duplicate = duplicate || candidate == previousCandidate;
        }
        if (!duplicate && cardIndex == candidate) {
            visualSlot = VisualSlots[index];
            found = true;
            break;
        }
    }
    if (!found) {
        return {0.0, 0.0, 0.0, false};
    }

    // The renderer rotates the deck around each card's lower-right corner.
    // A shared y therefore pins every lower-right corner to the face card's
    // baseline while the rotation itself supplies the reference layout's vertical shoulder.
    return fanPose(visualSlot, cardWidth);
}

CardStackPose makeClosedStackPose(int cardIndex, int cardCount,
                                  double outputWidth)
{
    if (cardCount <= 1 || cardIndex < 0 || cardIndex >= cardCount) {
        return {0.0, 0.0, 0.0, true};
    }

    constexpr double TouchPadReferenceWidth = 1024.0;
    constexpr double StackClosedStep = 7.0;
    constexpr int TouchPadMaximumClosedSteps = 3;
    const int stepsBehind = std::min(
        cardCount - 1 - cardIndex, TouchPadMaximumClosedSteps);
    const double scale = outputWidth / TouchPadReferenceWidth;
    return {-StackClosedStep * scale * stepsBehind, 0.0, 0.0, true};
}

CardStackPose makeInsertionStackPose(int memberIndex, int memberCount,
                                     int insertionIndex, double cardWidth)
{
    if (memberCount <= 1 || memberIndex < 0 || memberIndex >= memberCount) {
        return {0.0, 0.0, 0.0, true};
    }

    constexpr int VisibleDeckSize = 5;
    const int seam = std::clamp(insertionIndex, 0, memberCount - 1);
    const int visibleCount = std::min(memberCount, VisibleDeckSize);
    const int firstVisible = std::clamp(
        seam - visibleCount / 2, 0, memberCount - visibleCount);
    if (memberIndex < firstVisible
        || memberIndex >= firstVisible + visibleCount) {
        return {0.0, 0.0, 0.0, false};
    }

    const int relative = memberIndex - seam;
    return fanPose(relative, cardWidth);
}

CardStackEnvelope makeOpenStackEnvelope(int cardCount, int activeIndex,
                                        double cardWidth, double cardHeight)
{
    if (cardCount <= 1 || cardWidth <= 0.0 || cardHeight <= 0.0) {
        return {0.0, 0.0};
    }

    double left = 0.0;
    double right = 0.0;
    for (int cardIndex = 0; cardIndex < cardCount; ++cardIndex) {
        const CardStackPose pose = makeOpenStackPose(
            cardIndex, cardCount, activeIndex, cardWidth);
        if (!pose.visible) {
            continue;
        }
        const CardStackEnvelope bounds = bottomRightRotationEnvelope(
            pose, cardWidth, cardHeight);
        left = std::min(left, bounds.left);
        right = std::max(right, bounds.right);
    }
    return {left, right};
}

CardStackEnvelope makeInsertionStackEnvelope(int cardCount,
                                              double cardWidth,
                                              double cardHeight)
{
    if (cardCount <= 1 || cardWidth <= 0.0 || cardHeight <= 0.0) {
        return {0.0, 0.0};
    }

    double left = 0.0;
    double right = 0.0;
    // Only five poses can be visible. Their relative offsets are independent
    // of total membership; avoid quadratic work on every target query.
    const int visibleCount = std::min(cardCount, 5);
    for (int insertionIndex = 0; insertionIndex < visibleCount;
         ++insertionIndex) {
        for (int memberIndex = 0; memberIndex < visibleCount; ++memberIndex) {
            const CardStackPose pose = makeInsertionStackPose(
                memberIndex, visibleCount, insertionIndex, cardWidth);
            if (!pose.visible) {
                continue;
            }
            const CardStackEnvelope bounds = bottomRightRotationEnvelope(
                pose, cardWidth, cardHeight);
            left = std::min(left, bounds.left);
            right = std::max(right, bounds.right);
        }
    }
    return {left, right};
}

CardRect makeReservedCardTarget(const CardLineLayout &layout, int slot,
                                const CardStackEnvelope &centerEnvelope)
{
    const CardRect &center = layout.cards[1];
    const double pitch = center.width + layout.gutter;
    // Center the complete visual envelope, not merely its face card. the reference layout's
    // fan opens to the left, so keeping the face at screen center made the
    // whole deck look left-heavy. The face shifts right by half the envelope.
    const double groupCentering =
        -(centerEnvelope.left + centerEnvelope.right) / 2.0;
    double x = center.x + groupCentering + slot * pitch;
    if (slot < 0) {
        x += centerEnvelope.left;
    } else if (slot > 0) {
        x += centerEnvelope.right;
    }
    return {x, center.y, center.width, center.height};
}

CardRect makeActiveTarget(double workX, double workY,
                          double workWidth, double workHeight, double requestedGutter)
{
    const double limit = std::max(0.0, (std::min(workWidth, workHeight) - 1.0) / 2.0);
    const double gutter = std::min(std::clamp(requestedGutter, 6.0, 48.0), limit);
    return CardRect{workX + gutter, workY + gutter,
                    workWidth - (2.0 * gutter),
                    workHeight - (2.0 * gutter)};
}

} // namespace Kadunce
