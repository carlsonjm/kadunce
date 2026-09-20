/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of the deliberate edge-entry decision. Each case states a
// gesture from CARD-LIFECYCLE.md §3 and §10 and asserts the ownership the
// display should hold afterwards. Who the partner is belongs to the card stage,
// which reads Spread order; this decision is only told whether one was named.
#include "DeliberateEdgeEntry.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

// A display that can hold cards, with an eligible window being carried.
constexpr EdgeEntryRequest carried(CarryEdge edge)
{
    EdgeEntryRequest request;
    request.edge = edge;
    request.carriedEligible = true;
    request.canOwnCards = true;
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

    // The first action cannot become a pairing because something claims a
    // partner while the display owns nothing.
    {
        auto request = carried(CarryEdge::Left);
        request.partnerNamed = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::AdoptDisplay,
            "First edge action paired before the display owned anything");
    }

    // §3: once the display is owned, a side snap pairs the carried window with
    // the named partner. This holds whether the carried window is the Active
    // card, which §3 pairs with a Spread neighbour, or another window, which it
    // pairs with the Active card.
    for (const auto edge : Sides) {
        for (const bool carriedIsActive : {false, true}) {
            auto request = carried(edge);
            request.ownsDisplay = true;
            request.partnerNamed = true;
            request.carriedIsActive = carriedIsActive;
            require(planEdgeEntry(request) == EdgeEntryOutcome::PairIntoBento,
                "A side snap with a named partner did not pair into Bento");
        }
    }

    // §10: no partner named. A carried Active card leaves everything unchanged;
    // anything else becomes the Active card.
    for (const auto edge : Sides) {
        auto request = carried(edge);
        request.ownsDisplay = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::MakeActive,
            "A side snap with no partner did not yield one Active card");
        request.carriedIsActive = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::Unchanged,
            "The Active card with no partner did not leave the display unchanged");
    }

    // §10: the top edge always yields one Active card, never a pair, even when
    // a partner would have been available to a side snap.
    {
        auto request = carried(CarryEdge::Top);
        request.ownsDisplay = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::MakeActive,
            "Top edge did not yield a single Active card");
        request.partnerNamed = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::MakeActive,
            "Top edge paired into Bento");
        request.carriedIsActive = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::MakeActive,
            "Top edge answered the Active card differently");
    }

    // §5 owns what a live layout does with a side snap. Entry declines it
    // rather than solving a second layout or a second entry for the display.
    for (const auto edge : Sides) {
        auto request = carried(edge);
        request.ownsDisplay = true;
        request.hasBentoLayout = true;
        request.partnerNamed = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::Refuse,
            "Entry answered a side snap on a display that already has a Bento layout");
    }

    // §5's top-edge departure and §10's top edge give the same answer, so a
    // pane carried there becomes one independent Active card. Nothing about
    // the layout changes that, including a partner a side snap could have used.
    {
        auto request = carried(CarryEdge::Top);
        request.ownsDisplay = true;
        request.hasBentoLayout = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::MakeActive,
            "A pane carried to the top edge did not leave for one Active card");
        request.partnerNamed = true;
        require(planEdgeEntry(request) == EdgeEntryOutcome::MakeActive,
            "A pane carried to the top edge paired instead of leaving");
    }

    // A display that cannot own cards reaches neither pairing rule, so an edge
    // action composes Bento across it. Its ownership state cannot change that,
    // and a partner can never be named there.
    for (const auto edge : Admitting) {
        for (const bool owns : {false, true}) {
            auto request = carried(edge);
            request.canOwnCards = false;
            request.ownsDisplay = owns;
            require(planEdgeEntry(request) == EdgeEntryOutcome::ComposeDisplayBento,
                "A display that cannot own cards did not compose Bento");
            request.hasBentoLayout = true;
            // Including the top edge: there is no card for a pane to become,
            // so the departure §5 describes has no destination here.
            require(planEdgeEntry(request) == EdgeEntryOutcome::Refuse,
                "A live layout did not take priority over composing a new one");
        }
    }

    // §10: the bottom edge releases to Plasma, and an ineligible carry admits
    // nothing, on every edge and in every state.
    for (const bool owns : {false, true}) {
        for (const bool named : {false, true}) {
            for (const bool capable : {false, true}) {
                auto bottom = carried(CarryEdge::Bottom);
                bottom.ownsDisplay = owns;
                bottom.partnerNamed = named;
                bottom.canOwnCards = capable;
                require(planEdgeEntry(bottom) == EdgeEntryOutcome::Refuse,
                    "The bottom edge admitted a window");
                for (const auto edge : Admitting) {
                    auto ineligible = carried(edge);
                    ineligible.carriedEligible = false;
                    ineligible.ownsDisplay = owns;
                    ineligible.partnerNamed = named;
                    ineligible.canOwnCards = capable;
                    require(planEdgeEntry(ineligible) == EdgeEntryOutcome::Refuse,
                        "An ineligible carry was admitted");
                }
            }
        }
    }

    std::cout << "Deliberate edge-entry checks passed\n";
    return 0;
}
