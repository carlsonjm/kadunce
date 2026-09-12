#pragma once

namespace Kadunce {
enum class CardPaintRoute { Native, Hidden, Card };

// A native carry is one explicitly owned transaction, never a deck-wide escape.
constexpr CardPaintRoute cardPaintRoute(bool paintingTablet, bool tabletOwned,
                                       bool admitted, bool nativeCarry,
                                       bool cardCarry = false)
{
    if (cardCarry && tabletOwned && admitted) return CardPaintRoute::Card;
    if (nativeCarry || (!paintingTablet && !tabletOwned)) return CardPaintRoute::Native;
    if (paintingTablet && tabletOwned && admitted) return CardPaintRoute::Card;
    return CardPaintRoute::Hidden;
}
}
