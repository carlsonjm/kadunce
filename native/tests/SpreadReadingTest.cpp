// SPDX-License-Identifier: GPL-2.0-or-later
#include "SpreadReading.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
using namespace Qt::StringLiterals;

namespace {
void check(bool ok, const char *message)
{
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
}

int main()
{
    check(spreadEntryName({u"Firefox"_s, u"A more thoughtful web"_s}) == u"Firefox, A more thoughtful web"_s,
          "a card is its application and title");
    check(spreadEntryName({u"Kate"_s, u"Kate"_s}) == u"Kate"_s, "a title that only repeats the application is said once");
    check(spreadEntryName({u"Konsole"_s, u"~"_s, 3}) == u"Konsole, ~, Stack of 3 cards"_s,
          "a Stack says how many cards it holds");
    check(spreadEntryName({u"Elisa"_s, u"Now playing"_s, 1, true}) == u"Elisa, Now playing, sleeping"_s,
          "a sleeping card says so");
    check(spreadEntryName({u"Firefox"_s, u"Docs"_s, 2, false, {u"Firefox"_s, u"Kate"_s}})
              == u"Bento group, Firefox and Kate"_s,
          "the Bento group is named by its panes");
    check(spreadEntryName({}) == u"Card"_s, "a card with nothing to say is still a card");

    const QList<SpreadReadingEntry> row{{u"Firefox"_s, u"Docs"_s}, {u"Kate"_s, u"notes.txt"_s, 2}};
    check(spreadLine(row, 0) == u"Firefox, Docs, 1 of 2"_s, "the chosen entry is said with its place");
    check(spreadLine(row, 1) == u"Kate, notes.txt, Stack of 2 cards, 2 of 2"_s, "a chosen Stack says its size");
    check(spreadLine(row, 4).isEmpty() && spreadLine({}, 0).isEmpty(), "a choice outside the row says nothing");
    return 0;
}
