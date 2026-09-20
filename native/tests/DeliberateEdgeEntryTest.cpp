/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of the deliberate edge-entry decision. Each case states
// a gesture from CARD-LIFECYCLE.md §3 and §10 and asserts the ownership the
// display should hold afterwards. Nothing here inspects a caller's shape.
#include "DeliberateEdgeEntry.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

constexpr EdgeEntryRequest carried(CarryEdge edge)
{
    EdgeEntryRequest request;
    request.edge = edge;
    request.carriedEligible = true;
    return request;
}

constexpr CarryEdge Sides[] = {CarryEdge::Left, CarryEdge::Right};
constexpr CarryEdge Admitting[] = {CarryEdge::Top, CarryEdge::Left, CarryEdge::Right};
} // namespace

int main()
{
    // §3: the first deliberate action adopts the display, and adoption produces
    // individual cards. A side snap does not start a layout.
    for (const auto edge : Admitting) {
        require(planEdgeEntry(carried(edge)) == EdgeEntryOutcome::AdoptDisplay,
            "First edge action did not adopt the display as individual cards");
    }

    // The same first action cannot become a pairing merely because something
    // claims a card is Active while the display owns nothing.
    {
        auto request = carried(CarryEdge::Left);
        request.activeCardPresent = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::AdoptDisplay,
            "First edge action paired before the display owned anything");
    }

    // §3: Bento begins on a side snap while an individual card is Active, and
    // pairs exactly those two.
    for (const auto edge : Sides) {
        auto request = carried(edge);
        request.ownsDisplay = true;
        request.activeCardPresent = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::PairIntoBento,
            "Side snap beside an Active card did not pair into Bento");
    }

    // §10: nothing to pair with leaves one individual Active card.
    for (const auto edge : Sides) {
        auto request = carried(edge);
        request.ownsDisplay = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::MakeActive,
            "Side snap with no Active card started a layout");
        request.activeCardPresent = true;
        request.activeCardIsCarried = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::MakeActive,
            "The Active card paired with itself");
    }

    // §10: the top edge always yields one Active card, never a layout.
    {
        auto request = carried(CarryEdge::Top);
        request.ownsDisplay = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::MakeActive,
            "Top edge did not yield a single Active card");
        request.activeCardPresent = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::MakeActive,
            "Top edge paired into Bento");
    }

    // §5 owns what a live layout does with a returning card. Entry declines it
    // rather than solving a second layout or a second entry for the display.
    for (const auto edge : Admitting) {
        auto request = carried(edge);
        request.ownsDisplay = true;
        request.hasBentoLayout = true;
        request.activeCardPresent = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::Refuse,
            "Entry answered a display that already has a Bento layout");
    }

    // §10: the bottom edge releases to Plasma, and an ineligible carry admits
    // nothing, on every edge and in every ownership state.
    for (const bool owns : {false, true}) {
        for (const bool active : {false, true}) {
            auto bottom = carried(CarryEdge::Bottom);
            bottom.ownsDisplay = owns;
            bottom.activeCardPresent = active;
            require(planEdgeEntry(bottom) == EdgeEntryOutcome::Refuse,
                "The bottom edge admitted a window");
            for (const auto edge : Admitting) {
                auto ineligible = carried(edge);
                ineligible.carriedEligible = false;
                ineligible.ownsDisplay = owns;
                ineligible.activeCardPresent = active;
                require(planEdgeEntry(ineligible) == EdgeEntryOutcome::Refuse,
                    "An ineligible carry was admitted");
            }
        }
    }

    std::cout << "Deliberate edge-entry checks passed\n";
    return 0;
}
