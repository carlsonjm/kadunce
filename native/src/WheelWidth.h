/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <algorithm>
#include <cmath>
#include <span>
#include <utility>

namespace Kadunce
{
// CARD-LIFECYCLE.md §11, Connected: a wheel turned while a card is held at
// the top or a side edge sets its width like a volume knob. Each notch moves
// it a twenty-fourth of the display, 80 px on a 1920 monitor.
inline constexpr double WheelWidthStep = 1.0 / 24.0;
// Notches closer together than this are a crank, and pass every stop.
inline constexpr long long WheelWidthCrankMs = 120;

// The top edge narrows the Active card, kept centred, from the whole width.
inline constexpr double WheelWidthTopStops[] = {3.0 / 4.0, 2.0 / 3.0, 1.0 / 2.0};
// A side edge sets its column's share, the other side taking the rest.
inline constexpr double WheelWidthSideStops[] = {2.0 / 3.0, 1.0 / 2.0, 1.0 / 3.0, 1.0 / 4.0};
inline constexpr double WheelWidthSideMost = 0.8;
// Turned this far, a window held away from the edges takes the whole width,
// as the top edge gives it; short of it, a pane keeps its place at that width.
inline constexpr double WheelWidthWhole = 0.98;
// Narrower than this is no longer a place to work, whatever the app allows.
inline constexpr double WheelWidthLeast = 1.0 / 6.0;

// The width after `notches`, positive away from the person, which widens.
// Turned slowly, a step that would pass a stop, or end within half a step of
// one, lands on it, and the next slow notch rests there before it moves on;
// a crank passes through. The width stays in [low, high].
struct WheelWidthKnob {
    double width = 1.0;
    bool caught = false;

    void turn(int notches, bool crank, double low, double high, std::span<const double> stops)
    {
        if (low > high || notches == 0) return;
        if (!crank && std::exchange(caught, false)) {
            notches -= notches > 0 ? 1 : -1;
            if (notches == 0) return;
        }
        double next = std::clamp(width + notches * WheelWidthStep, low, high);
        caught = false;
        if (!crank) {
            const double from = std::min(width, next), to = std::max(width, next);
            double nearest = -1.0;
            for (const double stop : stops) {
                if (stop < low || stop > high || std::abs(stop - width) < 1e-6) continue;
                const bool passed = stop > from && stop < to;
                const bool near = std::abs(stop - next) <= WheelWidthStep / 2.0;
                // The first stop the turn reaches catches it.
                if ((passed || near)
                    && (nearest < 0.0 || std::abs(stop - width) < std::abs(nearest - width)))
                    nearest = stop;
            }
            if (nearest >= 0.0) { next = nearest; caught = true; }
        }
        width = next;
    }
};
} // namespace Kadunce
