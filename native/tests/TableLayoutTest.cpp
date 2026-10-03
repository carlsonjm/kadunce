// SPDX-License-Identifier: GPL-2.0-or-later
#include "TableLayout.h"
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
    const auto layout = layoutTable(display, tabs, cards, QString(), u"Empty. It dissolves when you leave."_s, metrics);
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
    std::cout << "Table layout checks passed\n";
    return 0;
}
