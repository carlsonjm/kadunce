/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ProjectedCardGeometry.h"

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
    const Kadunce::CardRect slot{120.0, 80.0, 640.0, 360.0};
    const auto landscape = Kadunce::makeProjectedCardVisualRect(
        slot, 1600.0, 900.0);
    const auto portrait = Kadunce::makeProjectedCardVisualRect(
        slot, 720.0, 1200.0);
    const auto square = Kadunce::makeProjectedCardVisualRect(
        slot, 800.0, 800.0);

    for (const auto &visual : {landscape, portrait, square}) {
        require(visual.x >= slot.x && visual.right() <= slot.right()
                    && visual.y >= slot.y && visual.bottom() <= slot.bottom(),
                "Projected card visual escaped its canonical slot");
        require(close(visual.x + visual.width / 2.0,
                      slot.x + slot.width / 2.0)
                    && close(visual.y + visual.height / 2.0,
                             slot.y + slot.height / 2.0),
                "Projected card visual is not centered");
    }
    require(close(landscape.width / landscape.height, 1600.0 / 900.0)
                && (close(landscape.width, slot.width)
                    || close(landscape.height, slot.height)),
            "Landscape projected card visual is not proportional and maximal");
    require(close(portrait.width / portrait.height, 720.0 / 1200.0)
                && (close(portrait.width, slot.width)
                    || close(portrait.height, slot.height)),
            "Portrait projected card visual is not proportional and maximal");
    require(close(square.width / square.height, 1.0)
                && (close(square.width, slot.width)
                    || close(square.height, slot.height)),
            "Square projected card visual is not proportional and maximal");

    const auto invalid = Kadunce::makeProjectedCardVisualRect(slot, 0.0, 900.0);
    require(close(invalid.x, slot.x) && close(invalid.y, slot.y)
                && close(invalid.width, slot.width)
                && close(invalid.height, slot.height),
            "Invalid projected source changed the canonical slot");
}
