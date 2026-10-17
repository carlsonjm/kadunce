/* SPDX-License-Identifier: GPL-2.0-or-later */
// The card book reads every record of a card's status at once and names each
// place they disagree. It reports only: these cases check what it names, not
// that anything is repaired.
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
CardBookEntry card(quintptr window, const QString &screen, const QString &home = {})
{
    CardBookEntry entry;
    entry.window = window;
    entry.screen = screen;
    entry.card = true;
    entry.home = home;
    return entry;
}
CardBookEntry pane(quintptr window, const QString &screen)
{
    CardBookEntry entry;
    entry.window = window;
    entry.screen = screen;
    entry.paneOf = screen;
    return entry;
}
} // namespace

int main()
{
    const QString monitor = u"DP-1"_s;
    const QString tablet = u"eDP-1"_s;
    const QList<CardBookDisplay> displays{
        {monitor, true, true, false},
        {tablet, false, true, true},
    };

    // A clean book: a pane on each display, a Stack behind the tablet's pane
    // waiting for the tablet, a Stack in Spread's row waiting together.
    {
        auto behind = card(12, monitor, tablet);
        behind.behindPanes = {2};
        auto face = card(13, monitor);
        face.stackFace = 13;
        auto under = card(14, monitor);
        under.stackFace = 13;
        const std::vector<CardBookEntry> entries{
            pane(1, monitor), pane(2, tablet), card(11, monitor), behind, face, under,
        };
        require(checkCardBook(displays, entries).empty(), "A clean book named a mismatch");
        require(checkCardBook({}, {}).empty(), "An empty book named a mismatch");
    }

    // The Stack-carry defect: the pane went to the tablet, the card behind it
    // still waits for Spread's display.
    {
        auto behind = card(12, monitor);
        behind.behindPanes = {2};
        const auto found = checkCardBook(displays, {pane(2, tablet), behind});
        require(names(found, 12, CardBookMismatch::Rule::StackSplitAcrossDisplays),
            "A Stack left behind by its pane was not named");
        require(found.size() == 1, "A Stack left behind named more than one mismatch");
    }

    // Behind a pane on Spread's display, a card waits for no other display.
    {
        auto behind = card(12, monitor, tablet);
        behind.behindPanes = {1};
        require(names(checkCardBook(displays, {pane(1, monitor), behind}), 12,
                    CardBookMismatch::Rule::StackSplitAcrossDisplays),
            "A card behind a Spread pane waiting for the tablet was not named");
    }

    // One window held as a card and as a pane at once.
    {
        auto both = pane(3, tablet);
        both.card = true;
        require(names(checkCardBook(displays, {both}), 3, CardBookMismatch::Rule::CardAndPane),
            "A card that is also a pane was not named");
    }

    // A layout holding a pane that stands on another display.
    {
        auto strayed = pane(4, tablet);
        strayed.screen = monitor;
        require(names(checkCardBook(displays, {strayed}), 4, CardBookMismatch::Rule::PaneOffItsDisplay),
            "A pane off its layout's display was not named");
    }

    // Waiting for Spread's display, or one keeping no cards, is no waiting
    // place; waiting for a display that is gone is, until it returns.
    {
        const QList<CardBookDisplay> plain{{monitor, true, true, false}, {tablet, false, false, true}};
        const auto found = checkCardBook(plain,
            {card(5, monitor, monitor), card(6, monitor, tablet), card(19, monitor, u"HDMI-A-1"_s)});
        require(names(found, 5, CardBookMismatch::Rule::WaitsForNoCardDisplay),
            "A card waiting for Spread's display was not named");
        require(names(found, 6, CardBookMismatch::Rule::WaitsForNoCardDisplay),
            "A card waiting for a display keeping no cards was not named");
        require(found.size() == 2, "A card waiting for a display that is gone was named");
    }

    // A pane that is also said to wait.
    {
        auto waiting = pane(7, tablet);
        waiting.home = tablet;
        require(names(checkCardBook(displays, {waiting}), 7, CardBookMismatch::Rule::PaneWaits),
            "A pane that also waits was not named");
    }

    // A Stack listing a pane, behind a window that is no pane, and a card
    // listed behind two panes; a window not yet a card may be listed.
    {
        auto loose = pane(8, tablet);
        loose.behindPanes = {2};
        CardBookEntry arriving;
        arriving.window = 21;
        arriving.screen = tablet;
        arriving.home = tablet;
        arriving.behindPanes = {2};
        auto twice = card(9, monitor, tablet);
        twice.behindPanes = {2, 20};
        auto orphan = card(10, monitor, tablet);
        orphan.behindPanes = {11};
        const auto found = checkCardBook(displays,
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

    // The cards of one Stack in Spread's row waiting for different displays.
    {
        auto face = card(13, monitor);
        face.stackFace = 13;
        auto under = card(14, monitor, tablet);
        under.stackFace = 13;
        require(names(checkCardBook(displays, {face, under}), 13,
                    CardBookMismatch::Rule::RowStackSplitAcrossDisplays),
            "A row Stack split across displays was not named");
    }

    // A Solo display shows one card; a minimized pane is not shown, and
    // Spread's display is never Solo-checked.
    {
        auto hidden = pane(16, tablet);
        hidden.minimized = true;
        require(checkCardBook(displays, {pane(15, tablet), hidden, pane(1, monitor), pane(17, monitor)}).empty(),
            "One shown card on a Solo display was named");
        const auto found = checkCardBook(displays, {pane(15, tablet), pane(18, tablet)});
        require(names(found, 0, CardBookMismatch::Rule::SoloShowsTwo),
            "A Solo display showing two cards was not named");
    }

    // A display's own stage shows its card: it fronts a Stack as a pane does
    // and counts as shown; held by Spread's stage as well, it is held twice.
    {
        CardBookEntry shown;
        shown.window = 30;
        shown.screen = tablet;
        shown.stage = tablet;
        auto behind = card(31, monitor, tablet);
        behind.behindPanes = {30};
        require(checkCardBook(displays, {shown, behind}).empty(),
            "A display stage's card and its Stack were named");
        auto twice = shown;
        twice.card = true;
        require(names(checkCardBook(displays, {twice}), 30, CardBookMismatch::Rule::CardAndPane),
            "A card held by Spread and a display stage was not named");
        auto strayed = shown;
        strayed.screen = monitor;
        require(names(checkCardBook(displays, {strayed}), 30, CardBookMismatch::Rule::PaneOffItsDisplay),
            "A display stage's card off its display was not named");
        require(names(checkCardBook(displays, {shown, pane(32, tablet)}), 0,
                    CardBookMismatch::Rule::SoloShowsTwo),
            "A Solo display showing a stage card and a pane was not named");
    }

    for (auto rule = int(CardBookMismatch::Rule::CardAndPane);
         rule <= int(CardBookMismatch::Rule::SoloShowsTwo); ++rule)
        require(!cardBookRuleName(CardBookMismatch::Rule(rule)).isEmpty(), "A rule has no name");

    std::cout << "card book: all cases passed\n";
    return 0;
}
