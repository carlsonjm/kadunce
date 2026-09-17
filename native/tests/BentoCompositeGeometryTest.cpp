/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BentoCompositeGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
void require(bool condition, const char *message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
}
bool close(double a, double b) { return std::abs(a - b) < 0.0001; }
}

int main()
{
    const Kadunce::CardRect slot{100, 50, 600, 400};
    const std::vector<Kadunce::CardRect> frames{
        {20, 10, 900, 700}, {940, 10, 460, 340}, {940, 370, 460, 340},
    };
    const auto composite = Kadunce::makeBentoCompositeGeometry(slot, frames);
    require(composite.valid(), "Valid Bento composite was rejected");
    require(composite.targetUnion.x >= slot.x && composite.targetUnion.y >= slot.y
        && composite.targetUnion.right() <= slot.right()
        && composite.targetUnion.bottom() <= slot.bottom(),
        "Composite escaped canonical Card Line slot");
    require(close(composite.targetUnion.x + composite.targetUnion.width / 2.0,
                  slot.x + slot.width / 2.0)
        && close(composite.targetUnion.y + composite.targetUnion.height / 2.0,
                 slot.y + slot.height / 2.0),
        "Composite did not remain centered");

    std::vector<Kadunce::CardRect> mapped;
    for (const auto &frame : frames)
        mapped.push_back(Kadunce::mapBentoCompositeRect(composite, frame));
    require(close(mapped[1].x - mapped[0].x,
                  (frames[1].x - frames[0].x) * composite.scale)
        && close(mapped[2].y - mapped[1].y,
                 (frames[2].y - frames[1].y) * composite.scale),
        "Composite changed relative pane placement");
    require(close(mapped[0].width / mapped[0].height,
                  frames[0].width / frames[0].height)
        && close(mapped[1].width / mapped[1].height,
                 frames[1].width / frames[1].height),
        "Composite changed pane aspect ratios");

    const Kadunce::CardRect expanded{frames[1].x - 14, frames[1].y - 20,
                                    frames[1].width + 28, frames[1].height + 34};
    const auto mappedExpanded = Kadunce::mapBentoCompositeRect(composite, expanded);
    require(close(mappedExpanded.x, mapped[1].x - 14 * composite.scale)
        && close(mappedExpanded.y, mapped[1].y - 20 * composite.scale),
        "Expanded live surface lost its frame-relative offset");

    const std::vector<Kadunce::CardRect> extreme{{0, 0, 2400, 120}, {0, 140, 180, 900}};
    const auto extremeComposite = Kadunce::makeBentoCompositeGeometry(slot, extreme);
    require(extremeComposite.valid()
        && extremeComposite.targetUnion.right() <= slot.right()
        && extremeComposite.targetUnion.bottom() <= slot.bottom(),
        "Extreme mixed-aspect composite escaped its slot");
    require(!Kadunce::makeBentoCompositeGeometry(slot, {}).valid(),
        "Empty composite unexpectedly became valid");
    const std::vector<Kadunce::CardRect> invalid{{0, 0, 0, 10}};
    require(!Kadunce::makeBentoCompositeGeometry(slot, invalid).valid(),
        "Invalid member unexpectedly became valid");
}
