/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of which display holds cards. Each case is a machine a
// person might install Shuffle on, and asserts where its cards go: to the
// display a touchscreen drives, and nowhere when none does.
#include "TouchDisplay.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

// The Z13's panel reports 288 x 180 mm and its digitizer 284 x 187.6 mm, so
// only the built-in fallback places it.
const TouchDisplayOutput Z13Panel{QStringLiteral("eDP-1"), true, QSizeF(288, 180)};
const TouchDisplayDevice Z13Touch{QString(), QSizeF(284, 187.636)};
const TouchDisplayOutput Monitor{QStringLiteral("DP-1"), false, QSizeF(527, 296)};
const TouchDisplayDevice MonitorTouch{QString(), QSizeF(527, 296)};
const TouchDisplayOutput SecondMonitor{QStringLiteral("HDMI-A-1"), false, QSizeF(598, 336)};
} // namespace

int main()
{
    require(cardOutputName({Z13Panel}, {Z13Touch}) == QStringLiteral("eDP-1"),
        "A tablet's own touchscreen did not hold its cards");
    require(cardOutputName({Z13Panel, Monitor}, {Z13Touch}) == QStringLiteral("eDP-1"),
        "Plugging in a plain monitor moved a tablet's cards");

    require(cardOutputName({Monitor}, {MonitorTouch}) == QStringLiteral("DP-1"),
        "A desktop's touch monitor did not hold cards");
    require(cardOutputName({SecondMonitor, Monitor}, {MonitorTouch}) == QStringLiteral("DP-1"),
        "Cards went to the first monitor rather than the touched one");

    require(cardOutputName({Monitor, SecondMonitor}, {}).isEmpty(),
        "A desktop with no touchscreen was given cards");
    require(cardOutputName({Z13Panel, Monitor}, {}).isEmpty(),
        "A laptop whose panel is not touch was given cards on it");
    require(cardOutputName({Z13Panel, Monitor}, {MonitorTouch}) == QStringLiteral("DP-1"),
        "A laptop's touch monitor did not hold cards over its untouchable panel");

    require(cardOutputName({Monitor, Z13Panel}, {MonitorTouch, Z13Touch}) == QStringLiteral("eDP-1"),
        "Docking a tablet to a touch monitor moved its cards off the tablet");
    require(cardOutputName({Monitor, SecondMonitor},
                {TouchDisplayDevice{QStringLiteral("HDMI-A-1"), QSizeF()}})
            == QStringLiteral("HDMI-A-1"),
        "A touchscreen the user assigned to a display was placed elsewhere");
    require(cardOutputName({SecondMonitor, Monitor},
                {TouchDisplayDevice{QString(), QSizeF(100, 100)}})
            == QStringLiteral("HDMI-A-1"),
        "An unplaceable touchscreen did not fall back to the first display, as KWin does");
    require(cardOutputName({}, {MonitorTouch}).isEmpty(),
        "A touchscreen with no display produced a card display");
    return 0;
}
