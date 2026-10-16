/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "BentoLayout.h"

#include <QHash>
#include <QString>

#include <optional>

namespace Kadunce
{
// How a display plays cards (CARD-LIFECYCLE.md §1, §11). Solo shows one card
// at a time over the whole work area, a Bento pair at most. Connected plays a
// picked card into the display's layout beside what it already shows.
enum class DisplaySizing { Solo, Connected };

// Kadunce chooses by room, never by inches: the display's size in logical
// pixels, at the scale the person set, so a panel or dock that takes some of
// it does not change the answer. A display with room for more than the
// compact pane cap is Connected; it is the bound every layout path reads
// (BentoLayout.h).
[[nodiscard]] constexpr DisplaySizing automaticSizing(int width, int height)
{
    return bentoPaneCap(width, height) > BentoCompactPaneCap
        ? DisplaySizing::Connected : DisplaySizing::Solo;
}

// A choice as kaduncerc's [Sizing] group keeps it. "active"
// is Solo's earlier name, still read because saved choices carry it. Anything
// else, the empty choice included, lets Kadunce choose.
[[nodiscard]] inline std::optional<DisplaySizing> parseSizing(const QString &text)
{
    const QString name = text.trimmed();
    if (name.compare(QStringLiteral("solo"), Qt::CaseInsensitive) == 0
        || name.compare(QStringLiteral("active"), Qt::CaseInsensitive) == 0)
        return DisplaySizing::Solo;
    if (name.compare(QStringLiteral("connected"), Qt::CaseInsensitive) == 0)
        return DisplaySizing::Connected;
    return std::nullopt;
}

[[nodiscard]] inline QString sizingName(DisplaySizing sizing)
{
    return sizing == DisplaySizing::Connected ? QStringLiteral("connected")
                                              : QStringLiteral("solo");
}

// A display's chosen sizing, from kaduncerc's [Sizing] group, by display name,
// and [SizingDisplays], each of those names' lasting identity when chosen. A
// choice follows its display's identity to the name it has now, and a name a
// dock has since given another display carries no choice to it. A choice kept
// before identities were is read by name. Empty when none was chosen.
[[nodiscard]] inline QString chosenSizing(const QHash<QString, QString> &choices,
    const QHash<QString, QString> &identities, const QString &name, const QString &identity)
{
    if (!identity.isEmpty()) {
        for (auto it = identities.cbegin(); it != identities.cend(); ++it)
            if (it.value() == identity && choices.contains(it.key())) return choices.value(it.key());
    }
    const QString recorded = identities.value(name);
    if (!recorded.isEmpty() && !identity.isEmpty() && recorded != identity) return {};
    return choices.value(name);
}

// The person's choice for the display where there is one, else Kadunce's.
[[nodiscard]] inline DisplaySizing displaySizing(const QString &chosen,
                                                 int width, int height)
{
    return parseSizing(chosen).value_or(automaticSizing(width, height));
}
} // namespace Kadunce
