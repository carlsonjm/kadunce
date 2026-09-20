/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BentoProjectionSession.h"

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char *message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
}
}

int main()
{
    Kadunce::BentoProjectionShape shape{
        .workspaceArea = {0, 0, 1280, 740},
        .panes = {1, 2, 3},
        .sleeping = {4, 5},
        .stackingOrder = {4, 1, 3, 2, 5},
        .rects = {{0, 0, .6, 1}, {.6, 0, .4, .5}, {.6, .5, .4, .5}},
        .lead = 1,
    };
    require(Kadunce::validBentoProjectionShape(shape),
        "Valid pane/sleeping projection shape was rejected");

    auto invalid = shape;
    invalid.rects.pop_back();
    require(!Kadunce::validBentoProjectionShape(invalid),
        "Mismatched pane geometry was accepted");
    invalid = shape; invalid.sleeping.push_back(2);
    require(!Kadunce::validBentoProjectionShape(invalid),
        "Duplicate pane/sleeping identity was accepted");
    invalid = shape; invalid.lead = 5;
    require(!Kadunce::validBentoProjectionShape(invalid),
        "A member that is not a visible pane was accepted as the lead");
    invalid = shape; invalid.stackingOrder.push_back(99);
    require(!Kadunce::validBentoProjectionShape(invalid),
        "A stacking order naming a window that is no member was accepted");
    invalid = shape; invalid.stackingOrder.pop_back();
    require(!Kadunce::validBentoProjectionShape(invalid),
        "Incomplete authoritative stacking order was accepted");
    invalid = shape; invalid.hasSide = true; invalid.sideWindow = 99;
    require(!Kadunce::validBentoProjectionShape(invalid),
        "Unknown side member was accepted");
    shape.hasSide = true; shape.sideWindow = 3;
    require(Kadunce::validBentoProjectionShape(shape),
        "Valid side-member metadata was rejected");
    invalid = shape; invalid.workspaceArea = {};
    require(!Kadunce::validBentoProjectionShape(invalid),
        "Projection without an authoritative work area was accepted");
}
