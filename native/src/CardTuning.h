/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <algorithm>
#include <optional>

namespace Kadunce
{
// How a person has tuned the cards, read from kwinrc [Effect-kadunce] by
// ActiveSettings.h. Each stands at Kadunce's own value until it is set.
struct CardTuning {
    static constexpr double DefaultCornerRadius = 10.0;
    static constexpr int DefaultStackPauseMs = 450;
    static constexpr double DefaultSpreadPeek = 0.5;

    // CardCornerRadius: the paper tier every window is cut to, 2 to 15 px.
    double cornerRadius = DefaultCornerRadius;
    // SleepingDimmed: whether Spread dims a sleeping card. Unset, it is dimmed
    // unless a context line says it sleeps (CARD-LIFECYCLE.md § Sleeping).
    std::optional<bool> sleepingDimmed;
    // StackPauseMs: how long a carried card rests on another before they
    // stack, 200 to 800 ms. Spread's own shorter pause keeps its proportion.
    int stackPauseMs = DefaultStackPauseMs;
    // StackBehind: "hairline" shows the cards behind a Stack's front card at
    // their edges in Spread; "none" shows its front card alone.
    bool stackBehindShown = true;
    // SpreadPeek: how far Spread's neighbours show past the centre card, from
    // 0, the narrowest, to 1, the widest; 0.5 is Kadunce's own.
    double spreadPeek = DefaultSpreadPeek;
    // CardShadow: how deep a shadow each loose window and card casts on what
    // is under it, from 0, none, which is Kadunce's own, to 1, the deepest.
    double shadow = 0.0;

    [[nodiscard]] double stackPauseScale() const { return double(stackPauseMs) / DefaultStackPauseMs; }
    bool operator==(const CardTuning &) const = default;
};

inline CardTuning &cardTuningStore()
{
    static CardTuning tuning;
    return tuning;
}
[[nodiscard]] inline const CardTuning &cardTuning() { return cardTuningStore(); }
inline void setCardTuning(const CardTuning &tuning) { cardTuningStore() = tuning; }

// The shadow a card casts at a depth from 0 to 1, in logical pixels: how far
// it softens, how far it falls below the card, and how dark it is where it
// meets the card's edge. At 0 there is none.
struct CardShadowShape {
    double blur = 0.0;
    double offset = 0.0;
    double opacity = 0.0;
    [[nodiscard]] bool cast() const { return opacity > 0.0; }
    // How far past the card the shadow reaches on each side, below the most.
    [[nodiscard]] double reachAbove() const { return std::max(0.0, blur - offset); }
    [[nodiscard]] double reachSides() const { return blur; }
    [[nodiscard]] double reachBelow() const { return blur + offset; }
};
[[nodiscard]] inline CardShadowShape cardShadowShape(double depth)
{
    const double d = std::clamp(depth, 0.0, 1.0);
    if (d <= 0.0) return {};
    return {6.0 + 30.0 * d, 2.0 + 6.0 * d, 0.10 + 0.40 * d};
}

// The space between Spread's cards as a share of the work area's width, for a
// peek from 0 to 1: the wider the peek, the narrower the space, so more of
// each neighbour shows beside the centre card without the card itself
// changing size. At 0.5 it is Kadunce's own 0.056.
[[nodiscard]] inline double spreadGutterShare(double peek)
{
    constexpr double Narrowest = 0.12;
    constexpr double Own = 0.056;
    constexpr double Widest = 0.02;
    const double p = std::clamp(peek, 0.0, 1.0);
    return p <= 0.5 ? Narrowest + (Own - Narrowest) * (p / 0.5)
                    : Own + (Widest - Own) * ((p - 0.5) / 0.5);
}
}
