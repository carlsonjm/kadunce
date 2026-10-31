/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "BentoLayout.h"
#include "BentoColumns.h"
#include <map>
#include "BentoSidePlacement.h"
#include "WheelWidth.h"
#include <algorithm>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>

using namespace Kadunce;

namespace
{

bool overlaps(const BentoPixelRect &first, const BentoPixelRect &second)
{
    return first.x < second.x + second.width
        && first.x + first.width > second.x
        && first.y < second.y + second.height
        && first.y + first.height > second.y;
}

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

}

int main()
{
    const BentoCandidate small{100, 100, 1, 1, false};
    for (int count = 2; count <= 8; ++count) {
        for (bool right : {false, true}) for (bool large : {false, true}) {
            const auto layout = bentoSideLayout({right, large}, 3840, 2160, small,
                std::vector<BentoCandidate>(count-1, small));
            require(layout && layout->size() == static_cast<size_t>(count),
                "Monitor preset lost a resident");
            const auto pixels = makePixelBentoLayout(*layout, 0, 0, 3840, 2160);
            for (size_t i = 0; i < pixels.size(); ++i) {
                require(pixels[i].width >= 100 && pixels[i].height >= 100,
                    "Monitor preset violated minimum");
                for (size_t j = i+1; j < pixels.size(); ++j)
                    require(!overlaps(pixels[i], pixels[j]), "Monitor preset overlaps");
            }
        }
    }
    for (bool right : {false, true}) for (bool large : {false, true}) {
        const auto layout = bentoSideLayout({right, large}, 1260, 780, small, small);
        require(layout && layout->size() == 2, "Side placement failed");
        const auto pixels = makePixelBentoLayout(*layout, 10, 10, 1260, 780);
        require((pixels[0].x > pixels[1].x) == right, "Side placement reversed");
        require((pixels[0].width > pixels[1].width) == large, "Side share reversed");
        require(!overlaps(pixels[0], pixels[1]), "Side panes overlap");
        require(std::abs((*layout)[0].width - (large ? 2.0/3.0 : 1.0/3.0)) < .00001,
            "Unconstrained pane does not use thirds");
    }
    const auto sideConstrained = bentoSideLayout({false, false}, 1200, 780,
        BentoCandidate{500, 200, 1, 1, false}, small);
    require(sideConstrained && makePixelBentoLayout(*sideConstrained, 0, 0, 1200, 780)[0].width >= 500,
        "Side split violated app minimum");
    require(!bentoSideLayout({false, true}, 1200, 780,
        BentoCandidate{800, 100, 1, 1, false}, BentoCandidate{800, 100, 1, 1, false}),
        "Impossible pair admitted");
    require(bentoSideLayout({true, false}, 1200, 780, small, std::nullopt)->at(0).width == 1,
        "Single card did not fill Active space");
    require(bentoSideChoice(false, 410, 400, BentoSidePlacement{false, true}).large
        && !bentoSideChoice(false, 425, 400, BentoSidePlacement{false, true}).large,
        "Midpoint hysteresis incorrect");
    for (int count = 1; count <= 8; ++count) {
        const std::vector<BentoRect> layout = makeBentoLayout(count, true);
        require(static_cast<int>(layout.size()) == count,
                "Bento layout returned the wrong pane count");
        const std::vector<BentoPixelRect> pixels = makePixelBentoLayout(
            layout, 10, 10, 2540, 1410);
        for (int first = 0; first < count; ++first) {
            require(pixels[first].x >= 10,
                    "Bento pane escaped the left edge");
            require(pixels[first].y >= 10,
                    "Bento pane escaped the top edge");
            require(pixels[first].x + pixels[first].width <= 2550,
                    "Bento pane escaped the right edge");
            require(pixels[first].y + pixels[first].height <= 1420,
                    "Bento pane escaped the bottom edge");
            for (int second = first + 1; second < count; ++second) {
                require(!overlaps(pixels[first], pixels[second]),
                        "Bento panes overlapped");
            }
        }
    }

    // Four panes stand in even quarters (CARD-LIFECYCLE.md §5).
    for (const bool landscape : {true, false}) {
        const std::vector<BentoRect> quarters = makeBentoLayout(4, landscape);
        require(quarters.size() == 4, "Four panes did not make four places");
        for (const auto &rect : quarters)
            require(std::abs(rect.width - 0.5) < .001 && std::abs(rect.height - 0.5) < .001,
                    "Four panes did not stand in even quarters");
    }

    const std::vector<BentoCandidate> ordinary{
        {300, 240, 1200, 800, true},
        {300, 240, 900, 900, false},
        {300, 240, 800, 1000, false},
        {300, 240, 1000, 700, false},
    };
    const BentoAdmission four = chooseBentoAdmission(
        ordinary, 2540, 1410);
    require(four.candidateIndices.size() == 4,
            "Four feasible monitor windows were not all admitted");
    require(four.rects.size() == 4,
            "Four-pane admission did not retain four rectangles");
    require(four.candidateIndices.front() == 0,
            "The preferred handoff window did not receive the lead pane");

    const std::vector<BentoCandidate> wideMinimums{
        {900, 300, 1200, 700, true},
        {900, 300, 1100, 700, false},
    };
    const BentoAdmission horizontalTwo = chooseBentoAdmission(
        wideMinimums, 1463, 885, 2);
    require(horizontalTwo.candidateIndices.size() == 2,
            "Two wide-minimum windows were unnecessarily parked");
    require(std::abs(horizontalTwo.rects.front().width - 1.0) < 0.001,
            "Wide-minimum apps did not select the horizontal split");
    require(std::abs(horizontalTwo.rects.front().height - 0.5) < 0.001,
            "Horizontal split height was not one half");

    const std::vector<BentoCandidate> constrained{
        {1900, 1200, 1900, 1200, true},
        {300, 240, 900, 700, false},
        {300, 240, 900, 700, false},
    };
    const BentoAdmission reduced = chooseBentoAdmission(
        constrained, 1200, 800, 3);
    require(reduced.candidateIndices.size() == 2,
            "Admission did not reduce an impossible three-pane layout");
    require(std::set<int>(reduced.candidateIndices.begin(),
                          reduced.candidateIndices.end())
            == std::set<int>({1, 2}),
            "Admission parked the wrong constrained window");

    require(!chooseBentoTransferAdmission(constrained, 0, 1200, 800, 3),
            "Oversized arrival was silently parked");
    const auto arrival = chooseBentoTransferAdmission(ordinary, 0, 2540, 1410);
    require(arrival && std::find(arrival->candidateIndices.begin(),
        arrival->candidateIndices.end(), 0) != arrival->candidateIndices.end(),
        "Feasible arrival has no visible pane");
    // Existing apps could fill two slots, but the arrival only fits alone.
    // Search a smaller valid arrangement instead of accepting its omission.
    const std::vector<BentoCandidate> singleArrival{
        {1100, 700, 1100, 700, true},
        {100, 100, 400, 400, false},
        {100, 100, 400, 400, false},
    };
    const auto single = chooseBentoTransferAdmission(singleArrival, 0, 1200, 800, 3);
    require(single && single->candidateIndices == std::vector<int>{0},
        "Arrival was omitted instead of selecting a feasible smaller layout");
    require(!chooseBentoTransferAdmission(ordinary, -1, 1200, 800)
        && !chooseBentoTransferAdmission(ordinary, 4, 1200, 800)
        && !chooseBentoTransferAdmission(ordinary, 0, 0, 800)
        && !chooseBentoTransferAdmission(ordinary, 0, 1200, 800, 0),
        "Invalid transfer destination admitted");
    for (int incoming = 0; incoming < 4; ++incoming) {
        const auto plan = chooseBentoTransferAdmission(ordinary, incoming, 2540, 1410);
        require(plan && std::find(plan->candidateIndices.begin(), plan->candidateIndices.end(), incoming)
            != plan->candidateIndices.end(), "Required non-leading candidate missing");
    }
    const std::vector<BentoCandidate> sideCrowd{
        {300, 240, 900, 700, true}, {1000, 700, 1000, 700, false},
        {300, 240, 900, 700, false}};
    for (bool right : {false, true}) for (bool large : {false, true}) {
        const auto fitted = chooseBentoSideAdmission({right, large}, 1200, 800, sideCrowd);
        require(fitted && fitted->candidateIndices == std::vector<int>({0, 2}),
            "Infeasible resident blocked edge admission or displaced dragged card");
        require(!chooseBentoSideAdmission({right, large}, 1200, 800, sideCrowd, 1),
            "Required newcomer was silently parked");
        const auto alone = chooseBentoSideAdmission({right, large}, 1200, 800,
            std::vector<BentoCandidate>{sideCrowd[1], sideCrowd[0]});
        require(alone && alone->candidateIndices == std::vector<int>{0}
            && alone->rects[0].width == 1 && alone->rects[0].height == 1,
            "Only fitting card did not fill Active space");
    }
    const std::vector<BentoCandidate> monitorFour(4, {600,600,900,700,false});
    for (bool right : {false,true}) {
        const auto plan = chooseBentoSideAdmission({right,true},2540,1410,monitorFour,0);
        require(plan && plan->candidateIndices.size() == 4,
            "Monitor edge preference hid a fourth window that fits normal Bento");
    }
    // A full monitor of mid-sized windows fits nine candidates only as the
    // curated library's eight, which puts candidate 0 wherever that pattern's
    // first slot is. The snap named a side, so the dragged card is on it.
    std::vector<BentoCandidate> fullMonitor(9, {600, 450, 900, 700, false});
    fullMonitor[0] = {940, 500, 900, 700, true};
    for (bool right : {false, true}) for (bool large : {false, true}) {
        const auto plan = chooseBentoSideAdmission({right, large}, 2540, 1410, fullMonitor, 0);
        require(plan && plan->candidateIndices.size() == 8,
            "A full monitor did not keep eight panes for an edge arrival");
        const auto at = std::find(plan->candidateIndices.begin(),
            plan->candidateIndices.end(), 0) - plan->candidateIndices.begin();
        const auto &r = plan->rects.at(at);
        require(right ? std::abs(r.x + r.width - 1) < .002 : std::abs(r.x) < .002,
            "An edge arrival on a full monitor landed away from the edge it was snapped to");
        const auto pixels = makePixelBentoLayout(plan->rects, 0, 0, 2540, 1410);
        for (size_t i = 0; i < pixels.size(); ++i) {
            const auto &c = fullMonitor.at(plan->candidateIndices.at(i));
            require(pixels[i].width >= c.minimumWidth && pixels[i].height >= c.minimumHeight,
                "Moving the edge arrival to its side put a window below its minimum size");
        }
    }
    for (bool right : {false,true}) {
        const std::vector<BentoRect> pair = right
            ? std::vector<BentoRect>{{0,0,2.0/3,1},{2.0/3,0,1.0/3,1}}
            : std::vector<BentoRect>{{1.0/3,0,2.0/3,1},{0,0,1.0/3,1}};
        const std::vector<BentoCandidate> small(3,{100,100,400,400,false});
        const auto split = splitBentoColumn(pair,small,2,{right,false},2560,1440);
        require(split && (*split)[0].x == pair[0].x && (*split)[0].width == pair[0].width
            && (*split)[1].y == 0 && (*split)[1].height == .5
            && (*split)[2].y == .5 && (*split)[2].width == pair[1].width,
            "Occupied small column did not preserve opposite pane and split downward");
        auto tooTall = small; tooTall[2].minimumHeight = 1380;
        require(!splitBentoColumn(pair,tooTall,2,{right,false},2560,1440),
            "Column split ignored native minimum heights");
        // A third pane is a curated-library arrangement. The compact display
        // caps at two, so it declines the split whichever column was contacted.
        require(!splitBentoColumn(pair,small,2,{right,false},1463,885)
            && !splitBentoColumn(pair,small,2,{!right,false},1463,885),
            "A column split grew a compact display past its two-pane cap");
    }
    const auto regroup = splitBentoColumn({{0,0,1.0/3,1},{1.0/3,0,1.0/3,1},{2.0/3,0,1.0/3,1}},
        std::vector<BentoCandidate>(3,{100,100,400,400,false}),0,{true,false},2560,1440);
    require(regroup && (*regroup)[0].y == .5 && (*regroup)[2].height == .5
        && (*regroup)[1].x == 0 && std::abs((*regroup)[1].width-2.0/3) < .001,
        "Moving a third column left a hole instead of filling the surviving pane");

    // CARD-LIFECYCLE.md section 5 grammar: one cap per display, orientation
    // from the work area, two three-pane shapes, and a contact that names the
    // shape. 1443x894 is the measured tablet work area.
    constexpr int tabletWidth = 1443;
    constexpr int tabletHeight = 894;
    constexpr int monitorWidth = 2540;
    constexpr int monitorHeight = 1410;
    require(bentoPaneCap(tabletWidth, tabletHeight) == 2
        && bentoPaneCap(monitorWidth, monitorHeight) == BentoPaneBound,
        "The tablet work area does not cap at two panes, or a monitor kept a count of eight");
    require(bentoLandscapeArea(tabletWidth, tabletHeight)
        && !bentoLandscapeArea(tabletHeight, tabletWidth),
        "Layout orientation did not follow the work area's own proportions");

    const std::vector<BentoCandidate> fiveSmall(5, {200, 200, 600, 400, false});
    require(chooseBentoAdmission(fiveSmall, tabletWidth, tabletHeight)
            .candidateIndices.size() == 2,
        "Ordinary admission did not read the display's pane cap");
    // The edge path had no maximum of its own, which is how a tablet side snap
    // reached three panes while ordinary admission stopped at two.
    const auto cappedSide = chooseBentoSideAdmission({false, false},
        tabletWidth, tabletHeight, fiveSmall);
    require(cappedSide && cappedSide->candidateIndices.size() == 2,
        "Edge admission read a different pane cap from ordinary admission");
    const auto monitorSide = chooseBentoSideAdmission({false, false},
        monitorWidth, monitorHeight, fiveSmall);
    require(chooseBentoAdmission(fiveSmall, monitorWidth, monitorHeight)
            .candidateIndices.size() == 5
        && monitorSide && monitorSide->candidateIndices.size() == 5,
        "A larger display lost panes to the compact cap");

    // CARD-LIFECYCLE.md §11: the cap is the display's sizing's where it has
    // one, never its work area's. A 1920x1080 monitor with a 24 px bar and a
    // 56 px dock leaves a 1900x980 stage, which by area caps at two.
    constexpr int panelledWidth = 1900;
    constexpr int panelledHeight = 980;
    const std::vector<BentoCandidate> fourSmall(4, {300, 200, 500, 400, false});
    require(bentoPaneCap(panelledWidth, panelledHeight) == BentoCompactPaneCap
        && bentoPaneCap(panelledWidth, panelledHeight, std::nullopt) == BentoCompactPaneCap
        && bentoPaneCap(panelledWidth, panelledHeight, BentoPaneBound) == BentoPaneBound
        && bentoPaneCap(monitorWidth, monitorHeight, BentoCompactPaneCap) == BentoCompactPaneCap,
        "A pane cap given by the display's sizing did not replace the work area's");
    require(chooseBentoAdmission(fourSmall, panelledWidth, panelledHeight)
                .candidateIndices.size() == 2
        && chooseBentoAdmission(fourSmall, panelledWidth, panelledHeight, BentoPaneBound,
               BentoPaneBound).candidateIndices.size() == 4,
        "Ordinary admission read the work area's cap over the display's");
    const auto sizedTransfer = chooseBentoTransferAdmission(fourSmall, 3,
        panelledWidth, panelledHeight, BentoPaneBound, BentoPaneBound);
    require(sizedTransfer && sizedTransfer->candidateIndices.size() == 4,
        "A transfer read the work area's cap over the display's");
    const auto areaSide = chooseBentoSideAdmission({false, true, true},
        panelledWidth, panelledHeight, fourSmall);
    const auto sizedSide = chooseBentoSideAdmission({false, true, true},
        panelledWidth, panelledHeight, fourSmall, 0, BentoPaneBound);
    require(areaSide && areaSide->candidateIndices.size() == 2
        && sizedSide && sizedSide->candidateIndices.size() == 4,
        "Edge admission read the work area's cap over the display's");
    const std::vector<BentoRect> halves{{0, 0, .5, 1}, {.5, 0, .5, 1}};
    const std::vector<BentoCandidate> threeSmall(3, {100, 100, 400, 400, false});
    require(!splitBentoColumn(halves, threeSmall, 2, {true, false, true},
                panelledWidth, panelledHeight)
        && splitBentoColumn(halves, threeSmall, 2, {true, false, true},
               panelledWidth, panelledHeight, BentoPaneBound),
        "Splitting a column read the work area's cap over the display's");
    // Chosen Solo, a monitor shows a pair at most however much room it has.
    const auto soloSide = chooseBentoSideAdmission({false, false},
        monitorWidth, monitorHeight, fiveSmall, 0, BentoCompactPaneCap);
    require(chooseBentoAdmission(fiveSmall, monitorWidth, monitorHeight, BentoPaneBound,
                BentoCompactPaneCap).candidateIndices.size() == 2
        && soloSide && soloSide->candidateIndices.size() == 2,
        "A monitor chosen Solo showed more than a pair");

    // §11, Connected: shares are even, so two windows too wide for the 0.38
    // pane and too tall for half the height stand side by side in halves. On
    // the 2560x1440 monitor three 1200x900 windows show two that way; Solo,
    // and a display without cards, keep the curated pair and show one.
    const std::vector<BentoCandidate> threeTall(3, {1200, 900, 1200, 900, false});
    const auto curatedTall = chooseBentoAdmission(threeTall, monitorWidth, monitorHeight,
        BentoPaneBound, BentoCompactPaneCap);
    const auto evenTall = chooseBentoAdmission(threeTall, monitorWidth, monitorHeight,
        BentoPaneBound, BentoPaneBound, true);
    const auto evenTallPixels = makePixelBentoLayout(evenTall.rects, 10, 10,
        monitorWidth, monitorHeight);
    require(curatedTall.candidateIndices.size() == 1
        && chooseBentoAdmission(threeTall, monitorWidth, monitorHeight)
               .candidateIndices.size() == 1
        && evenTall.candidateIndices == std::vector<int>({0, 1})
        && evenTallPixels.size() == 2
        && evenTallPixels[0].x == 10 && evenTallPixels[0].width == evenTallPixels[1].width
        && evenTallPixels[0].height == monitorHeight && evenTallPixels[1].height == monitorHeight
        && evenTallPixels[1].x + evenTallPixels[1].width == 10 + monitorWidth,
        "Connected auto-fill did not stand two tall windows side by side in halves");
    const auto evenTallTransfer = chooseBentoTransferAdmission(threeTall, 2,
        monitorWidth, monitorHeight, BentoPaneBound, BentoPaneBound, true);
    const auto curatedTallTransfer = chooseBentoTransferAdmission(threeTall, 2,
        monitorWidth, monitorHeight, BentoPaneBound, BentoPaneBound);
    require(evenTallTransfer && evenTallTransfer->candidateIndices.size() == 2
        && std::count(evenTallTransfer->candidateIndices.cbegin(),
               evenTallTransfer->candidateIndices.cend(), 2) == 1
        && curatedTallTransfer && curatedTallTransfer->candidateIndices == std::vector<int>({2}),
        "A transfer on Connected did not stand beside a tall pane in halves");
    // Where a curated pair holds both, Connected keeps it: the halves come last.
    const std::vector<BentoCandidate> twoSmall(2, {300, 200, 500, 400, false});
    const auto connectedPair = chooseBentoAdmission(twoSmall, monitorWidth, monitorHeight,
        BentoPaneBound, BentoPaneBound, true);
    require(connectedPair.rects.size() == 2 && std::abs(connectedPair.rects[0].width - .62) < 1e-9
        && std::abs(connectedPair.rects[1].x - .62) < 1e-9,
        "Connected auto-fill dropped the curated pair where it fits");

    // Past eight panes, minimum sizes decide the count. On the 2560x1440
    // monitor twelve 600x450 windows fill a four-by-three grid, and a
    // thirteenth fits no five-by-three cell, so it is the one left out.
    const std::vector<BentoCandidate> twelve(12, {600, 450, 800, 600, false});
    const auto grid = chooseBentoAdmission(twelve, 2560, 1440);
    require(grid.candidateIndices.size() == 12,
        "Twelve windows that each fit a twelfth of the monitor did not all show");
    for (const auto &pixel : makePixelBentoLayout(grid.rects, 0, 0, 2560, 1440))
        require(pixel.width + 2 >= 600 && pixel.height + 2 >= 450,
            "A grid pane is smaller than its window's minimum");
    const std::vector<BentoCandidate> thirteen(13, {600, 450, 800, 600, false});
    const auto crowded = chooseBentoAdmission(thirteen, 2560, 1440);
    require(crowded.candidateIndices.size() == 12
        && std::find(crowded.candidateIndices.begin(), crowded.candidateIndices.end(), 12)
            == crowded.candidateIndices.end(),
        "Thirteen windows did not stop at the twelve whose minimums fit");
    // An arrival that must show takes a grid pane from the last resident in
    // order, not from one the person was using earlier.
    const auto arriving = chooseBentoTransferAdmission(thirteen, 12, 2560, 1440);
    require(arriving && arriving->candidateIndices.size() == 12
        && std::count(arriving->candidateIndices.begin(), arriving->candidateIndices.end(), 12) == 1
        && std::count(arriving->candidateIndices.begin(), arriving->candidateIndices.end(), 11) == 0,
        "A required arrival did not take the grid's last resident's place");
    // Windows too large for any grid cell keep the curated library, and a
    // display's whole window history does not make that search slow: two
    // dozen of them settle within its candidates.
    const std::vector<BentoCandidate> large(BentoPaneBound, {700, 500, 800, 600, false});
    const auto curated = chooseBentoAdmission(large, 2560, 1440);
    require(!curated.candidateIndices.empty()
        && curated.candidateIndices.size() <= BentoCuratedPaneCap,
        "Windows too large for any grid did not keep the curated library");
    for (const int index : curated.candidateIndices)
        require(index < BentoCuratedCandidates,
            "The curated search read past its candidates");
    const auto lateArrival = chooseBentoTransferAdmission(large, BentoPaneBound - 1, 2560, 1440);
    require(lateArrival && std::count(lateArrival->candidateIndices.begin(),
            lateArrival->candidateIndices.end(), BentoPaneBound - 1) == 1,
        "An arrival past the curated candidates was not shown");

    // Both three-pane shapes exist, cover the area and do not overlap. The
    // curated library offers them on a display large enough to hold three.
    for (bool landscape : {false, true}) {
        const std::vector<BentoRect> shapes[2]{makeBentoLayout(3, landscape),
                                               makeAlternateThreePaneBentoLayout(landscape)};
        for (const auto &shape : shapes) {
            require(shape.size() == 3, "A permitted three-pane shape is missing");
            const auto pixels = makePixelBentoLayout(shape, 0, 0, 1600, 1000);
            for (size_t i = 0; i < pixels.size(); ++i)
                for (size_t j = i + 1; j < pixels.size(); ++j)
                    require(!overlaps(pixels[i], pixels[j]), "Three-pane shape overlaps");
        }
        require((std::abs(shapes[1][0].height - 1.0) < .001) == landscape
            && (std::abs(shapes[1][0].width - 1.0) < .001) == !landscape,
            "The second three-pane shape ignored the work area's long axis");
        int wholeHeight = 0;
        for (const auto &r : shapes[0])
            if (std::abs(r.y) < .01 && std::abs(r.height - 1.0) < .01) ++wholeHeight;
        require((wholeHeight == 1) == landscape,
            "The first three-pane shape is not one pane beside a split");
    }

    // Three windows too tall for a stacked half reach the other three-pane
    // shape instead of dropping a pane.
    const std::vector<BentoCandidate> tallThree(3, {300, 1200, 900, 1200, false});
    const auto alternateThree = chooseBentoAdmission(tallThree, monitorWidth, monitorHeight);
    require(alternateThree.candidateIndices.size() == 3,
        "The second three-pane shape was not offered to ordinary admission");
    for (const auto &r : alternateThree.rects)
        require(std::abs(r.height - 1.0) < .01,
            "Three tall windows did not take the three-panes-across shape");

    // The tablet's three-pane contact grammar is deferred. No display caps at
    // the grammar's pane count, so a side snap keeps the curated proportional
    // placement and stops at the compact cap whichever half was contacted.
    const std::vector<BentoCandidate> tabletThree(3, {200, 200, 700, 500, false});
    for (bool right : {false, true}) {
        for (bool large : {false, true}) {
            require(bentoSideShapeOrder({right, large}, BentoContactGrammarPaneCap,
                        tabletWidth, tabletHeight).front() == BentoSideShape::Proportional
                && bentoSideShapeOrder({right, large}, BentoContactGrammarPaneCap,
                        monitorWidth, monitorHeight).front() == BentoSideShape::Proportional,
                "The deferred three-pane contact grammar was consulted");
            const auto admission = chooseBentoSideAdmission({right, large},
                tabletWidth, tabletHeight, tabletThree);
            require(admission && admission->candidateIndices.size() == 2,
                "A tablet side snap grew past the two-pane cap");
            require(right
                    ? std::abs(admission->rects[0].x + admission->rects[0].width - 1.0) < .01
                    : std::abs(admission->rects[0].x) < .01,
                "The arrival did not keep the contacted edge");
            require((admission->rects[0].width > admission->rects[1].width) == large,
                "The contacted half did not choose the arrival's share");
        }
    }
    // CARD-LIFECYCLE.md §8: a layout that cannot grow gives the arrival one
    // slot, and fit is the whole rule for which one. The tablet's two-pane
    // shape in pixels is what the arrival is measured against.
    const auto tabletSlots = makePixelBentoLayout(
        makeBentoLayout(2, bentoLandscapeArea(tabletWidth, tabletHeight)),
        0, 0, tabletWidth, tabletHeight);
    require(tabletSlots.size() == 2 && tabletSlots[0].width > tabletSlots[1].width,
        "The two-pane tablet shape is not one wide pane beside one narrow one");
    const double wide = tabletSlots[0].width;
    const double narrow = tabletSlots[1].width;

    // A window that only fits the wide pane takes the wide pane, so the window
    // sitting in it is the one that leaves.
    require(bentoSlotForArrival(tabletSlots, narrow + 1, 200) == 0,
        "An arrival too wide for the narrow pane did not claim the wide one");

    // One that fits either takes the narrow pane and leaves the wide one, and
    // its occupant, alone. Nothing but size decides it.
    require(bentoSlotForArrival(tabletSlots, narrow - 1, 200) == 1,
        "An arrival that fits the narrow pane displaced the wide one");

    // No slot fits: §8 answers that arrival with a card, never by shrinking a
    // pane below what its window can show.
    require(bentoSlotForArrival(tabletSlots, wide + 1, 200) == -1
        && bentoSlotForArrival(tabletSlots, 200, tabletHeight + 1.0) == -1,
        "A slot was offered to an arrival no pane can hold");
    require(bentoSlotForArrival({}, 200, 200) == -1,
        "A layout with no slots offered one");

    // Connected: the slot of the window used longest ago yields, whatever its
    // size, so long as the arrival fits it.
    require(bentoSlotUsedLongestAgo(tabletSlots, {7, 3}, 200, 200) == 1,
        "A full Connected layout did not give up the window used longest ago");
    require(bentoSlotUsedLongestAgo(tabletSlots, {3, 7}, 200, 200) == 0,
        "A full Connected layout gave up a window used more recently");
    // The window used longest ago keeps its pane when the arrival cannot fit
    // it; the next one the arrival fits yields instead.
    require(bentoSlotUsedLongestAgo(tabletSlots, {7, 3}, narrow + 1, 200) == 0,
        "An arrival was given a pane too small for it");
    require(bentoSlotUsedLongestAgo(tabletSlots, {5, 5}, 200, 200) == 0,
        "Two windows used alike did not yield the earlier slot");
    require(bentoSlotUsedLongestAgo(tabletSlots, {7, 3}, wide + 1, 200) == -1
        && bentoSlotUsedLongestAgo(tabletSlots, {7}, 200, 200) == -1,
        "A slot was offered with no fit, or without a use for every slot");
    // The snapped card's slot never gives way, though used longest ago, and
    // an arrival only it fits is offered none.
    require(bentoSlotUsedLongestAgo(tabletSlots, {3, 7}, 200, 200, 0) == 1
        && bentoSlotUsedLongestAgo(tabletSlots, {7, 3}, 200, 200, 0) == 1,
        "A full Connected layout gave up the snapped card's slot");
    require(bentoSlotUsedLongestAgo(tabletSlots, {7, 3}, narrow + 1, 200, 0) == -1,
        "An arrival only the snapped card's slot fits was offered it");

    // CARD-LIFECYCLE.md §5: a side release onto a full layout displaces the
    // pane occupying that side. With a full-height pane on the left and a
    // stack on the right, separated by gaps, the left release names the
    // full-height pane wherever it lands, and the right release names the
    // stacked pane the contact's half names.
    const std::vector<BentoPixelRect> beside{
        {8, 8, 1200, 1584}, {1216, 8, 1376, 788}, {1216, 804, 1376, 788}};
    require(bentoPaneOnSide(beside, false, true) == 0 && bentoPaneOnSide(beside, false, false) == 0,
        "A left release did not name the pane against the left edge");
    require(bentoPaneOnSide(beside, true, true) == 1,
        "An upper right release did not name the upper pane of the right stack");
    require(bentoPaneOnSide(beside, true, false) == 2,
        "A lower right release did not name the lower pane of the right stack");
    // Order carries no meaning: the same shape listed another way names the
    // same panes, and a pane one pixel short of the edge still occupies it.
    const std::vector<BentoPixelRect> reordered{
        {1216, 804, 1375, 788}, {8, 8, 1200, 1584}, {1216, 8, 1376, 788}};
    require(bentoPaneOnSide(reordered, true, false) == 0
        && bentoPaneOnSide(reordered, true, true) == 2
        && bentoPaneOnSide(reordered, false, false) == 1,
        "Which pane occupies a side turned on the order the panes were listed in");
    // Three across: only the outer pane on each side occupies it.
    const std::vector<BentoPixelRect> across{
        {0, 0, 800, 1600}, {800, 0, 1000, 1600}, {1800, 0, 800, 1600}};
    require(bentoPaneOnSide(across, false, false) == 0 && bentoPaneOnSide(across, true, true) == 2,
        "A middle pane was named as occupying a side");
    require(bentoPaneOnSide({}, true, true) == -1,
        "A layout with no panes named one on a side");

    // Zones drawn with KDE's tile editor: a quarter, a half and a quarter of
    // a 2560 by 1440 monitor.
    const std::vector<BentoPixelRect> zones{{0, 0, 640, 1440}, {640, 0, 1280, 1440}, {1920, 0, 640, 1440}};
    const BentoCandidate any{100, 100, 800, 600, true};
    const BentoCandidate plain{100, 100, 800, 600, false};
    const BentoCandidate halfOnly{900, 100, 1000, 600, false};
    {
        // The first window takes the largest zone; one window leaves two empty.
        const auto lone = chooseZoneAdmission({any}, zones);
        require(lone && lone->candidateIndices == std::vector<int>{0} && lone->zoneIndices == std::vector<int>{1},
            "A lone window did not take the largest zone");
        // Three windows fill three zones, each in one.
        const auto three = chooseZoneAdmission({any, plain, plain}, zones);
        require(three && three->candidateIndices.size() == 3
            && std::set<int>(three->zoneIndices.begin(), three->zoneIndices.end()).size() == 3,
            "Three windows did not fill three zones");
        // A fourth has no zone and is left out; the earliest three are shown.
        const auto four = chooseZoneAdmission({any, plain, plain, plain}, zones);
        require(four && four->candidateIndices == std::vector<int>({0, 1, 2}),
            "A window past the last zone was shown, or an earlier one left out");
        // A window too wide for a quarter goes to the half, whoever is first.
        const auto fit = chooseZoneAdmission({any, halfOnly}, zones);
        require(fit && fit->candidateIndices == std::vector<int>({0, 1}) && fit->zoneIndices[1] == 1,
            "A window too wide for a quarter was not given the half");
        // Two windows that need the half cannot both be shown: the first is.
        const auto clash = chooseZoneAdmission({halfOnly, halfOnly}, zones);
        require(clash && clash->candidateIndices == std::vector<int>{0},
            "Two windows needing one zone were both shown");
        // An arrival that must be shown is, and the residents keep their zones.
        const auto kept = chooseZoneAdmission({any, plain, plain}, zones, {-1, 0, 2}, 0);
        require(kept && kept->zoneIndices == std::vector<int>({1, 0, 2}),
            "An arrival moved a resident out of its zone");
        // A resident's zone is kept even when the arrival is first.
        const auto stay = chooseZoneAdmission({any, plain}, zones, {-1, 1}, 0);
        require(stay && stay->zoneIndices[1] == 1 && stay->zoneIndices[0] != 1,
            "An arrival took the zone a resident holds");
        // A required window that fits no zone makes no admission.
        const BentoCandidate huge{3000, 100, 3000, 600, false};
        require(!chooseZoneAdmission({plain, huge}, zones, {}, 1),
            "A required window that fits no zone was admitted");
        require(!chooseZoneAdmission({}, zones) && !chooseZoneAdmission({plain}, {}),
            "An admission was made with no windows or no zones");
    }

    {
        // CARD-LIFECYCLE.md §11, Connected: a side snap names a place. Under
        // adds a card to the bottom of that side's column, beside adds a
        // full-height column at that edge.
        using Columns = BentoColumns<int>;
        const auto same = [](const BentoRect &a, const BentoRect &b) {
            return std::abs(a.x - b.x) < .001 && std::abs(a.y - b.y) < .001
                && std::abs(a.width - b.width) < .001 && std::abs(a.height - b.height) < .001;
        };
        const auto placeOf = [](const Columns &layout, int window) -> std::optional<BentoRect> {
            std::vector<int> handles;
            std::vector<BentoRect> rects;
            bentoColumnsLayout(layout, handles, rects);
            for (std::size_t i = 0; i < handles.size(); ++i)
                if (handles[i] == window) return rects[i];
            return std::nullopt;
        };
        const auto shape = [](const Columns &layout) {
            std::vector<std::vector<int>> cards;
            for (const auto &column : layout.columns) {
                cards.emplace_back();
                for (const auto &slot : column) cards.back().push_back(slot.open ? 0 : slot.window);
            }
            return cards;
        };
        const BentoSidePlacement rightUnder{true, false, true}, rightBeside{true, true, true};
        const BentoSidePlacement leftUnder{false, false, true}, leftBeside{false, true, true};

        // Four cards in columns, none widened and no room open, stand as two
        // columns of two, in the order they stood.
        Columns thirds;
        thirds.columns = {{{1, false}}, {{2, false}}, {{3, false}, {4, false}}};
        const auto quartered = bentoColumnsQuartered(thirds);
        require(quartered && shape(*quartered) == std::vector<std::vector<int>>{{1, 2}, {3, 4}},
            "Four cards in columns did not stand in quarters");
        require(placeOf(*quartered, 3) && same(*placeOf(*quartered, 3), {0.5, 0.0, 0.5, 0.5}),
            "The third of four cards did not take the top right quarter");
        Columns open = thirds;
        open.columns[0].push_back({0, true});
        require(!bentoColumnsQuartered(open), "Columns with room open were quartered");
        require(!bentoColumnsQuartered(*quartered), "Quarters were quartered again");

        // Alone, beside takes the half on its side and under that half's
        // lower corner, the rest open.
        Columns alone;
        bentoColumnsPlace(alone, 1, rightBeside);
        require(placeOf(alone, 1) && same(*placeOf(alone, 1), {0.5, 0.0, 0.5, 1.0}),
            "A card placed beside on the right did not take the right half");
        alone = {};
        bentoColumnsPlace(alone, 1, leftUnder);
        require(placeOf(alone, 1) && same(*placeOf(alone, 1), {0.0, 0.5, 0.5, 0.5}),
            "A card placed under on the left did not take the lower left corner");

        // Over takes that half's upper corner alone; the next card fills the
        // room below it. With a column on that side, over goes on top of it,
        // taking room open there first.
        BentoSidePlacement leftOver{false, false, true};
        leftOver.over = true;
        Columns overAlone;
        bentoColumnsPlace(overAlone, 1, leftOver);
        require(placeOf(overAlone, 1) && same(*placeOf(overAlone, 1), {0.0, 0.0, 0.5, 0.5}),
            "A card placed over on the left did not take the upper left corner");
        require(bentoColumnsFill(overAlone, 2) && shape(overAlone) == std::vector<std::vector<int>>{{1, 2}},
            "An arrival did not fill the room below a card placed over");
        Columns overTop;
        bentoColumnsPlace(overTop, 1, leftBeside);
        bentoColumnsPlace(overTop, 2, leftOver);
        require(shape(overTop) == std::vector<std::vector<int>>{{2, 1}},
            "Over on the left did not go on top of the left column");
        Columns overRoom;
        bentoColumnsPlace(overRoom, 1, leftUnder);
        bentoColumnsPlace(overRoom, 2, leftOver);
        require(shape(overRoom) == std::vector<std::vector<int>>{{2, 1}},
            "Over on the left did not take the room open above a card placed under");

        // The edge reads as a quarter, a half and a quarter of the display.
        const auto edgeAt = [](double y, std::optional<BentoSidePlacement> before = {}) {
            return bentoConnectedSideChoice(false, y, 0.0, 1440.0, before);
        };
        require(edgeAt(100).over && !edgeAt(100).large, "The top quarter of an edge did not read as over");
        require(edgeAt(720).large && !edgeAt(720).over, "The middle of an edge did not read as beside");
        require(!edgeAt(1300).large && !edgeAt(1300).over, "The bottom quarter of an edge did not read as under");
        require(edgeAt(370, edgeAt(350)).over, "A contact resting on the upper boundary did not keep over");
        require(edgeAt(1070, edgeAt(1100)).connected && !edgeAt(1070, edgeAt(1100)).large,
            "A contact resting on the lower boundary did not keep under");

        // The product owner's sequence: a terminal under on the right, a
        // second under on the right, then a browser beside on the right.
        Columns layout;
        bentoColumnsPlace(layout, 1, rightUnder);
        require(shape(layout) == std::vector<std::vector<int>>{{0, 1}} && layout.alignRight,
            "The first terminal did not stand in the lower right corner");
        require(bentoColumnsFill(layout, 2) && shape(layout) == std::vector<std::vector<int>>{{2, 1}},
            "An arrival did not fill the room above a card placed under");
        bentoColumnsPlace(layout, 2, rightUnder);
        require(shape(layout) == std::vector<std::vector<int>>{{1, 2}},
            "A second terminal under did not join the right column below the first");
        require(same(*placeOf(layout, 1), {0.5, 0.0, 0.5, 0.5})
                && same(*placeOf(layout, 2), {0.5, 0.5, 0.5, 0.5}),
            "The right column did not share its height between the terminals");
        require(bentoColumnsFill(layout, 3) && shape(layout) == std::vector<std::vector<int>>{{3}, {1, 2}},
            "An arrival did not take the half left open");
        bentoColumnsPlace(layout, 3, rightBeside);
        require(shape(layout) == std::vector<std::vector<int>>{{1, 2}, {3}},
            "A browser beside on the right did not take its own column at the edge");
        require(same(*placeOf(layout, 3), {0.5, 0.0, 0.5, 1.0})
                && same(*placeOf(layout, 1), {0.0, 0.0, 0.5, 0.5})
                && same(*placeOf(layout, 2), {0.0, 0.5, 0.5, 0.5}),
            "The stacked terminals did not stay together left of the browser");
        require(!bentoColumnsFill(layout, 4), "A layout with no open room offered some");

        // A fourth card with no room open joins the column holding fewest,
        // and a side snap still places among the columns.
        Columns fourth = layout;
        bentoColumnsJoin(fourth, 4);
        require(shape(fourth) == std::vector<std::vector<int>>{{1, 2}, {3, 4}},
            "A fourth card did not join the column holding fewest");
        bentoColumnsPlace(fourth, 4, leftBeside);
        require(shape(fourth) == std::vector<std::vector<int>>{{4}, {1, 2}, {3}},
            "Beside on the left after a fourth card did not take the left edge");
        bentoColumnsPlace(fourth, 3, leftUnder);
        require(shape(fourth) == std::vector<std::vector<int>>{{4, 3}, {1, 2}},
            "Under on the left after a fourth card did not join the left column");

        // Under on the other side starts a column there; beside on the left
        // goes first; a card leaving opens no room in a column of others.
        bentoColumnsPlace(layout, 4, leftUnder);
        require(shape(layout) == std::vector<std::vector<int>>{{1, 2, 4}, {3}},
            "Under on the left did not join the left edge column");
        bentoColumnsPlace(layout, 5, leftBeside);
        require(shape(layout) == std::vector<std::vector<int>>{{5}, {1, 2, 4}, {3}},
            "Beside on the left did not take its own column at the left edge");
        bentoColumnsRemove(layout, 5);
        bentoColumnsRemove(layout, 3);
        require(shape(layout) == std::vector<std::vector<int>>{{1, 2, 4}} && !layout.alignRight,
            "The column left did not keep the left half");
        bentoColumnsRemove(alone, 1);
        require(alone.columns.empty(), "A column of open room outlived its last card");

        // A wheel turned while holding a card at a side edge gives its column
        // a share; the other side takes the rest, and arrivals keep it.
        {
            BentoSidePlacement narrow = leftBeside;
            narrow.share = 0.25;
            Columns linked;
            bentoColumnsPlace(linked, 2, rightBeside);
            bentoColumnsPlace(linked, 1, narrow);
            require(same(*placeOf(linked, 1), {0.0, 0.0, 0.25, 1.0})
                    && same(*placeOf(linked, 2), {0.25, 0.0, 0.75, 1.0}),
                "A terminal narrowed to a quarter did not widen the browser beside it");
            Columns lone;
            bentoColumnsPlace(lone, 1, narrow);
            require(same(*placeOf(lone, 1), {0.0, 0.0, 0.25, 1.0}),
                "A lone card narrowed to a quarter did not stand in that quarter");
            require(bentoColumnsFill(lone, 2) && same(*placeOf(lone, 2), {0.25, 0.0, 0.75, 1.0}),
                "An arrival beside a narrowed card did not take the rest");
            bentoColumnsPlace(lone, 3, leftUnder);
            require(same(*placeOf(lone, 3), {0.0, 0.5, 0.25, 0.5}),
                "A card under a narrowed column did not keep its width");
            bentoColumnsPlace(lone, 1, rightBeside);
            require(lone.shares.empty(), "A card snapped again with no wheel kept its share");
            Columns crowded;
            BentoSidePlacement wide = rightBeside;
            wide.share = 0.8;
            bentoColumnsPlace(crowded, 1, leftBeside);
            bentoColumnsPlace(crowded, 3, wide);
            bentoColumnsPlace(crowded, 2, narrow);
            require(same(*placeOf(crowded, 2), {0.0, 0.0, 0.25, 1.0})
                    && same(*placeOf(crowded, 1), {0.25, 0.0, 0.375, 1.0}),
                "The newest share did not win over an older one that left no room");
            bentoColumnsRemove(linked, 2);
            require(linked.shares.size() == 1 && same(*placeOf(linked, 1), {0.0, 0.0, 0.25, 1.0}),
                "A narrowed card left alone did not keep its width");
            bentoColumnsRemove(linked, 1);
            require(linked.shares.empty(), "A share outlived its card");
        }

        // The room a layout leaves open, in the order a card arriving fills
        // it: any open slot, then beside a lone column the rest of the display.
        {
            Columns lone;
            BentoSidePlacement narrow = rightBeside;
            narrow.share = 0.25;
            bentoColumnsPlace(lone, 1, narrow);
            const auto rooms = bentoColumnsOpenRooms(lone);
            require(rooms.size() == 1 && same(rooms[0], {0.0, 0.0, 0.75, 1.0}),
                "A lone column on the right did not leave the rest of the display open");
            Columns under;
            bentoColumnsPlace(under, 1, leftUnder);
            const auto both = bentoColumnsOpenRooms(under);
            require(both.size() == 2 && same(both[0], {0.0, 0.0, 0.5, 0.5}) && same(both[1], {0.5, 0.0, 0.5, 1.0}),
                "A card under on the left did not leave the room above, then the other half, open");
            require(bentoColumnsFill(under, 2) && same(bentoColumnsOpenRooms(under)[0], {0.5, 0.0, 0.5, 1.0}),
                "A card filled other than the first room open");
            require(bentoColumnsFill(under, 3) && bentoColumnsOpenRooms(under).empty(),
                "A layout with every room filled still left some open");
        }

        // Like a volume knob: turned slowly it catches at each stop and rests
        // there a notch; cranked it passes through.
        {
            const auto turn = [](double from, int notches, bool crank, int times = 1) {
                WheelWidthKnob knob{from};
                for (int i = 0; i < times; ++i)
                    knob.turn(notches, crank, WheelWidthLeast, WheelWidthSideMost, WheelWidthSideStops);
                return knob.width;
            };
            const auto near = [](double a, double b) { return std::abs(a - b) < 1e-9; };
            require(near(turn(0.5, -1, false), 0.5 - WheelWidthStep),
                "A slow notch off a stop did not move one step");
            require(near(turn(0.5, -4, false), 1.0 / 3.0), "A slow turn did not catch at a third");
            require(near(turn(0.5, -1, false, 4), 1.0 / 3.0)
                    && near(turn(0.5, -1, false, 5), 1.0 / 3.0)
                    && near(turn(0.5, -1, false, 6), 1.0 / 3.0 - WheelWidthStep),
                "Slow notches did not rest a notch at a third, then move on");
            require(near(turn(0.5, -1, true, 4), 0.5 - 4 * WheelWidthStep), "A crank was caught at a stop");
            require(near(turn(0.5, -1, true, 20), WheelWidthLeast), "A crank went past the narrowest width");
            require(near(turn(0.5, 1, false, 30), WheelWidthSideMost),
                "Turning away did not stop at the widest share");
            WheelWidthKnob top;
            for (int i = 0; i < 7; ++i) top.turn(-1, false, 0.4, 1.0, WheelWidthTopStops);
            require(near(top.width, 0.75), "Seven slow notches at the top edge did not rest at three quarters");
            WheelWidthKnob least{0.5};
            least.turn(-1, true, 0.5, 1.0, WheelWidthTopStops);
            require(near(least.width, 0.5), "The app's own minimum did not hold the width");
        }

        // Any other shape is dealt into as many columns as rows.
        require(shape(bentoColumnsDealt<int>({1, 2, 3, 4, 5}))
                    == std::vector<std::vector<int>>{{1, 4}, {2, 5}, {3}},
            "Five cards were not dealt into three columns");
        require(shape(bentoColumnsDealt<int>({1, 2, 3, 4}))
                    == std::vector<std::vector<int>>{{1, 3}, {2, 4}},
            "Four cards were not dealt into two columns");

        // A layout on screen reads as columns only when it is one.
        const auto read = bentoColumnsFromRects<int>({1, 2, 3},
            {{0.0, 0.0, 0.5, 0.5}, {0.0, 0.5, 0.5, 0.5}, {0.5, 0.0, 0.5, 1.0}});
        require(read && shape(*read) == std::vector<std::vector<int>>{{1, 2}, {3}},
            "A row of columns on screen did not read as columns");
        require(!bentoColumnsFromRects<int>({1, 2, 3},
                    {{0.0, 0.0, 1.0, 0.5}, {0.0, 0.5, 0.5, 0.5}, {0.5, 0.5, 0.5, 0.5}}),
            "A layout split across did read as columns");

        // A pick paired with the card in use on a 1900x980 stage: two even
        // columns, the pick on the right. A third widens to thirds, the first
        // two keeping their order, and a fourth joins the shortest column,
        // rightmost, so the first two keep their places too.
        Columns pair;
        pair.columns = {{{1, false}}, {{2, false}}};
        pair.paired = true;
        const auto pixelOf = [&](const Columns &layout, int window) {
            std::vector<int> handles;
            std::vector<BentoRect> rects;
            bentoColumnsLayout(layout, handles, rects);
            const auto pixels = makePixelBentoLayout(rects, 10, 34, 1900, 980);
            for (std::size_t i = 0; i < handles.size(); ++i)
                if (handles[i] == window) return pixels[i];
            return BentoPixelRect{};
        };
        const auto at = [&](const Columns &layout, int window, BentoPixelRect want) {
            const auto got = pixelOf(layout, window);
            return got.x == want.x && got.y == want.y && got.width == want.width
                && got.height == want.height;
        };
        require(at(pair, 1, {10, 34, 945, 980}) && at(pair, 2, {965, 34, 945, 980}),
            "A Connected pair did not share the width evenly");
        pair.columns.push_back({{3, false}});
        require(at(pair, 1, {10, 34, 628, 980}) && at(pair, 2, {648, 34, 623, 980})
                && at(pair, 3, {1282, 34, 628, 980}),
            "A third column did not share the width in thirds");
        bentoColumnsJoin(pair, 4);
        require(shape(pair) == std::vector<std::vector<int>>{{1}, {2}, {3, 4}}
                && at(pair, 1, {10, 34, 628, 980}) && at(pair, 2, {648, 34, 623, 980})
                && at(pair, 3, {1282, 34, 628, 485}) && at(pair, 4, {1282, 529, 628, 485}),
            "A fourth card moved the first two, or did not join the rightmost column");
        // Columns a pair began stay so through arrivals and leavings.
        bentoColumnsRemove(pair, 2);
        require(pair.paired && shape(pair) == std::vector<std::vector<int>>{{1}, {3, 4}},
            "Columns a pair began forgot it when a card left");

        // Beside, a column of its own shares the width evenly.
        const auto even = bentoSideLayout({true, true, true}, 2560, 1400,
            {400, 300, 1, 1, false}, std::optional<BentoCandidate>{BentoCandidate{400, 300, 1, 1, false}});
        require(even && even->size() == 2 && std::abs((*even)[0].width - 0.5) < .001,
            "A Connected side snap did not share the width evenly");
    }

    {
        // CARD-LIFECYCLE.md §11, Connected: arrivals fit in at the sizes their
        // kinds want (NaturalSize.h), never into equal strips.
        using Columns = BentoColumns<int>;
        const QSizeF hd(1920, 1080), qhd(2560, 1440);
        const auto needOn = [](AppKind kind, QSizeF area) {
            return naturalNeed(naturalSizeFor(kind, area), QSizeF(400, 300), area);
        };
        std::map<int, AppKind> kinds{{1, AppKind::Browser}, {2, AppKind::Terminal},
                                     {3, AppKind::Files}, {4, AppKind::Document}};
        const auto shape = [](const Columns &layout) {
            std::vector<std::vector<int>> cards;
            for (const auto &column : layout.columns) {
                cards.emplace_back();
                for (const auto &slot : column) cards.back().push_back(slot.open ? 0 : slot.window);
            }
            return cards;
        };
        const auto near = [](double a, double b) { return std::abs(a - b) < .002; };
        for (const QSizeF area : {hd, qhd}) {
            const auto needOf = [&](int window) { return needOn(kinds.at(window), area); };
            Columns layout;
            layout.columns = {{{1, false}}};
            require(bentoColumnsFitNatural(layout, 2, needOf(2), needOf, 0) == NaturalFit::Opened
                    && shape(layout) == std::vector<std::vector<int>>{{1}, {2, 0}},
                "A terminal beside a browser did not take the top of a column of its own");
            const auto widths = bentoColumnsWidths(layout);
            require(near(widths[0], .5), "Standing columns moved when the layout turned natural");
            require(bentoColumnsFitNatural(layout, 3, needOf(3), needOf, 0) == NaturalFit::Filled
                    && shape(layout) == std::vector<std::vector<int>>{{1}, {2, 3}},
                "Files did not fill the room under the terminal");
        }
        {
            // A browser alone at its page width, then a terminal and files:
            // one large window and two small, room too thin to use given away.
            const auto needOf = [&](int window) { return needOn(kinds.at(window), hd); };
            Columns layout;
            layout.columns = {{{1, false}}};
            layout.natural = true;
            layout.shares = {{1, 1280.0 / 1920.0}};
            require(bentoColumnsFitNatural(layout, 2, needOf(2), needOf, 0) == NaturalFit::Opened,
                "A terminal found no room beside a browser at its page width");
            auto widths = bentoColumnsWidths(layout);
            require(widths.size() == 2 && near(widths[0], 1280.0 / 1920.0) && near(widths[1], 640.0 / 1920.0),
                "The browser gave up width, or the terminal did not take the rest");
            require(bentoColumnsFitNatural(layout, 3, needOf(3), needOf, 0) == NaturalFit::Filled
                    && shape(layout) == std::vector<std::vector<int>>{{1}, {2, 3}},
                "Three windows were not one large and two small");
            require(bentoColumnsOpenRooms(layout).empty(), "Room was left open in a full layout");
            // No room left at natural sizes: nothing changes, and the caller
            // gives up the pane used longest ago.
            const Columns full = layout;
            require(bentoColumnsFitNatural(layout, 4, needOf(4), needOf, 0) == NaturalFit::None
                    && layout == full,
                "A document squeezed in below the sizes the others want");
        }
        {
            // A wider display leaves room open beside natural columns.
            const auto needOf = [&](int window) { return needOn(kinds.at(window), qhd); };
            Columns layout;
            layout.columns = {{{1, false}}};
            layout.natural = true;
            layout.shares = {{1, .5}};
            require(bentoColumnsFitNatural(layout, 2, needOf(2), needOf, 0) == NaturalFit::Opened,
                "A terminal found no room on a wide display");
            require(bentoColumnsFitNatural(layout, 3, needOf(3), needOf, 0) == NaturalFit::Filled,
                "Files did not take the room under the terminal");
            const auto widths = bentoColumnsWidths(layout);
            require(near(widths[1], 960.0 / 2560.0), "The column did not widen to what files want");
            const auto rooms = bentoColumnsOpenRooms(layout);
            require(rooms.size() == 1 && near(rooms[0].width, 320.0 / 2560.0) && near(rooms[0].x, 1 - 320.0 / 2560.0),
                "The room beside the columns was not left open");
            // A card leaving keeps its column's width, and a column going
            // leaves its width open.
            bentoColumnsRemove(layout, 2);
            require(near(bentoColumnsWidths(layout)[1], 960.0 / 2560.0)
                    && shape(layout) == std::vector<std::vector<int>>{{1}, {0, 3}},
                "A column narrowed, or closed up, when a card left it");
            bentoColumnsRemove(layout, 3);
            require(layout.columns.size() == 1 && bentoColumnsOpenRooms(layout).size() == 1
                    && near(bentoColumnsOpenRooms(layout)[0].width, .5),
                "A column that went did not leave its width open");
        }
        {
            // Short windows join a column of short windows when nothing else
            // fits; a tall one never does.
            const auto needOf = [&](int window) { return needOn(window == 1 ? AppKind::Browser : AppKind::Terminal, hd); };
            Columns layout;
            layout.columns = {{{1, false}}, {{2, false}, {3, false}}};
            layout.natural = true;
            layout.shares = {{1, 1280.0 / 1920.0}, {2, 640.0 / 1920.0}};
            require(bentoColumnsFitNatural(layout, 5, needOf(5), needOf, 1) == NaturalFit::Joined
                    && shape(layout) == std::vector<std::vector<int>>{{1}, {2, 3, 5}},
                "A terminal did not join the column of terminals");
        }
        {
            // The wheel sets a held pane's column width: open room first,
            // then the others' spare width, never past what they allow.
            Columns layout;
            layout.columns = {{{1, false}}, {{2, false}, {3, false}}};
            layout.natural = true;
            layout.shares = {{1, .5}, {2, .3}};
            const auto least = [](int) { return .2; };
            require(bentoColumnsResize(layout, 2, .4, least)
                    && near(bentoColumnsWidths(layout)[0], .5) && near(bentoColumnsWidths(layout)[1], .4),
                "Widening a column did not take the open room first");
            require(bentoColumnsResize(layout, 3, .7, least)
                    && near(bentoColumnsWidths(layout)[0], .3) && near(bentoColumnsWidths(layout)[1], .7),
                "Widening past the open room did not take the other column's spare width");
            require(bentoColumnsResize(layout, 1, 1.0, least)
                    && near(bentoColumnsWidths(layout)[0], .8) && near(bentoColumnsWidths(layout)[1], .2),
                "A column was widened past what the others allow");
            require(!bentoColumnsResize(layout, 9, .5, least), "A window the layout does not hold was resized");
        }
        require(appKindFromCategories({"System", "TerminalEmulator"}) == AppKind::Terminal
                && appKindFromCategories({"Network", "WebBrowser"}) == AppKind::Browser
                && appKindFromCategories({"Qt", "KDE", "System", "FileManager"}) == AppKind::Files
                && appKindFromCategories({"Utility"}) == AppKind::Other,
            "An application's kind was misread from its categories");
        require(!naturalSizeFor(AppKind::Other, QSizeF(1920, 1080))
                && naturalSizeFor(AppKind::Browser, QSizeF(1024, 700)) == QSizeF(1024, 700),
            "A natural size was given where none is known, or exceeded the area");
    }

    std::cout << "Bento layout/admission checks passed\n";
    return 0;
}
