/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <algorithm>
#include <optional>

namespace Kadunce
{
// How a person has tuned the cards, read from kwinrc [Effect-kadunce] by
// ActiveSettings.h. Each stands at Kadunce's own value until it is set.
struct CardTuning {
    static constexpr double DefaultCornerRadius = 8.0;
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
