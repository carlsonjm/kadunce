/* SPDX-License-Identifier: GPL-2.0-or-later */
// The card book reads every record of a card's status at once, on every
// desktop, and names each place they disagree. It reports only: these cases
// check what it names, not that anything is repaired.
#include "CardBook.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce;
using namespace Qt::StringLiterals;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
bool names(const std::vector<CardBookMismatch> &found, quintptr window, CardBookMismatch::Rule rule)
{
    for (const auto &mismatch : found) {
        if (mismatch.window == window && mismatch.rule == rule) return true;
    }
    return false;
}
CardBookEntry card(quintptr window, const QString &screen, const QString &stage = {})
{
    CardBookEntry entry;
    entry.window = window;
    entry.screen = screen;
    entry.card = true;
    entry.cardStage = stage.isEmpty() ? screen : stage;
    entry.cardStages = 1;
    return entry;
}
CardBookEntry pane(quintptr window, const QString &screen)
{
    CardBookEntry entry;
    entry.window = window;
    entry.screen = screen;
    entry.paneOf = screen;
    entry.panes = 1;
    return entry;
}
} // namespace

int main()
{
    const QString monitor = u"DP-1"_s;
    const QString tablet = u"eDP-1"_s;

    // A clean book: a pane on each display, a Stack behind the tablet's pane
    // kept by the tablet's stage, which holds its card.
    {
        auto behind = card(12, tablet);
        behind.behindPanes = {{2, tablet}};
        const std::vector<CardBookEntry> entries{
            pane(1, monitor), pane(2, tablet), card(11, monitor), behind,
        };
        require(checkCardBook(entries).empty(), "A clean book named a mismatch");
        require(checkCardBook({}).empty(), "An empty book named a mismatch");
    }

    // The Stack-carry defect: the pane went to the tablet, the card behind it
    // is still held by the monitor's stage.
    {
        auto behind = card(12, monitor);
        behind.behindPanes = {{2, tablet}};
        const auto found = checkCardBook({pane(2, tablet), behind});
        require(names(found, 12, CardBookMismatch::Rule::StackSplitAcrossDisplays),
            "A Stack left behind by its pane was not named");
        require(found.size() == 1, "A Stack left behind named more than one mismatch");
    }

    // One window held as a card and as a pane at once, or by two layouts.
    {
        auto both = pane(3, tablet);
        both.card = true;
        both.cardStage = tablet;
        both.cardStages = 1;
        require(names(checkCardBook({both}), 3, CardBookMismatch::Rule::CardAndPane),
            "A card that is also a pane was not named");
        auto twice = pane(22, tablet);
        twice.panes = 2;
        require(names(checkCardBook({twice}), 22, CardBookMismatch::Rule::CardAndPane),
            "A pane of two layouts was not named");
    }

    // A layout holding a pane that stands on another display.
    {
        auto strayed = pane(4, tablet);
        strayed.screen = monitor;
        require(names(checkCardBook({strayed}), 4, CardBookMismatch::Rule::PaneOffItsDisplay),
            "A pane off its layout's display was not named");
    }

    // A card held by two stages, as one a desktop not shown still holds, and
    // one held by a display's stage while it stands on another, as one a
    // display that is gone still holds.
    {
        auto twice = card(5, tablet);
        twice.cardStages = 2;
        require(names(checkCardBook({twice}), 5, CardBookMismatch::Rule::CardOnTwoStages),
            "A card of two stages was not named");
        const auto found = checkCardBook({card(6, tablet, u"HDMI-A-1"_s)});
        require(names(found, 6, CardBookMismatch::Rule::CardOffItsDisplay),
            "A card a gone display still holds was not named");
    }

    // A Stack listing a pane, behind a window that is no pane, and a card
    // listed behind two panes; a window not yet a card may be listed.
    {
        auto loose = pane(8, tablet);
        loose.behindPanes = {{2, tablet}};
        CardBookEntry arriving;
        arriving.window = 21;
        arriving.screen = tablet;
        arriving.behindPanes = {{2, tablet}};
        auto twice = card(9, tablet);
        twice.behindPanes = {{2, tablet}, {20, tablet}};
        auto orphan = card(10, tablet);
        orphan.behindPanes = {{11, tablet}};
        const auto found = checkCardBook(
            {pane(2, tablet), pane(20, tablet), card(11, monitor), loose, arriving, twice, orphan});
        require(names(found, 8, CardBookMismatch::Rule::StackMemberIsPane),
            "A pane in another pane's Stack was not named");
        for (const auto &mismatch : found)
            require(mismatch.window != 21, "A window arriving into a Stack was named");
        require(names(found, 9, CardBookMismatch::Rule::InTwoStacks),
            "A card in two Stacks was not named");
        require(names(found, 11, CardBookMismatch::Rule::StackBehindNoPane),
            "A Stack behind a card was not named");
    }

    for (auto rule = int(CardBookMismatch::Rule::CardAndPane);
         rule <= int(CardBookMismatch::Rule::CardOffItsDisplay); ++rule)
        require(!cardBookRuleName(CardBookMismatch::Rule(rule)).isEmpty(), "A rule has no name");

    std::cout << "card book: all cases passed\n";
    return 0;
}
