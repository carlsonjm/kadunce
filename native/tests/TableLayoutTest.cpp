// SPDX-License-Identifier: GPL-2.0-or-later
#include "TableLayout.h"
#include "TableSavedLayouts.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
using namespace Qt::StringLiterals;

namespace {
void check(bool ok, const char *message)
{
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
// Seven pixels a character at 14 px, scaled with the size.
const TableMetrics metrics{[](const QString &text, double size, bool) { return text.size() * size * 0.5; }};
}

int main()
{
    const QSizeF display(1463, 914);
    const QList<TableTabContent> tabs{{u"Personal"_s, true, 3, 1}, {u"Work"_s, false, 2, 2}, {u"Research"_s, false, 0, 3}};
    const QList<QList<TableCardContent>> cards{
        {{u"Firefox"_s, u"A more thoughtful web"_s}, {u"Elisa"_s, u"Now playing"_s}},
        {{u"Kate"_s, u"TableModel.cpp"_s}},
        {}};
    const auto layout = layoutTable(display, tabs, cards, QString(), u"Empty, right click to name it"_s, metrics);
    check(layout.parts == TableTabParts::All, "an uncrowded row shows everything");
    check(layout.plus.width() == layout.row && layout.plus.height() == layout.row, "+ at rest is a circle");
    check(layout.row == 46.0 && layout.tabsTop > 0 && layout.trayTop > layout.tabsTop + layout.row, "rows stack under the edge");
    check(layout.depth > layout.tabsTop + layout.row && layout.depth < layout.trayTop,
          "the depth line sits between the tabs and the cards");
    check(layout.depth - layout.tabs[0].bottom() == TableSizes::LineGap && layout.depth < layout.trayTop,
          "the depth line sits 5 mm under the tabs, above the cards");
    check(layout.lift - layout.cards[0][0].bottom() == TableSizes::LineGap, "the lift line sits 5 mm under the cards");
    check(layout.tabs.size() == 3 && layout.cards.size() == 3, "every workspace is laid out");
    for (int i = 0; i + 1 < layout.tabs.size(); ++i)
        check(layout.tabs[i].right() < layout.tabs[i + 1].left(), "tabs never overlap");
    check(layout.tabs.last().right() < layout.plus.left(), "+ comes after the tabs");
    const double left = layout.tabs.first().left();
    const double right = display.width() - layout.plus.right();
    check(std::abs(left - right) < 1.0, "the row is centred");
    check(layout.remove.isEmpty(), "nothing offers removal while no name is being typed");
    for (int w = 0; w < layout.cards.size(); ++w) {
        for (const auto &card : layout.cards[w]) {
            check(card.top() == layout.trayTop && card.left() >= 12 && card.right() <= display.width() - 12,
                  "cards hang on the display, below the tabs");
        }
        for (int i = 0; i + 1 < layout.cards[w].size(); ++i)
            check(layout.cards[w][i].right() < layout.cards[w][i + 1].left(), "cards never overlap");
    }
    const auto &work = layout.cards[1];
    check(std::abs(work.first().center().x() - layout.tabs[1].center().x()) < 1.0, "one card hangs straight under its tab");
    check(layout.cards[2].isEmpty() && layout.empty[2].center().x() > layout.tabs[1].right(), "an empty workspace says so under its own tab");

    const auto carrying = layoutTable(display, tabs, cards, u"New"_s, u"Empty."_s, metrics);
    check(carrying.plus.width() > carrying.row, "+ says New while a card is carried to it");
    const auto full = layoutTable(display, tabs, cards, QString(), u"Empty."_s, metrics, false);
    check(full.plus.isEmpty() && std::abs(full.tabs.first().left() - (display.width() - full.tabs.last().right())) < 1.0,
          "with no more room for desktops there is no +, and the tabs stay centred");

    const auto crowd = [&](int count) {
        QList<TableTabContent> many;
        for (int i = 0; i < count; ++i) many.append({QStringLiteral("A rather long workspace %1").arg(i), i == 0, 4, i + 1});
        const auto crowded = layoutTable(display, many, {}, QString(), u"Empty."_s, metrics);
        check(crowded.tabs.size() == count && crowded.plus.right() <= display.width() - 11.0,
              "every workspace and + stay on the display");
        for (int i = 0; i + 1 < crowded.tabs.size(); ++i)
            check(crowded.tabs[i].right() < crowded.tabs[i + 1].left(), "crowded tabs never overlap");
        for (const auto &tab : crowded.tabs) check(tab.width() >= crowded.row - 1e-9, "a tab is never narrower than a circle");
        const double floor = crowded.parts == TableTabParts::NamesOnly ? TableSizes::NameMinimum : TableSizes::NameComfortable;
        if (crowded.parts != TableTabParts::Numbers)
            for (double name : crowded.nameWidths) check(name >= floor - 1e-9, "names never shrink below what their tier keeps");
        return crowded;
    };
    check(crowd(5).parts == TableTabParts::All, "a few workspaces shorten their names and keep everything");
    check(crowd(8).parts == TableTabParts::NoColours, "more give up the application colours first, keeping their names");
    check(crowd(11).parts == TableTabParts::NamesOnly, "then the names shorten further");
    check(crowd(13).parts == TableTabParts::NamesOnly, "the names shorten further before they go");
    const auto most = crowd(25);
    check(most.parts == TableTabParts::Numbers, "KDE's most, 25, are numbers");
    for (int i = 0; i < most.tabs.size(); ++i)
        check(most.tabs[i].width() == most.row && most.nameWidths[i] == 0.0, "a number-only tab is a circle with no name");

    const QList<TableTabContent> unnamed{{u"Code"_s, true, 1, 1}, {QString(), false, 0, 2}};
    const auto fresh = layoutTable(display, unnamed, {}, QString(), u"Empty."_s, metrics);
    check(fresh.nameWidths[1] == 0.0 && fresh.tabs[1].width() >= fresh.row && fresh.tabs[1].width() < fresh.tabs[0].width(),
          "a workspace nothing has named is its number");

    // The tab being renamed shows what has been typed whole, however crowded
    // the row, and cleared it offers removal beside it.
    QList<TableTabContent> typing;
    for (int i = 0; i < 20; ++i) typing.append({QStringLiteral("A rather long workspace %1").arg(i), i == 0, 4, i + 1});
    typing[3].name = u"Somewhere new"_s;
    typing[3].editing = true;
    const auto typed = layoutTable(display, typing, {}, QString(), u"Empty."_s, metrics);
    check(typed.parts == TableTabParts::Numbers && typed.nameWidths[3] == metrics.measure(u"Somewhere new"_s, 14, true) + 2.0,
          "a name being typed stays whole in a row of numbers");
    for (int i = 0; i + 1 < typed.tabs.size(); ++i)
        check(typed.tabs[i].right() < typed.tabs[i + 1].left(), "a name being typed pushes its neighbours aside");
    check(typed.plus.right() <= display.width() - 11.0, "and the row still fits the display");
    QList<TableTabContent> cleared = tabs;
    cleared[1].name.clear();
    cleared[1].editing = true;
    const auto removing = layoutTable(display, cleared, cards, QString(), u"Empty."_s, metrics, true, u"Remove workspace"_s);
    check(removing.nameWidths[1] == TableSizes::NameMinimum, "a cleared name keeps room for the cursor");
    check(!removing.remove.isEmpty() && removing.remove.left() > removing.tabs[1].right()
              && removing.tabs[2].left() > removing.remove.right() && removing.plus.left() > removing.tabs[2].right(),
          "a cleared name offers removal beside its tab");
    check(std::abs(removing.tabs.first().left() - (display.width() - removing.plus.right())) < 1.0,
          "the row stays centred with it");
    // A stack is one card, its edges to the left of its face.
    const QList<QList<TableCardContent>> decks{{{u"Firefox"_s, u"A more thoughtful web"_s, 2}, {u"Elisa"_s, u"Now playing"_s}}, {}, {}};
    const auto dealt = layoutTable(display, tabs, decks, QString(), u"Empty."_s, metrics);
    check(std::abs(dealt.cards[0][0].width() - layout.cards[0][0].width() - 2 * TableSizes::SliverStep) < 1e-9
              && dealt.cards[0][1].left() - dealt.cards[0][0].right() == TableSizes::CardGap,
          "a stack's slot holds its edges, and keeps the gap to its neighbour");
    // Table lists an application's own windows, and none kept out of the
    // taskbar, the pager or the task switcher.
    check(tableListsWindow({}), "an ordinary window hangs in its workspace's row");
    check(!tableListsWindow({.normal = false}), "a panel or popup does not");
    check(!tableListsWindow({.onAllDesktops = true}), "a window on every desktop does not");
    check(!tableListsWindow({.dependent = true}), "a dialog following its application does not");
    check(!tableListsWindow({.skipSwitcher = true}), "a window kept out of the task switcher does not");
    check(!tableListsWindow({.skipTaskbar = true}), "a window kept out of the taskbar does not");
    check(!tableListsWindow({.skipPager = true}), "a window kept out of the pager does not");
    check(!tableListsWindow({.skipSwitcher = true, .skipTaskbar = true, .skipPager = true}),
          "a window kept out of all three does not");
    {
        // Cards under + hang centred under it, as a tab's do.
        const QList<TableCardContent> under{{u"Empty workspace"_s, u"Nothing open"_s, 0}, {u"Writing"_s, u"2 windows"_s, 0}};
        const auto plus = layoutTable(display, tabs, cards, QString(), u"Empty."_s, metrics, true, QString(), under);
        check(plus.plusCards.size() == 2 && plus.plusTextWidths.size() == 2 && plus.plusContents.size() == 2,
              "each card under + is laid out");
        const double middle = (plus.plusCards.first().left() + plus.plusCards.last().right()) / 2.0;
        check(std::abs(middle - plus.plus.center().x()) < 1.0 || plus.plusCards.last().right() <= display.width() - 11.9,
              "the cards under + hang centred under it, or as near as the display allows");
        check(plus.plusCards.first().top() == plus.trayTop && plus.plusCards.first().height() == plus.cardHeight,
              "they hang where a tab's cards hang, as tall");
        check(plus.cards == layoutTable(display, tabs, cards, QString(), u"Empty."_s, metrics).cards,
              "the workspaces' cards stand where they did");
        const auto none = layoutTable(display, tabs, cards, QString(), u"Empty."_s, metrics, false, QString(), under);
        check(none.plusCards.isEmpty(), "with no + nothing hangs under it");
    }
    {
        // A Layout read as Core gives it: two displays side by side.
        const QVariantMap left{{u"apps"_s, QStringList{u"org.kde.kate"_s}}, {u"display"_s, u"eDP-1"_s}, {u"shown"_s, true},
                               {u"pictureX"_s, 0.0}, {u"pictureY"_s, 0.0}, {u"pictureWidth"_s, 0.5}, {u"pictureHeight"_s, 1.0},
                               {u"displayX"_s, 0.0}, {u"displayY"_s, 0.0}, {u"displayWidth"_s, 1000.0}, {u"displayHeight"_s, 500.0}};
        const QVariantMap stack{{u"apps"_s, QStringList{u"firefox"_s, u"org.kde.dolphin"_s}}, {u"stack"_s, true},
                                {u"display"_s, u"DP-1"_s}, {u"shown"_s, true},
                                {u"pictureX"_s, 0.5}, {u"pictureY"_s, 0.0}, {u"pictureWidth"_s, 0.5}, {u"pictureHeight"_s, 1.0},
                                {u"displayX"_s, 1000.0}, {u"displayY"_s, 0.0}, {u"displayWidth"_s, 1000.0}, {u"displayHeight"_s, 1000.0}};
        const auto read = tableSavedLayout(u"3"_s, u" Writing "_s, {left, stack});
        check(read.id == u"3"_s && read.name == u"Writing"_s && read.frontApp == u"org.kde.kate"_s && read.windows == 3,
              "a Layout reads its name, its first app and every window in every group");
        check(std::abs(read.aspect - 2.0) < 1e-9 && read.displays.size() == 2 && read.pictured.size() == 2,
              "its desk is both displays, side by side");
        check(read.displays.first() == QRectF(0, 0, 0.5, 0.5) && read.pictured.last() == QRectF(0.75, 0, 0.25, 1),
              "each display and window stands in shares of the whole desk");
        check(read.picturedApps == QStringList({u"org.kde.kate"_s, u"firefox"_s}), "each pictured window names its front app");
        QVariantMap bare{{u"apps"_s, QStringList{u"org.kde.kate"_s}}};
        const auto old = tableSavedLayout(u"1"_s, u"Old"_s, {bare, stack});
        check(old.aspect == 0.0 && old.pictured.isEmpty() && old.windows == 3 && old.frontApp == u"org.kde.kate"_s,
              "a Layout kept without places has its icon and no picture");
    }
    {
        // A Stack's cards hang centred under it, on a row of their own under
        // the cards, between the lift line and their own.
        QList<QList<TableCardContent>> stacked = cards;
        stacked[1][0].stacked = 2;
        stacked[1][0].members = {{u"Kate"_s, u"TableModel.cpp"_s}, {u"Kate"_s, u"notes.md"_s}, {u"Dolphin"_s, u"kadunce"_s}};
        const auto hung = layoutTable(display, tabs, stacked, QString(), u"Empty."_s, metrics);
        check(hung.members.size() == 3 && hung.members[1].size() == 1 && hung.members[1][0].size() == 3
                  && hung.members[0].size() == 2 && hung.members[0][0].isEmpty(),
              "each Stack's cards are laid out; a lone card has none");
        const auto &row = hung.members[1][0];
        check(row[0].top() == hung.membersTop && hung.membersTop > hung.lift && hung.membersLift > row[0].bottom(),
              "the Stack's cards hang under the lift line, their own line under them");
        check(hung.membersTop - hung.lift == hung.trayTop - hung.depth, "they hang under their line as the cards do");
        check(std::abs((row.first().left() + row.last().right()) / 2.0 - hung.cards[1][0].center().x()) < 1.0
                  || row.first().left() == TableSizes::TrayEdge || row.last().right() == display.width() - TableSizes::TrayEdge,
              "centred under the Stack, kept on the display");
        for (int i = 0; i + 1 < row.size(); ++i) check(row[i].right() < row[i + 1].left(), "its cards never overlap");
    }
    std::cout << "Table layout checks passed\n";
    return 0;
}
