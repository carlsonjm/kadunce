/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BentoCompositeGeometry.h"

#include <array>
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
    const Kadunce::CardRect workspace{0, 0, 1600, 900};
    const std::array<Kadunce::CardRect, 3> frames{
        Kadunce::CardRect{10, 10, 1038, 870},
        Kadunce::CardRect{1062, 10, 528, 428},
        Kadunce::CardRect{1062, 452, 528, 428},
    };
    const auto composite = Kadunce::makeBentoCompositeGeometry(slot, workspace);
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
    require(close(composite.sourceUnion.x, workspace.x)
        && close(composite.sourceUnion.y, workspace.y)
        && close(composite.sourceUnion.width, workspace.width)
        && close(composite.sourceUnion.height, workspace.height),
        "Composite replaced the full work area with a pane union");

    std::vector<Kadunce::CardRect> mapped;
    for (const auto &frame : frames)
        mapped.push_back(Kadunce::mapBentoCompositeRect(composite, frame));
    require(close(mapped[1].x - mapped[0].x,
                  (frames[1].x - frames[0].x) * composite.scale)
        && close(mapped[2].y - mapped[1].y,
                 (frames[2].y - frames[1].y) * composite.scale),
        "Composite changed relative pane placement");
    require(close(mapped[0].x - composite.targetUnion.x, 10 * composite.scale)
        && close(mapped[0].y - composite.targetUnion.y, 10 * composite.scale)
        && close(composite.targetUnion.right() - mapped[2].right(), 10 * composite.scale)
        && close(composite.targetUnion.bottom() - mapped[2].bottom(), 20 * composite.scale),
        "Work-area gutters or dock clearance were not preserved");
    require(close(mapped[1].x - mapped[0].right(), 14 * composite.scale)
        && close(mapped[2].y - mapped[1].bottom(), 14 * composite.scale),
        "Inter-pane gaps were not preserved");
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

    const Kadunce::BentoRect wideRect{0, 0, 2.0 / 3.0, 1};
    const Kadunce::CardRect driftedFrame{4300, 2100, 700, 300};
    const Kadunce::CardRect driftedExpanded{4288, 2082, 730, 334};
    const auto projected = Kadunce::makeBentoProjectedPaneGeometry(composite,
        workspace, wideRect, driftedFrame, driftedExpanded);
    require(projected.has_value(), "Stored Bento pane could not be projected");
    const auto stage = Kadunce::makeBentoStageArea(workspace);
    const auto expectedPixels = Kadunce::makePixelBentoLayout({wideRect},
        int(stage.x), int(stage.y), int(stage.width), int(stage.height));
    require(close(projected->authoritativeFrame.x, expectedPixels[0].x)
        && close(projected->authoritativeFrame.y, expectedPixels[0].y)
        && close(projected->authoritativeFrame.width, expectedPixels[0].width)
        && close(projected->authoritativeFrame.height, expectedPixels[0].height),
        "Live frame drift replaced the stored Bento rect");
    require(close(projected->authoritativeSurface.x,
                  projected->authoritativeFrame.x - 12)
        && close(projected->authoritativeSurface.y,
                 projected->authoritativeFrame.y - 18)
        && close(projected->authoritativeSurface.right(),
                 projected->authoritativeFrame.right() + 18)
        && close(projected->authoritativeSurface.bottom(),
                 projected->authoritativeFrame.bottom() + 16),
        "Decoration margins were not applied around the authoritative frame");

    const auto repeated = Kadunce::makeBentoProjectedPaneGeometry(composite,
        workspace, wideRect, {9000, 6000, 300, 900},
        {8988, 5982, 330, 934});
    require(repeated.has_value()
        && close(repeated->targetSurface.x, projected->targetSurface.x)
        && close(repeated->targetSurface.y, projected->targetSurface.y)
        && close(repeated->targetSurface.width, projected->targetSurface.width)
        && close(repeated->targetSurface.height, projected->targetSurface.height),
        "Repeated projection accumulated live frame drift");

    const Kadunce::BentoRect smallRect{2.0 / 3.0, 0, 1.0 / 3.0, .5};
    const auto small = Kadunce::makeBentoProjectedPaneGeometry(composite,
        workspace, smallRect, {100, 100, 80, 700}, {90, 90, 100, 720});
    require(small.has_value()
        && small->authoritativeFrame.x > projected->authoritativeFrame.x
        && small->authoritativeFrame.width < projected->authoritativeFrame.width
        && small->authoritativeFrame.height < projected->authoritativeFrame.height,
        "Stored small pane did not retain its relative shape");

    const auto fullWithShadow = Kadunce::makeBentoProjectedPaneGeometry(composite,
        workspace, {0, 0, 1, 1}, {50, 50, 400, 300}, {20, 20, 460, 360});
    require(fullWithShadow.has_value()
        && close(fullWithShadow->targetClip.x, composite.targetUnion.x)
        && close(fullWithShadow->targetClip.y, composite.targetUnion.y)
        && close(fullWithShadow->targetClip.right(), composite.targetUnion.right())
        && close(fullWithShadow->targetClip.bottom(), composite.targetUnion.bottom()),
        "Projected pane shadow was not clipped to the workspace card");

    const auto extremeComposite = Kadunce::makeBentoCompositeGeometry(
        slot, {0, 0, 2400, 120});
    require(extremeComposite.valid()
        && extremeComposite.targetUnion.right() <= slot.right()
        && extremeComposite.targetUnion.bottom() <= slot.bottom(),
        "Extreme mixed-aspect composite escaped its slot");
    require(!Kadunce::makeBentoCompositeGeometry(slot, {}).valid(),
        "Empty work area unexpectedly became valid");
    require(!Kadunce::makeBentoCompositeGeometry(slot, {0, 0, 0, 10}).valid(),
        "Invalid work area unexpectedly became valid");
}
