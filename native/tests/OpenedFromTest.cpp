/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of where a new window opened from: a ticket and a press
// on a card name that card, and anything less names none; of how a window
// joins the Stack of the card it names; and of where the pair of the two
// applications says it lands.
#include "OpenedFrom.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

using namespace Kadunce::OpenedFrom;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
} // namespace

int main()
{
    // A link pressed in one card opens a window that presents its ticket.
    require(find({true, Pressed::Card, false}) == Finding::Card,
        "a ticket and a press on a card name that card");
    // The dock, a panel or Search took the press: no card asked for it.
    require(find({true, Pressed::Elsewhere, false}) == Finding::NotACard,
        "a ticket for a press on a panel names no card");
    require(find({true, Pressed::Nothing, false}) == Finding::NotACard,
        "a ticket with no press seen names no card");
    require(find({true, Pressed::Itself, false}) == Finding::NotACard,
        "a window is never opened from itself");
    // A shortcut of the desktop's opens a window while a card holds the keys:
    // the key never reached that card.
    require(find({true, Pressed::Shortcut, false}) == Finding::NotACard,
        "a ticket for a key a shortcut took names no card");
    // A card pressed after the ticket was given is not the one it was given for.
    require(find({true, Pressed::Later, false}) == Finding::NotACard,
        "a ticket given before the press on a card names no card");
    // An application that passes no ticket on says nothing, whatever was pressed.
    require(find({false, Pressed::Card, false}) == Finding::NoTicket,
        "a press on a card without a ticket names no card");
    require(find({false, Pressed::Nothing, false}) == Finding::NoTicket,
        "nothing known names no card");
    // A window a program asked for by name goes where that program said.
    require(find({true, Pressed::Card, true}) == Finding::AskedFor,
        "a window asked for by name is not opened from a card");
    require(find({false, Pressed::Elsewhere, true}) == Finding::AskedFor,
        "asked for by name comes before a missing ticket");

    for (const Finding finding : {Finding::Card, Finding::AskedFor, Finding::NoTicket, Finding::NotACard})
        require(!std::string_view(words(finding)).empty(), "every finding has its words");
    for (const Pressed pressed : {Pressed::Nothing, Pressed::Card, Pressed::Itself, Pressed::Elsewhere,
             Pressed::Shortcut, Pressed::Later})
        require(!std::string_view(words(pressed)).empty(), "every press has its words");

    require(ticketNumber("kwin-1") == 1, "the first ticket is read");
    require(ticketNumber("kwin-412") == 412, "a later ticket is read");
    require(ticketNumber("") < 0, "no ticket has no number");
    require(ticketNumber("kwin-") < 0, "a ticket with no count has no number");
    require(ticketNumber("not-granted-666") < 0, "a refused ticket has no number");
    require(ticketNumber("kwin-12x") < 0, "a count with more after it has no number");
    require(ticketNumber("kwin--3") < 0, "a count below nothing has no number");

    // The dock gave ticket 9 at 100; a card was pressed at 500. A window then
    // opening with ticket 7, given before 9, was not asked for by that press;
    // one with ticket 12 may have been.
    const std::array<TicketSeen, 3> seen{TicketSeen{4, 20}, TicketSeen{9, 100}, TicketSeen{12, 700}};
    require(givenBeforePress(7, 500, seen), "a ticket older than one seen before the press came first");
    require(!givenBeforePress(12, 500, seen), "a ticket newer than every one seen before the press may be its own");
    require(!givenBeforePress(9, 500, seen), "a ticket is not older than itself");
    require(!givenBeforePress(10, 500, seen), "a later ticket seen only after the press proves nothing");
    require(!givenBeforePress(3, 10, seen), "a press before every ticket seen is before them all");

    // Opened from a pane, the window takes that pane as it opens; one whose
    // ticket followed it already has a pane and moves onto the card's.
    require(joinFor(Place::Window, Place::Pane) == Join::TakesPane, "a window opening from a pane takes the pane");
    require(joinFor(Place::Pane, Place::Pane) == Join::PaneOnPane, "a pane opened from a pane moves onto it");
    require(joinFor(Place::Card, Place::Pane) == Join::None, "a card is not put on a pane it did not open into");
    // Opened from a card, it joins once it is a card or a pane itself.
    require(joinFor(Place::Window, Place::Card) == Join::Waits, "a window opening from a card waits to be one");
    require(joinFor(Place::Card, Place::Card) == Join::CardOnCard, "a card opened from a card joins its Stack");
    require(joinFor(Place::Pane, Place::Card) == Join::CardBehindPane, "a card that left for it waits behind its pane");
    // A window that is no card holds no Stack to join.
    for (const Place opened : {Place::Window, Place::Pane, Place::Card})
        require(joinFor(opened, Place::Window) == Join::None, "nothing joins a window that is no card");

    // Pairs as they are kept.
    const auto link = pairFrom("mail>zen=front");
    require(link && link->from == "mail" && link->to == "zen" && link->lands == Lands::InFront, "a pair reads back whole");
    require(pairFrom("files>*=beside")->to == AnyApp, "a pair may name any application it opens");
    require(!pairFrom("mail>zen"), "a pair with no place is none");
    require(!pairFrom("mail>zen=sideways"), "a pair with a place Cards does not know is none");
    require(!pairFrom(">zen=front") && !pairFrom("mail>=front"), "a pair names an application on both sides");
    require(!pairFrom("mail>mail=front"), "an application is no pair with itself");
    require(!pairFrom("*>zen=front"), "a pair names the application of the card");
    require(followsFrom("pairs") == Follows::Pairs && followsFrom("every") == Follows::Every, "the choice reads back");
    require(followsFrom("") == Follows::None && followsFrom("anything") == Follows::None, "any other word follows none");

    const std::array<Pair, 4> pairs{Pair{"mail", "zen", Lands::InFront}, Pair{"files", "*", Lands::Beside},
        Pair{"files", "loupe", Lands::InFront}, Pair{"zen", "spotify", Lands::Alone}};
    // Followed for none, nothing is placed by where it opened from.
    require(landsFor(Follows::None, pairs, "mail", "zen") == Lands::AsBefore, "switched off, a pair is not followed");
    // Only the pairs: a pair lands where it says, any other as before.
    require(landsFor(Follows::Pairs, pairs, "mail", "zen") == Lands::InFront, "a pair stands in front where it says so");
    require(landsFor(Follows::Pairs, pairs, "zen", "mail") == Lands::AsBefore, "a pair is not followed the other way round");
    require(landsFor(Follows::Pairs, pairs, "mail", "notes") == Lands::AsBefore, "applications in no pair open as before");
    require(landsFor(Follows::Pairs, pairs, "files", "loupe") == Lands::InFront, "the pair naming both wins over any");
    require(landsFor(Follows::Pairs, pairs, "files", "okular") == Lands::Beside, "the pair naming any covers the rest");
    // Every application: in front unless a pair says otherwise.
    require(landsFor(Follows::Every, pairs, "mail", "notes") == Lands::InFront, "every application stands in front");
    require(landsFor(Follows::Every, pairs, "zen", "spotify") == Lands::Alone, "a pair set apart stays apart");
    require(landsFor(Follows::Every, pairs, "files", "okular") == Lands::Beside, "a pair still says where");
    return 0;
}
