/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "BentoLayout.h"
#include "BentoSidePlacement.h"
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
        && bentoPaneCap(monitorWidth, monitorHeight) == 8,
        "The tablet work area does not cap at two panes, or a monitor lost the library");
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
    std::cout << "Bento layout/admission checks passed\n";
    return 0;
}
