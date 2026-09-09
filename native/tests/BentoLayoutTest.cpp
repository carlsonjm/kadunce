/*
    SPDX-FileCopyrightText: 2026 Warbler Studio contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "BentoLayout.h"

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

    std::cout << "Bento layout/admission checks passed\n";
    return 0;
}
