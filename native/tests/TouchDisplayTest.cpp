/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of which display holds cards. Each case is a machine a
// person might install Shuffle on, and asserts where its one Spread goes: to
// the display the person chose, else the one a touchscreen drives, else the
// largest, and back to the chosen display when it returns.
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
// Displays that report no physical size, as virtual ones do.
const TouchDisplayOutput SmallVirtual{QStringLiteral("Virtual-0"), false, QSizeF(), QSizeF(1280, 800)};
const TouchDisplayOutput LargeVirtual{QStringLiteral("Virtual-1"), false, QSizeF(), QSizeF(2560, 1440)};
const SpreadDisplayChoice TouchOnly{QString(), false};
SpreadDisplayChoice chosen(const char *name) { return {QString::fromLatin1(name), true}; }
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

    // Without a touchscreen, the largest display holds Spread.
    require(cardOutputName({Monitor, SecondMonitor}, {}) == QStringLiteral("HDMI-A-1"),
        "A desktop with no touchscreen did not put Spread on its largest monitor");
    require(cardOutputName({Z13Panel, Monitor}, {}) == QStringLiteral("DP-1"),
        "A laptop whose panel is not touch kept Spread off its larger monitor");
    require(cardOutputName({Z13Panel}, {}) == QStringLiteral("eDP-1"),
        "A laptop without touch and without a monitor had no Spread");
    require(cardOutputName({SmallVirtual, LargeVirtual}, {}) == QStringLiteral("Virtual-1"),
        "Displays without a physical size were not compared by their size");
    require(cardOutputName({LargeVirtual, Monitor}, {}) == QStringLiteral("Virtual-1"),
        "A display without a physical size was compared in millimetres");
    require(cardOutputName({Monitor, Monitor}, {}) == QStringLiteral("DP-1"),
        "Two displays of one size did not keep screen order");
    require(touchOutputName({Monitor, SecondMonitor}, {}).isEmpty(),
        "A desktop with no touchscreen was said to have a touched display");
    require(cardOutputName({Monitor, SecondMonitor}, {}, TouchOnly).isEmpty(),
        "Cards kept to a touchscreen reached a desktop that has none");
    require(cardOutputName({Z13Panel, Monitor}, {Z13Touch}, TouchOnly) == QStringLiteral("eDP-1"),
        "Cards kept to a touchscreen left the tablet");

    // The chosen display holds Spread while it is attached.
    require(cardOutputName({Z13Panel, Monitor}, {Z13Touch}, chosen("DP-1")) == QStringLiteral("DP-1"),
        "A chosen monitor did not take Spread from the tablet's own panel");
    require(cardOutputName({Monitor, SecondMonitor}, {}, chosen("DP-1")) == QStringLiteral("DP-1"),
        "A chosen monitor lost Spread to a larger one");
    require(cardOutputName({Monitor, SecondMonitor}, {}, SpreadDisplayChoice{QStringLiteral("DP-1"), false})
            == QStringLiteral("DP-1"),
        "A chosen display without touch lost Spread to the touch-only rule");

    // Unplugged, Spread goes to the touchscreen's display, else the largest
    // one left; plugged back in, it returns.
    require(cardOutputName({Z13Panel}, {Z13Touch}, chosen("DP-1")) == QStringLiteral("eDP-1"),
        "Unplugging the chosen monitor did not bring Spread to the touchscreen");
    require(cardOutputName({Z13Panel, Monitor}, {}, chosen("HDMI-A-1")) == QStringLiteral("DP-1"),
        "Unplugging the chosen monitor did not bring Spread to the largest display left");
    require(cardOutputName({Z13Panel, Monitor}, {Z13Touch}, chosen("HDMI-A-1")) == QStringLiteral("eDP-1"),
        "Unplugging the chosen monitor passed over the touchscreen for the larger display");
    require(cardOutputName({Z13Panel, Monitor, SecondMonitor}, {Z13Touch}, chosen("HDMI-A-1"))
            == QStringLiteral("HDMI-A-1"),
        "Plugging the chosen monitor back in did not return Spread to it");
    require(cardOutputName({Monitor}, {}, SpreadDisplayChoice{QStringLiteral("HDMI-A-1"), false}).isEmpty(),
        "Cards kept to a touchscreen survived their chosen display going away");
    require(cardOutputName({}, {}, chosen("DP-1")).isEmpty(),
        "A chosen display with no display attached produced a card display");
    return 0;
}
