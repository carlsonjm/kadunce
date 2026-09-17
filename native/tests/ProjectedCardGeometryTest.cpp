/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ProjectedCardGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <utility>
#include <vector>

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

std::pair<double, double> rotatedHorizontalBounds(
    const Kadunce::CardRect &rect, double rotation)
{
    const double radians = rotation * std::numbers::pi / 180.0;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    double minimum = rect.right();
    double maximum = rect.x;
    for (const auto &[x, y] : std::array{
             std::array{rect.x, rect.y},
             std::array{rect.right(), rect.y},
             std::array{rect.right(), rect.bottom()},
             std::array{rect.x, rect.bottom()},
         }) {
        const double transformed = rect.right()
            + cosine * (x - rect.right())
            - sine * (y - rect.bottom());
        minimum = std::min(minimum, transformed);
        maximum = std::max(maximum, transformed);
    }
    return {minimum, maximum};
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

    const std::vector<double> mixedAspects{
        16.0 / 9.0, 9.0 / 16.0, 1.0, 3.2,
    };
    const auto policy = Kadunce::makeProjectedStackPreviewPolicy(
        slot, mixedAspects);
    require(policy.valid() && close(policy.widestAspect, 3.2)
                && close(policy.commonHeight, 200.0),
            "Mixed projected stack did not derive height from widest aspect");

    const auto normalizedLandscape =
        Kadunce::makeNormalizedProjectedCardVisualRect(
            slot, 1600.0, 900.0, policy);
    const auto normalizedPortrait =
        Kadunce::makeNormalizedProjectedCardVisualRect(
            slot, 900.0, 1600.0, policy);
    const auto normalizedSquare =
        Kadunce::makeNormalizedProjectedCardVisualRect(
            slot, 1000.0, 1000.0, policy);
    const auto normalizedExtreme =
        Kadunce::makeNormalizedProjectedCardVisualRect(
            slot, 3200.0, 1000.0, policy);
    const double expectedCenterX = slot.x + slot.width / 2.0;
    const double expectedCenterY = slot.y + slot.height / 2.0;
    const double expectedBaseline = expectedCenterY + policy.commonHeight / 2.0;
    for (const auto &visual : {normalizedLandscape, normalizedPortrait,
             normalizedSquare, normalizedExtreme}) {
        require(close(visual.height, policy.commonHeight),
                "Projected stack member did not retain common height");
        require(visual.x >= slot.x && visual.right() <= slot.right()
                    && visual.y >= slot.y && visual.bottom() <= slot.bottom(),
                "Normalized projected stack member escaped canonical slot");
        require(close(visual.x + visual.width / 2.0, expectedCenterX)
                    && close(visual.y + visual.height / 2.0, expectedCenterY)
                    && close(visual.bottom(), expectedBaseline),
                "Normalized projected stack member lost center or baseline");
    }
    require(close(normalizedLandscape.width / normalizedLandscape.height,
                      16.0 / 9.0)
                && close(normalizedPortrait.width / normalizedPortrait.height,
                         9.0 / 16.0)
                && close(normalizedSquare.width / normalizedSquare.height, 1.0)
                && close(normalizedExtreme.width / normalizedExtreme.height, 3.2),
            "Normalized projected stack changed a member aspect ratio");

    const std::array<Kadunce::CardRect, 4> normalizedMembers{
        normalizedExtreme, normalizedPortrait,
        normalizedSquare, normalizedLandscape,
    };
    const auto envelope = Kadunce::makeOpenStackEnvelope(
        int(normalizedMembers.size()), 0, slot.width, slot.height);
    for (int memberIndex = 0;
         memberIndex < int(normalizedMembers.size()); ++memberIndex) {
        const auto pose = Kadunce::makeOpenStackPose(
            memberIndex, int(normalizedMembers.size()), 0, slot.width);
        auto posed = normalizedMembers.at(std::size_t(memberIndex));
        posed.x += pose.x;
        posed.y += pose.y;
        const auto [left, right] = rotatedHorizontalBounds(
            posed, pose.rotation);
        require(left >= slot.x + envelope.left - 0.0001
                    && right <= slot.right() + envelope.right + 0.0001,
                "Normalized projected member escaped the canonical fan envelope");
    }

    const auto heightLimited = Kadunce::makeProjectedStackPreviewPolicy(
        slot, std::vector<double>{0.4, 1.0});
    require(close(heightLimited.commonHeight, slot.height),
            "Tall projected stack did not use the available canonical height");
    const auto fallback = Kadunce::makeNormalizedProjectedCardVisualRect(
        slot, 720.0, 1200.0, {});
    require(close(fallback.x, portrait.x) && close(fallback.y, portrait.y)
                && close(fallback.width, portrait.width)
                && close(fallback.height, portrait.height),
            "Invalid group policy did not retain proportional aperture fallback");
}
