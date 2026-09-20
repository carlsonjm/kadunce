/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "CarrySession.h"

namespace Kadunce
{
// What a deliberate edge action means, decided before any layout is solved.
//
// CARD-LIFECYCLE.md §3 and §10 give an edge action one meaning per edge, but
// the meaning was previously implied by whichever admission path a carry seam
// happened to reserve. Every first snap therefore started a layout, and the
// solver chose pane membership nobody had asked for. This value states the
// meaning once, so both carry seams answer the same gesture the same way.
enum class EdgeEntryOutcome {
    // The gesture carries no entry meaning on this display.
    Refuse,
    // §3: adopt every eligible window on the display; the carried one is Active.
    AdoptDisplay,
    // §10: one individual Active card. No layout begins.
    MakeActive,
    // §3: the carried window and the Active card become the two panes, and no
    // other window joins.
    PairIntoBento,
};

struct EdgeEntryRequest {
    CarryEdge edge = CarryEdge::Top;
    // The display already owns individual cards.
    bool ownsDisplay = false;
    // The display already has a live Bento layout. Returning a card to a live
    // layout is §5's displacement, which is not an entry and is not decided here.
    bool hasBentoLayout = false;
    bool carriedEligible = false;
    // Presentation context, never ownership: which individual card is Active.
    // A carried window that is itself that card has nothing distinct to pair
    // with, so it stays Active rather than pairing with itself.
    bool activeCardPresent = false;
    bool activeCardIsCarried = false;
};

[[nodiscard]] constexpr EdgeEntryOutcome planEdgeEntry(const EdgeEntryRequest &request)
{
    if (!request.carriedEligible || request.hasBentoLayout)
        return EdgeEntryOutcome::Refuse;
    // §10 gives the bottom edge to Plasma. It never admits anything.
    if (request.edge == CarryEdge::Bottom) return EdgeEntryOutcome::Refuse;
    // §3: the first deliberate action on a display adopts it, whichever of the
    // three admitting edges was used. Adoption produces individual cards only.
    if (!request.ownsDisplay) return EdgeEntryOutcome::AdoptDisplay;
    if (request.edge == CarryEdge::Top) return EdgeEntryOutcome::MakeActive;
    return request.activeCardPresent && !request.activeCardIsCarried
        ? EdgeEntryOutcome::PairIntoBento
        : EdgeEntryOutcome::MakeActive;
}
} // namespace Kadunce
