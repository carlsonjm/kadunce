/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <algorithm>
#include <optional>

namespace Kadunce {
// How tall the Active card stands while the keyboard is up: its bottom edge a
// gutter above the keys, so whatever the application keeps at its own bottom
// edge sits on top of them. A card the keys do not reach keeps its height,
// and the room never grows a card past the height it rests at.
[[nodiscard]] inline double keyboardRoomHeight(
    double cardTop, double cardHeight, double keyboardTop, double gutter)
{
    return std::clamp(keyboardTop - gutter - cardTop, 0.0, cardHeight);
}

// Where the Active card's top edge goes while the keyboard is up. A client
// that cannot be as short as the room keeps its least height, and the card
// rises by what that leaves over, so the field at its bottom edge still ends
// a gutter above the keys and only its top goes out of sight. Where the
// client says where its text cursor is, `cursorBottom` (measured with the
// card at `cardTop`), the card rises only as far as that cursor needs to
// clear the keys: a search field at the top of a tall window stays in sight,
// and the window's bottom goes under the keys instead.
[[nodiscard]] inline double keyboardRoomTop(
    double cardTop, double height, double keyboardTop, double gutter,
    std::optional<double> cursorBottom = std::nullopt)
{
    const double over = std::max(0.0, cardTop + height - (keyboardTop - gutter));
    if (!cursorBottom) return cardTop - over;
    return cardTop - std::min(over, std::max(0.0, *cursorBottom - (keyboardTop - gutter)));
}

// The height to ask the Active card's client for as the keys move, if any.
// The card is drawn ending a gutter above the keys on every frame whatever
// size its client has, so the client is asked only where the drawn card would
// otherwise show more than the client has: the room the keys give back is
// asked for at once, all of it that the motion will give, so the client is
// already that tall when the keys uncover it; the room they take is asked for
// once they rest, since a client made short early stands clear of keys still
// rising. `asked` is the height last asked for, `now` the room where the keys
// stand, `heading` the room where they are going when that is known, and
// `whole` the card's own height.
[[nodiscard]] inline std::optional<double> keyboardRoomAsk(
    double asked, double now, std::optional<double> heading, double whole, bool resting)
{
    if (resting) return now == asked ? std::nullopt : std::optional<double>(now);
    // Keys on their way to less room than they leave now are still arriving,
    // whatever was asked for them ahead of time.
    if (now <= asked || (heading && *heading < now)) return std::nullopt;
    return std::max(now, heading.value_or(whole));
}

}
