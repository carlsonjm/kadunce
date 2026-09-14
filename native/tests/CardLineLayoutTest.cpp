/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CardLineLayout.h"
#include "FocusedPairLayout.h"
#include "HeldCardGeometry.h"
#include "NeighborStackPose.h"

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
    for (double fraction : {0.54, 0.64}) {
        const Kadunce::CardRect pickup{200, 100, 1200 * fraction, 800 * fraction};
        for (double anchor : {0.0, 0.2, 0.5, 1.0}) {
            const double x = pickup.x + pickup.width * anchor;
            const double y = pickup.y + pickup.height * anchor;
            for (double t : {0.0, 0.25, 0.7, 1.0}) {
                const auto held = Kadunce::anchoredStackCarry(pickup, x, y,
                    1200 * Kadunce::HeldCardFraction, 800 * Kadunce::HeldCardFraction, t);
                require(close(held.x + held.width * anchor, x)
                    && close(held.y + held.height * anchor, y), "Held scaling moved contact anchor");
                require(close(held.width / held.height, 1.5), "Held scaling distorted aspect");
                if (t == 1.0) require(close(held.width, 528) && close(held.height, 352),
                    "Held card did not reach44% work-area dimensions");
            }
        }
    }
    for (const auto dimensions : {std::array<double, 4>{0, 0, 2560, 1500},
                                  std::array<double, 4>{-1280, 40, 1280, 1920}}) {
        const auto [x, y, w, h] = dimensions;
        const auto pair = Kadunce::makeFocusedPairLayout(x, y, w, h);
        const auto ordinary = Kadunce::makeCardLineLayout(x, y, w, h);
        for (int neighbors : {0, 1, 2, 3}) {
            const auto guest = Kadunce::makeLauncherGuestLayout(x, y, w, h, neighbors);
            const double scale = neighbors <= 1 ? 0.64 : 0.54;
            require(close(guest.cards[1].width, w * scale)
                        && close(guest.cards[1].height, h * scale)
                        && close(guest.cards[0].width, w * 0.54)
                        && close(guest.cards[2].height, h * 0.54),
                    "Launcher guest and shoulder sizes disagree with neighbor count");
        }
        require(pair.cards[1].width > ordinary.cards[1].width,
                "Focused pair did not gain an intermediate size");
        require(close(pair.cards[1].width, w * 0.64)
                    && close(pair.cards[1].height, h * 0.64),
                "Focused pair center did not use the accepted 64% size");
        for (int shoulder : {0, 2}) {
            require(close(pair.cards[shoulder].width, ordinary.cards[shoulder].width)
                        && close(pair.cards[shoulder].height, ordinary.cards[shoulder].height)
                        && close(pair.cards[shoulder].y, ordinary.cards[shoulder].y),
                    "Pair shoulder changed standard three-card dimensions or vertical alignment");
        }
        require(close(pair.cards[1].x - pair.cards[0].right(), pair.gutter)
                    && close(pair.cards[2].x - pair.cards[1].right(), pair.gutter),
                "Mixed-size pair did not preserve shoulder spacing");
        for (const auto envelope : {Kadunce::CardStackEnvelope{0, 0},
                                     Kadunce::CardStackEnvelope{-90, 20}}) {
            const auto reservedCenter = Kadunce::makeReservedFocusedPairTarget(pair, 0, envelope);
            const auto reservedLeft = Kadunce::makeReservedFocusedPairTarget(pair, -1, envelope);
            const auto reservedRight = Kadunce::makeReservedFocusedPairTarget(pair, 1, envelope);
            require(close(reservedLeft.width, w * 0.54)
                        && close(reservedRight.height, h * 0.54)
                        && close(reservedCenter.width, w * 0.64),
                    "Stack reservation replaced mixed-size pair dimensions");
            require(close(reservedCenter.x + envelope.left - reservedLeft.right(), pair.gutter)
                        && close(reservedRight.x - reservedCenter.right() - envelope.right, pair.gutter),
                    "Mixed-size shoulders do not clear the selected stack envelope");
        }
        require(close(pair.cards[1].x + pair.cards[1].width / 2, x + w / 2)
                    && close(pair.cards[1].y + pair.cards[1].height / 2, y + h / 2),
                "Focused pair is not centered on its own output");
        require(pair.cards[1].x > x && pair.cards[1].right() < x + w
                    && pair.cards[1].y > y && pair.cards[1].bottom() < y + h,
                "Focused pair center is not fully visible");
        require(pair.cards[0].x < x && pair.cards[0].right() > x
                    && pair.cards[2].x < x + w && pair.cards[2].right() > x + w,
                "Focused pair shoulders are not exposed at both possible edges");
        require(close(pair.cards[1].width / pair.cards[1].height, w / h),
                "Focused pair stretches the accepted aperture aspect ratio");
    }
    constexpr double width = 1024.0;
    // Contract coverage across output origins and every supported preference,
    // not just the current tablet's geometry or a few sample gutter values.
    for (const auto origin : {std::array<double, 2>{0, 0},
                              std::array<double, 2>{-1920, 240}}) {
        for (int gutter = 6; gutter <= 48; ++gutter) {
            const auto bounds = Kadunce::makeActiveTarget(origin[0], origin[1], 1280, 800, gutter, 10);
            require(close(bounds.x - origin[0], gutter)
                        && close(bounds.y - origin[1], gutter)
                        && close(origin[0] + 1280 - bounds.right(), gutter)
                        && close(origin[1] + 800 - bounds.bottom(), gutter + 10),
                    "Active bounds lost the stable Bento dock clearance");
        }
    }
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
    require(close(Kadunce::heldPickupProgress(0), 0)
        && close(Kadunce::heldPickupProgress(90), 0.875)
        && close(Kadunce::heldPickupProgress(180), 1)
        && close(Kadunce::heldPickupProgress(500), 1),
        "Pickup easing must start at captured pose and finish in180ms");
    for (int count : {3, 4}) {
        for (int side : {-1, 1}) {
            const double extent = 7.0 * (count - 1);
            const auto front = Kadunce::neighborStackPose(0, count, side, 100, extent);
            const auto back = Kadunce::neighborStackPose(count - 1, count, side, 100, extent);
            require(close(front.x - back.x, 40), "Neighbor spread must share a fixed budget");
            require(close(side > 0 ? back.x : front.x, side > 0 ? -extent : 0),
                "Neighbor inward edge must preserve the gap");
            for (int depth = 1; depth < count; ++depth) {
                const auto a = Kadunce::neighborStackPose(depth - 1, count, side, 100, extent);
                const auto b = Kadunce::neighborStackPose(depth, count, side, 100, extent);
                require(close(a.x - b.x, 40.0 / (count - 1)), "Unequal neighbor shoulders");
            }
        }
    }
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
                && close(insertionRight.y, 0.0),
            "Luna insertion geometry did not open around its selected seam");
    const auto insertionEnvelope =
        Kadunce::makeInsertionStackEnvelope(
            20, center.width, center.height);
    for (int depth = 0; depth < 4; ++depth) {
        const auto browse = Kadunce::makeOpenStackPose(3-depth, 4, 3, center.width);
        const auto insert = Kadunce::makeInsertionStackPose(3-depth, 4, 3, center.width);
        require(close(browse.x, insert.x) && close(browse.y, insert.y)
                    && close(browse.rotation, insert.rotation),
                "Browse and insertion must share rigid fan poses");
    }
    for (int count : {5,20,100}) {
        const auto env = Kadunce::makeInsertionStackEnvelope(count, center.width, center.height);
        require(close(env.left, insertionEnvelope.left) && close(env.right, insertionEnvelope.right),
                "Insertion envelope grew beyond visible deck");
    }
    require(insertionEnvelope.left < 0.0
                && insertionEnvelope.right > 0.0,
            "The insertion envelope did not reserve both browsable seams");

    const auto active = Kadunce::makeActiveTarget(
        0.0, 0.0, width, height, 10, 10);
    require(active.x > 0.0 && active.y > 0.0,
            "Active target has no protective margin");
    require(active.right() < width && active.bottom() < height,
            "Active target exceeds the tablet work area");
    require(close(active.x, 10.0) && close(active.y, 10.0)
                && close(width - active.right(), 10.0)
                && close(height - active.bottom(), 20.0),
            "Active target does not match Bento's 10px sides and 20px bottom");

    const auto noDock = Kadunce::makeActiveTarget(0, 241, 1463, 915);
    require(close(noDock.y - 241, 10) && close(241 + 915 - noDock.bottom(), 10),
            "Dock-free tablet must retain symmetric margins");
    const auto edge = Kadunce::makeActiveTarget(20, 30, width, height, 0, 10);
    require(close(edge.x, 26) && close(edge.y, 36) && close(edge.width, width - 12)
                && close(edge.height, height - 22), "Saved zero gutter must clamp to 6 px plus dock clearance");
    const auto maximum = Kadunce::makeActiveTarget(0, 0, width, height, 200);
    require(close(maximum.x, 48) && close(maximum.width, width - 96),
            "Oversized gutter must clamp to 48 px");
    const auto minimum = Kadunce::makeActiveTarget(0, 0, width, height, 6);
    require(close(minimum.x, 6), "Minimum gutter must remain 6 px");
    const auto custom = Kadunce::makeActiveTarget(20, 30, width, height, 36);
    require(close(custom.x, 56) && close(custom.y, 66)
                && close(custom.width, width - 72), "Custom gutter was not applied");
    const auto tiny = Kadunce::makeActiveTarget(0, 0, 100, 80, 200);
    require(tiny.width > 0 && tiny.height >= 1, "Gutter produced invalid small-screen bounds");
    const auto negative = Kadunce::makeActiveTarget(0, 0, width, height, -5);
    require(close(negative.x, 6), "Negative gutter must clamp to 6 px");
    std::cout << "Card Line anchors and Active bounds are deterministic\n";
    return EXIT_SUCCESS;
}
