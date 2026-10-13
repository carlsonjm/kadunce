/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "BentoLayout.h"

#include <QString>

#include <optional>

namespace Kadunce
{
// How the display that holds Spread plays a card (CARD-LIFECYCLE.md §11).
// Active shows one card at a time over the whole work area. Connected plays a
// picked card into the display's layout beside what it already shows.
enum class DisplaySizing { Active, Connected };

// Kadunce chooses by room, never by inches: the display's size in logical
// pixels, at the scale the person set, so a panel or dock that takes some of
// it does not change the answer. A display with room for more than the
// compact pane cap is Connected; it is the bound every layout path reads
// (BentoLayout.h).
[[nodiscard]] constexpr DisplaySizing automaticSizing(int width, int height)
{
    return bentoPaneCap(width, height) > BentoCompactPaneCap
        ? DisplaySizing::Connected : DisplaySizing::Active;
}

// A choice as kaduncerc's [Sizing] group keeps it, by display name. Anything
// else, the empty choice included, lets Kadunce choose.
[[nodiscard]] inline std::optional<DisplaySizing> parseSizing(const QString &text)
{
    if (text.compare(QStringLiteral("active"), Qt::CaseInsensitive) == 0)
        return DisplaySizing::Active;
    if (text.compare(QStringLiteral("connected"), Qt::CaseInsensitive) == 0)
        return DisplaySizing::Connected;
    return std::nullopt;
}

[[nodiscard]] inline QString sizingName(DisplaySizing sizing)
{
    return sizing == DisplaySizing::Connected ? QStringLiteral("connected")
                                              : QStringLiteral("active");
}

// The person's choice for the display where there is one, else Kadunce's.
[[nodiscard]] inline DisplaySizing displaySizing(const QString &chosen,
                                                 int width, int height)
{
    return parseSizing(chosen).value_or(automaticSizing(width, height));
}
} // namespace Kadunce
