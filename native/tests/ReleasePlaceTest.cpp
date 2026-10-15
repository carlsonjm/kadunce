/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of where a card's window is given back when Cards let
// it go. A record keeps the place and the display it was taken on; a place
// whose display has been unplugged is carried onto a display there is, so no
// window is given back where no display shows it.
#include "ReleasePlace.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

// The tablet holds Spread; the monitor stood to its right before it was
// unplugged.
const ReleaseDisplay Tablet{QStringLiteral("Virtual-0"), QRectF(0, 0, 1280, 800), QRectF(0, 0, 1280, 736)};
const ReleaseDisplay Monitor{QStringLiteral("Virtual-1"), QRectF(1280, 0, 1280, 800), QRectF(1280, 0, 1280, 800)};
const QRectF MonitorGeometry = Monitor.geometry;

bool onSomeDisplay(const QList<ReleaseDisplay> &displays, const QRectF &place)
{
    for (const auto &display : displays)
        if (display.geometry.contains(place)) return true;
    return false;
}

QRectF given(const QList<ReleaseDisplay> &displays, const ReleaseRecord &record,
    const QString &own = {}, const QString &standing = Tablet.name)
{
    const auto carry = releaseCarry(displays, record, own, standing);
    return carry ? carriedPlace(record.place, carry->from, carry->display.work) : record.place;
}
} // namespace

int main()
{
    // A pane put away off the monitor's layout kept the monitor's place.
    const ReleaseRecord fromMonitor{QRectF(1500, 100, 600, 450), Monitor.name, MonitorGeometry};

    // The monitor still there: the window goes back where it began.
    require(!releaseCarry({Tablet, Monitor}, fromMonitor, {}, Tablet.name),
            "with its display there, a place is kept");

    // The monitor unplugged: the place is carried onto the tablet, inside its
    // room, the same share of the way across and down.
    const QList<ReleaseDisplay> unplugged{Tablet};
    const auto carry = releaseCarry(unplugged, fromMonitor, {}, Tablet.name);
    require(carry && carry->display.name == Tablet.name, "an unplugged display's place goes to the tablet");
    const QRectF place = given(unplugged, fromMonitor);
    require(onSomeDisplay(unplugged, place), "the carried place stands on a display there is");
    require(Tablet.work.contains(place), "the carried place stands in the room panels leave");
    require(place.size() == fromMonitor.place.size(), "a place that fits keeps its size");
    require(qFuzzyCompare(place.x(), (1500.0 - 1280.0) / (1280.0 - 600.0) * (1280.0 - 600.0)),
            "it keeps its share of the way across");

    // A display plugged in where the old one stood is not the old one: the
    // place is still carried, from where it stood on the display that left.
    const ReleaseDisplay Other{QStringLiteral("DP-2"), MonitorGeometry, MonitorGeometry};
    const auto replaced = releaseCarry({Tablet, Other}, fromMonitor, {}, Tablet.name);
    require(replaced && replaced->display.name == Tablet.name && replaced->from == MonitorGeometry,
            "a place whose display is gone is carried even where another now stands");

    // A record that does not know its display: carried only when no display
    // holds the place.
    const ReleaseRecord unknown{QRectF(1500, 100, 600, 450), QString(), QRectF()};
    require(!releaseCarry({Tablet, Monitor}, unknown, {}, Tablet.name), "an unknown display's place on a display is kept");
    const QRectF centred = given(unplugged, unknown);
    require(onSomeDisplay(unplugged, centred), "an unknown display's place off every display comes onto one");
    require(qFuzzyCompare(centred.x(), (1280.0 - 600.0) / 2.0), "with nothing to measure by, it is centred");

    // A window larger than the tablet's room is made to fit it.
    const ReleaseRecord large{QRectF(1280, 0, 1280, 800), Monitor.name, MonitorGeometry};
    require(Tablet.work.contains(given(unplugged, large)), "a large place is fitted into the room");

    // A place on the tablet itself never moves.
    const ReleaseRecord onTablet{QRectF(100, 100, 600, 450), Tablet.name, Tablet.geometry};
    require(!releaseCarry(unplugged, onTablet, {}, Tablet.name), "a place on the tablet is kept");

    // §11, Every display: a card goes back on the display it belongs to.
    require(!releaseCarry({Tablet, Monitor}, fromMonitor, Monitor.name, Tablet.name),
            "a card belonging to the monitor keeps its place there");
    const auto toOwn = releaseCarry({Tablet, Monitor}, onTablet, Monitor.name, Tablet.name);
    require(toOwn && toOwn->display.name == Monitor.name, "a card belonging to the monitor goes back on it");
    require(Monitor.geometry.contains(given({Tablet, Monitor}, onTablet, Monitor.name)),
            "carried onto its own display, it stands there");

    // The window stands nowhere known: the first display takes it.
    const auto first = releaseCarry(unplugged, fromMonitor, {}, QString());
    require(first && first->display.name == Tablet.name, "with no display named, the first one takes it");
    require(!releaseCarry({}, fromMonitor, {}, Tablet.name), "with no display at all, nothing moves");

    std::cout << "release-place: ok\n";
    return 0;
}
