/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <algorithm>

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

}
