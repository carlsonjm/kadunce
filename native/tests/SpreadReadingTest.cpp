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

    const SpreadReading reading = readSpread({{u"Firefox"_s, u"Docs"_s}, {u"Kate"_s, u"notes.txt"_s}}, 1);
    check(reading.names == QStringList{u"Firefox, Docs"_s, u"Kate, notes.txt"_s}, "every entry is named, in row order");
    check(reading.places == QStringList{u"1 of 2"_s, u"2 of 2"_s}, "every entry says where it stands");
    check(reading.current == 1, "the chosen entry holds the focus");
    check(readSpread({{u"Firefox"_s, u""_s}}, 4).current == -1, "a choice outside the row names nothing");
    check(readSpread({{u"A"_s, u"B"_s}}, 0) == readSpread({{u"A"_s, u"B"_s}}, 0), "an unchanged row reads the same");
    return 0;
}
