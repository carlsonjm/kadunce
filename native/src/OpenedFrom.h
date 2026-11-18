/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <charconv>
#include <string_view>

namespace Kadunce::OpenedFrom
{
// Where a new window was opened from, read from what the desktop itself
// knows and never from how soon it opened after something else.
//
// A window in use that opens another asks the compositor for a launch
// ticket, an xdg-activation token, and the window that opens presents it.
// The compositor gives one only for the person's latest press, tap or key,
// so a new window presenting a ticket was asked for by whatever that press
// landed on: a card, or something that is no card, such as a panel.

// What the person's latest press, tap or key landed on.
enum class Pressed {
    Nothing, // nothing since Kadunce started
    Card, // an application window other than the one opening
    Itself, // the window opening
    Elsewhere, // a panel, the desktop, Search or any other surface
};

struct Evidence {
    // The window presented a launch ticket.
    bool ticket = false;
    Pressed pressed = Pressed::Nothing;
    // A program asked Kadunce for this window by name: a placement request,
    // Search's launch, a saved layout or a bring-back.
    bool askedFor = false;
};

enum class Finding {
    Card, // opened from the card last pressed
    AskedFor, // a program asked for it, which names its own place
    NoTicket, // it presented no ticket, so nothing says where it came from
    NotACard, // its ticket was for a press that landed on no card
};

[[nodiscard]] constexpr Finding find(const Evidence &evidence)
{
    if (evidence.askedFor) return Finding::AskedFor;
    if (!evidence.ticket) return Finding::NoTicket;
    return evidence.pressed == Pressed::Card ? Finding::Card : Finding::NotACard;
}

[[nodiscard]] constexpr const char *words(Finding finding)
{
    switch (finding) {
    case Finding::Card: return "opened from the card last pressed";
    case Finding::AskedFor: return "asked for by name, so it names its own place";
    case Finding::NoTicket: return "no ticket, so nothing says where it opened from";
    case Finding::NotACard: return "its ticket is for a press that landed on no card";
    }
    return "";
}

[[nodiscard]] constexpr const char *words(Pressed pressed)
{
    switch (pressed) {
    case Pressed::Nothing: return "nothing";
    case Pressed::Card: return "a card";
    case Pressed::Itself: return "the window itself";
    case Pressed::Elsewhere: return "no card";
    }
    return "";
}

// The count in a ticket KWin gave, "kwin-12", which only rises; negative for
// any other text, as an empty ticket or one KWin refused.
[[nodiscard]] inline int ticketNumber(std::string_view ticket)
{
    constexpr std::string_view prefix = "kwin-";
    if (ticket.size() <= prefix.size() || ticket.substr(0, prefix.size()) != prefix) return -1;
    const std::string_view digits = ticket.substr(prefix.size());
    int number = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), number);
    return error == std::errc() && end == digits.data() + digits.size() && number >= 0 ? number : -1;
}
} // namespace Kadunce::OpenedFrom
