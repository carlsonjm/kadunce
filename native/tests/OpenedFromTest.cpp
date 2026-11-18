/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of where a new window opened from: a ticket and a press
// on a card name that card, and anything less names none.
#include "OpenedFrom.h"

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
             Pressed::Shortcut})
        require(!std::string_view(words(pressed)).empty(), "every press has its words");

    require(ticketNumber("kwin-1") == 1, "the first ticket is read");
    require(ticketNumber("kwin-412") == 412, "a later ticket is read");
    require(ticketNumber("") < 0, "no ticket has no number");
    require(ticketNumber("kwin-") < 0, "a ticket with no count has no number");
    require(ticketNumber("not-granted-666") < 0, "a refused ticket has no number");
    require(ticketNumber("kwin-12x") < 0, "a count with more after it has no number");
    require(ticketNumber("kwin--3") < 0, "a count below nothing has no number");
    return 0;
}
