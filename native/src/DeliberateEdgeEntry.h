/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "CarrySession.h"

namespace Kadunce
{
// What a deliberate edge action means, decided before any layout is solved.
//
// CARD-LIFECYCLE.md §3 and §10 give each edge one meaning, but the meaning was
// previously implied by whichever admission path a carry seam happened to
// reserve. This value states it once, so a side snap cannot mean one thing
// carried from the native desktop and another carried from Spread.
//
// Who the partner is does not belong here. §3 names it from Spread order, which
// only the card stage can read; this value is told whether one was named.
enum class EdgeEntryOutcome {
    // The gesture carries no entry meaning on this display.
    Refuse,
    // §3: adopt every eligible window on the display; the carried one is Active.
    AdoptDisplay,
    // §10: one individual Active card. No layout begins.
    MakeActive,
    // §3: the carried window and the named partner become the two panes.
    PairIntoBento,
    // §10: the Active card was carried and no partner was named, so nothing
    // pairs and nothing changes. Distinct from MakeActive, which moves a card
    // that was not already Active.
    Unchanged,
    // A display that cannot own cards reaches neither pairing rule, so an edge
    // action there composes Bento across the display. PRODUCT-CONTRACT.md gives
    // an external output ordinary windows or per-output Bento, never cards.
    ComposeDisplayBento,
};

struct EdgeEntryRequest {
    CarryEdge edge = CarryEdge::Top;
    // §3: Kadunce owns the display from its first Card or Bento action, whether
    // what it holds is individual cards, stacks or a Bento group.
    bool ownsDisplay = false;
    // Whether this display can hold cards at all. State and capability decide
    // the grammar; the display's hardware identity never does.
    bool canOwnCards = false;
    // The display already has a live Bento layout. A snap into one is §5's
    // displacement, which is not an entry and is not decided here.
    bool hasBentoLayout = false;
    bool carriedEligible = false;
    // A partner passed §4's partner predicate. The carried window is never its
    // own partner, so this is false when the walk found only the carried card.
    bool partnerNamed = false;
    bool carriedIsActive = false;
};

[[nodiscard]] constexpr EdgeEntryOutcome planEdgeEntry(const EdgeEntryRequest &request)
{
    if (!request.carriedEligible) return EdgeEntryOutcome::Refuse;
    // §10 gives the bottom edge to Plasma. It never admits anything.
    if (request.edge == CarryEdge::Bottom) return EdgeEntryOutcome::Refuse;
    // §5 owns what a live layout does with a snap, with one exception: §5's
    // top-edge departure and §10's top edge agree on the answer, so a pane
    // carried there leaves for one independent Active card exactly as a card
    // would. Where the display cannot own cards there is no card to become,
    // and every other snap into a live layout is §5's displacement.
    if (request.hasBentoLayout
        && !(request.edge == CarryEdge::Top && request.canOwnCards))
        return EdgeEntryOutcome::Refuse;
    if (!request.canOwnCards) return EdgeEntryOutcome::ComposeDisplayBento;
    // §3: the first deliberate action on a display adopts it, whichever of the
    // three admitting edges was used. Adoption produces individual cards only.
    if (!request.ownsDisplay) return EdgeEntryOutcome::AdoptDisplay;
    // §10: the top edge never pairs.
    if (request.edge == CarryEdge::Top) return EdgeEntryOutcome::MakeActive;
    if (request.partnerNamed) return EdgeEntryOutcome::PairIntoBento;
    return request.carriedIsActive ? EdgeEntryOutcome::Unchanged
                                   : EdgeEntryOutcome::MakeActive;
}
} // namespace Kadunce
