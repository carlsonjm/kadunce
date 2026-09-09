/*
    SPDX-FileCopyrightText: 2026 Warbler Studio contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CardLineLayout.h"

#include <algorithm>
#include <cmath>
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

bool close(double a, double b)
{
    return std::abs(a - b) < 0.0001;
}
}

int main()
{
    constexpr double width = 1024.0;
    constexpr double height = 640.0;
    const Kadunce::CardLineLayout layout =
        Kadunce::makeCardLineLayout(0.0, 0.0, width, height);
    const auto &left = layout.cards[0];
    const auto &center = layout.cards[1];
    const auto &right = layout.cards[2];

    require(center.x >= 0.0 && center.right() <= width,
            "Center card is not fully visible");
    require(center.y >= 0.0 && center.bottom() <= height,
            "Center card exceeds the vertical work area");
    require(left.x < 0.0 && left.right() > 0.0,
            "Left neighbor is not a partial edge card");
    require(right.x < width && right.right() > width,
            "Right neighbor is not a partial edge card");
    require(close(left.width, center.width)
                && close(center.width, right.width),
            "Neighbor width differs from center width");
    require(close(center.x - left.right(), layout.gutter),
            "Left gutter differs from the layout gutter");
    require(close(right.x - center.right(), layout.gutter),
            "Right gutter differs from the layout gutter");

    require(close(center.width, width * 0.54)
                && close(center.height, height * 0.54),
            "Card Line did not retain the accepted 54% card aperture");
    require(close(layout.gutter, width * 0.056),
            "Card Line did not retain the accepted 5.6% gutter");

    const auto landscapeCover = Kadunce::makeCoverPaintRect(
        center, 1600.0, 900.0);
    const auto portraitCover = Kadunce::makeCoverPaintRect(
        center, 720.0, 1200.0);
    for (const auto &cover : {landscapeCover, portraitCover}) {
        require(cover.x <= center.x && cover.right() >= center.right(),
                "Cover paint leaves a horizontal gap in its fixed slot");
        require(cover.y <= center.y && cover.bottom() >= center.bottom(),
                "Cover paint leaves a vertical gap in its fixed slot");
        require(close(cover.x + cover.width / 2.0,
                      center.x + center.width / 2.0)
                    && close(cover.y + cover.height / 2.0,
                             center.y + center.height / 2.0),
                "Cover paint is not centered behind its aperture");
    }
    require(close(portraitCover.width, center.width),
            "Portrait source changed the fixed Card Line width");

    using Kadunce::CardLineAction;
    using Kadunce::classifyCardLineGesture;
    require(classifyCardLineGesture(500, 300, 420, 305,
                                    center.x, center.right())
                == CardLineAction::Next,
            "Leftward Card Line swipe did not select next");
    require(classifyCardLineGesture(500, 300, 580, 295,
                                    center.x, center.right())
                == CardLineAction::Previous,
            "Rightward Card Line swipe did not select previous");
    require(classifyCardLineGesture(center.x - 10, 300,
                                    center.x - 10, 300,
                                    center.x, center.right())
                == CardLineAction::Previous,
            "Left neighbor tap did not select previous");
    require(classifyCardLineGesture(center.right() + 10, 300,
                                    center.right() + 10, 300,
                                    center.x, center.right())
                == CardLineAction::Next,
            "Right neighbor tap did not select next");
    require(classifyCardLineGesture(center.x + 20, 300,
                                    center.x + 20, 300,
                                    center.x, center.right())
                == CardLineAction::Activate,
            "Center card tap did not request Active");

    using Kadunce::classifyStackGesture;
    require(classifyStackGesture(500, 500, 504, 450, true) == -1,
            "Upward motion on a stack did not select its previous member");
    require(classifyStackGesture(500, 500, 496, 550, true) == 1,
            "Downward motion on a stack did not select its next member");
    require(classifyStackGesture(500, 500, 504, 450, false) == 0,
            "Vertical motion outside a stack changed its member");
    require(classifyStackGesture(500, 500, 550, 530, true) == 0,
            "A horizontal gesture was stolen by stack-member navigation");

    using Kadunce::classifyCardEdge;
    constexpr double edgeZone = 82.0;
    require(classifyCardEdge(width / 2.0, 0.0, width, edgeZone) == 0,
            "A destination-card hover incorrectly requested paging");
    require(classifyCardEdge(edgeZone + 1.0, 0.0, width, edgeZone) == 0,
            "Travel outside the edge zone incorrectly requested paging");
    require(classifyCardEdge(edgeZone - 1.0, 0.0, width, edgeZone) == -1,
            "The deliberate left edge zone did not request a page");
    require(classifyCardEdge(width - edgeZone + 1.0,
                             0.0, width, edgeZone) == 1,
            "The deliberate right edge zone did not request a page");

    const double deckStep = center.width * 0.24 / 3.0;
    const auto fanX = [deckStep](int step) {
        return -step * deckStep;
    };
    const auto openBack = Kadunce::makeOpenStackPose(
        0, 3, 1, center.width);
    const auto openActive = Kadunce::makeOpenStackPose(
        1, 3, 1, center.width);
    const auto openFront = Kadunce::makeOpenStackPose(
        2, 3, 1, center.width);
    require(openBack.visible && openActive.visible && openFront.visible,
            "A three-card deck hid an exposed member");
    require(close(openBack.x, fanX(1))
                && close(openActive.x, 0.0)
                && close(openFront.x, fanX(2)),
            "Open stack does not preserve its fixed reference layout fan");
    require(close(openBack.y, 0.0)
                && close(openActive.y, 0.0)
                && close(openFront.y, 0.0),
            "The reference layout fan lost its lower-right shared baseline");
    require(close(openBack.rotation, 0.2)
                && close(openActive.rotation, 0.6)
                && close(openFront.rotation, -0.2),
            "the reference layout's shallow crossing fan and tilted face were omitted");
    const auto openBottom = Kadunce::makeOpenStackPose(
        0, 4, 3, center.width);
    require(close(openBottom.rotation, -0.4),
            "The bottom reference layout shoulder did not retain its slight upward tilt");

    const auto closedBack = Kadunce::makeClosedStackPose(0, 4, width);
    const auto closedMiddle = Kadunce::makeClosedStackPose(2, 4, width);
    const auto closedFront = Kadunce::makeClosedStackPose(3, 4, width);
    require(close(closedBack.x, -21.0)
                && close(closedMiddle.x, -7.0)
                && close(closedFront.x, 0.0),
            "Closed stack does not expose the reference layout's final three 7 px steps");
    int visibleMembers = 0;
    for (int index = 0; index < 20; ++index) {
        visibleMembers += Kadunce::makeOpenStackPose(
            index, 20, 10, center.width).visible ? 1 : 0;
    }
    require(visibleMembers == 4,
            "A large stack did not expose exactly one face and three shoulders");

    std::array<double, 4> fourVisibleSlots{
        fanX(3), fanX(2), fanX(1), 0.0};
    std::sort(fourVisibleSlots.begin(), fourVisibleSlots.end());
    std::array<double, 4> actualSlots{};
    int actualSlotCount = 0;
    for (int index = 0; index < 5; ++index) {
        const auto pose = Kadunce::makeOpenStackPose(
            index, 5, 2, center.width);
        if (pose.visible) {
            actualSlots[static_cast<std::size_t>(actualSlotCount++)] = pose.x;
        }
    }
    require(actualSlotCount == 4,
            "A five-member stack did not retain four physical fan poses");
    std::sort(actualSlots.begin(), actualSlots.end());
    for (std::size_t index = 0; index < actualSlots.size(); ++index) {
        require(close(actualSlots[index], fourVisibleSlots[index]),
                "The centered deck lost one of its fixed physical poses");
    }

    const auto twoCardOtherAtFaceZero = Kadunce::makeOpenStackPose(
        1, 2, 0, center.width);
    const auto twoCardOtherAtFaceOne = Kadunce::makeOpenStackPose(
        0, 2, 1, center.width);
    require(close(twoCardOtherAtFaceZero.x, fanX(1))
                && close(twoCardOtherAtFaceOne.x, fanX(1)),
            "Cycling a two-card stack moved its fixed reference layout fan");

    const auto envelope = Kadunce::makeOpenStackEnvelope(
        3, 1, center.width, center.height);
    require(envelope.left < openBack.x && envelope.right > openFront.x,
            "Stack envelope omitted the rotated card corners");
    const auto reservedLeft = Kadunce::makeReservedCardTarget(
        layout, -1, envelope);
    const auto reservedCenter = Kadunce::makeReservedCardTarget(
        layout, 0, envelope);
    const auto reservedRight = Kadunce::makeReservedCardTarget(
        layout, 1, envelope);
    require(close(reservedCenter.x,
                  center.x - (envelope.left + envelope.right) / 2.0),
            "The complete reference layout fan envelope was not visually centered");
    require(close(reservedLeft.right() + layout.gutter,
                  reservedCenter.x + envelope.left),
            "Left neighbor did not preserve the gutter around the stack");
    require(close(reservedRight.x - layout.gutter,
                  reservedCenter.right() + envelope.right),
            "Right neighbor did not preserve the gutter around the stack");

    const auto stableEnvelope = Kadunce::makeOpenStackEnvelope(
        20, 0, center.width, center.height);
    for (int activeIndex = 1; activeIndex < 20; ++activeIndex) {
        const auto cycledEnvelope = Kadunce::makeOpenStackEnvelope(
            20, activeIndex, center.width, center.height);
        require(close(cycledEnvelope.left, stableEnvelope.left)
                    && close(cycledEnvelope.right, stableEnvelope.right),
                "Vertical stack cycling changed the horizontal group envelope");
    }

    const auto insertionLeft = Kadunce::makeInsertionStackPose(
        0, 3, 1, center.width);
    const auto insertionFace = Kadunce::makeInsertionStackPose(
        1, 3, 1, center.width);
    const auto insertionRight = Kadunce::makeInsertionStackPose(
        2, 3, 1, center.width);
    require(close(insertionLeft.x, -deckStep)
                && close(insertionFace.x, 0.0)
                && close(insertionRight.x, deckStep)
                && insertionRight.y > 0.0,
            "Luna insertion geometry did not open around its selected seam");
    const auto insertionEnvelope =
        Kadunce::makeInsertionStackEnvelope(
            20, center.width, center.height);
    require(insertionEnvelope.left < 0.0
                && insertionEnvelope.right > 0.0,
            "The insertion envelope did not reserve both browsable seams");

    const auto active = Kadunce::makeActiveTarget(
        0.0, 0.0, width, height);
    require(active.x > 0.0 && active.y > 0.0,
            "Active target has no protective margin");
    require(active.right() < width && active.bottom() < height,
            "Active target exceeds the tablet work area");
    require(close(active.x, 10.0) && close(active.y, 10.0)
                && close(width - active.right(), 10.0)
                && close(height - active.bottom(), 10.0),
            "Active target does not retain an exact 10 px gutter");

    std::cout << "Card Line anchors and Active bounds are deterministic\n";
    return EXIT_SUCCESS;
}
