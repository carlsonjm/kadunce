/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <charconv>
#include <optional>
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
//
// The press is read when the ticket is given, where the compositor announces
// that, and otherwise as the window opens, unless a ticket given later was
// seen before that press: then the ticket is for an earlier one.
//
// Switched on, a window opened from a card lands where the pair of the two
// applications says (landsFor): in front of that card's Stack (joinFor),
// beside the card, or on its own. A window of the card's own application is
// never placed by it.

// What the person's latest press, tap or key landed on.
enum class Pressed {
    Nothing, // nothing since Kadunce started
    Card, // an application window other than the one opening
    Itself, // the window opening
    Elsewhere, // a panel, the desktop, Search or any other surface
    Shortcut, // a key no window received: a shortcut of the desktop's took it
    Later, // a card, pressed after the ticket was given: the ticket is for an earlier press
};

struct Evidence {
    // The window presented a launch ticket the compositor gave. One it
    // refused to give is no ticket (ticketNumber).
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
    case Pressed::Shortcut: return "a shortcut";
    case Pressed::Later: return "a card, after its ticket was given";
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

// A ticket seen, and when on a clock that never steps back.
struct TicketSeen {
    int number = -1;
    long long at = 0;
};

// Whether a ticket was given before a press: one with a higher count, so
// given after it, was seen before that press was made.
template<typename Tickets>
[[nodiscard]] constexpr bool givenBeforePress(int ticket, long long pressedAt, const Tickets &seen)
{
    for (const TicketSeen &other : seen)
        if (other.number > ticket && other.at < pressedAt) return true;
    return false;
}

// Where a window stands in Cards: a pane of its display's layout, a card of
// its Spread, or neither yet.
enum class Place { Window, Pane, Card };

// How a window joins the Stack of the card it was opened from.
enum class Join {
    None, // it stays where it opened
    Waits, // it is no card yet; it joins once it is one
    TakesPane, // it takes the card's pane as it opens, the card behind it
    PaneOnPane, // already a pane beside it, it moves onto the card's pane
    CardBehindPane, // the card left the layout for it, and waits behind its pane
    CardOnCard, // both are cards: it stands in front of the card's Stack
};

[[nodiscard]] constexpr Join joinFor(Place opened, Place opener)
{
    if (opener == Place::Pane)
        return opened == Place::Window ? Join::TakesPane : opened == Place::Pane ? Join::PaneOnPane : Join::None;
    if (opener == Place::Card)
        return opened == Place::Window ? Join::Waits : opened == Place::Pane ? Join::CardBehindPane : Join::CardOnCard;
    return Join::None;
}

// Which windows opened from a card are placed by where they opened from:
// none, those of the pairs kept, or every one.
enum class Follows { None, Pairs, Every };

// Where such a window lands: as any window does, in front of the card's
// Stack, beside the card, or on its own with nothing standing beside it.
enum class Lands { AsBefore, InFront, Beside, Alone };

// One pair: the application of the card, the application of the window it
// opens, AnyApp for whichever, and where that window lands.
inline constexpr std::string_view AnyApp = "*";
struct Pair {
    std::string_view from;
    std::string_view to;
    Lands lands = Lands::InFront;
};

[[nodiscard]] constexpr const char *words(Follows follows)
{
    return follows == Follows::Every ? "every" : follows == Follows::Pairs ? "pairs" : "off";
}
[[nodiscard]] constexpr Follows followsFrom(std::string_view text)
{
    return text == "every" ? Follows::Every : text == "pairs" ? Follows::Pairs : Follows::None;
}
[[nodiscard]] constexpr const char *words(Lands lands)
{
    switch (lands) {
    case Lands::AsBefore: return "";
    case Lands::InFront: return "front";
    case Lands::Beside: return "beside";
    case Lands::Alone: return "alone";
    }
    return "";
}

// A pair as it is kept, "org.kde.dolphin>org.gnome.Loupe=front": nothing for
// text that names no application on either side, the same one on both, or no
// place. The views are into `text`.
[[nodiscard]] constexpr std::optional<Pair> pairFrom(std::string_view text)
{
    const auto arrow = text.find('>');
    const auto equals = text.rfind('=');
    if (arrow == std::string_view::npos || equals == std::string_view::npos || equals < arrow) return std::nullopt;
    const Pair pair{text.substr(0, arrow), text.substr(arrow + 1, equals - arrow - 1),
        text.substr(equals + 1) == "front" ? Lands::InFront : text.substr(equals + 1) == "beside" ? Lands::Beside
            : text.substr(equals + 1) == "alone" ? Lands::Alone : Lands::AsBefore};
    if (pair.from.empty() || pair.to.empty() || pair.from == AnyApp || pair.from == pair.to
        || pair.lands == Lands::AsBefore) return std::nullopt;
    return pair;
}

// Where a window of `to` opened from a card of `from` lands. The pair naming
// both wins over the pair naming the card's application with any other; with
// neither, every window stands in front where every one is followed.
template<typename Pairs>
[[nodiscard]] constexpr Lands landsFor(Follows follows, const Pairs &pairs, std::string_view from, std::string_view to)
{
    if (follows == Follows::None) return Lands::AsBefore;
    Lands any = follows == Follows::Every ? Lands::InFront : Lands::AsBefore;
    bool anyFound = false;
    for (const Pair &pair : pairs) {
        if (pair.from != from) continue;
        if (pair.to == to) return pair.lands;
        if (pair.to == AnyApp && !anyFound) {
            any = pair.lands;
            anyFound = true;
        }
    }
    return any;
}
} // namespace Kadunce::OpenedFrom
