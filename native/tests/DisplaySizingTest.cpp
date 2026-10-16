/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of Solo and Connected sizing. Each case is a display a
// person might keep Spread on, by its size at its own scale, and asserts how it
// plays a card: Kadunce chooses by usable room, and the person's choice for
// the display wins.
#include "DisplaySizing.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
} // namespace

int main()
{
    // The Z13 at 150%, a 14-inch laptop at 175%, a 15-inch at 125%.
    require(automaticSizing(1706, 1066) == DisplaySizing::Solo,
        "A tablet's panel was not Solo");
    require(automaticSizing(1646, 1029) == DisplaySizing::Solo,
        "A 14-inch laptop was not Solo");
    require(automaticSizing(1536, 864) == DisplaySizing::Solo,
        "A 15-inch laptop at 125% was not Solo");
    // A 1080p monitor, and a 1440p monitor.
    require(automaticSizing(1920, 1080) == DisplaySizing::Connected,
        "A 1080p monitor was not Connected");
    require(automaticSizing(2560, 1440) == DisplaySizing::Connected,
        "A 1440p monitor was not Connected");
    // An upright monitor is too narrow to sit three across, but tall enough to
    // stack them: room, not orientation, decides.
    require(automaticSizing(1080, 1920) == DisplaySizing::Solo,
        "An upright 1080p monitor was not Solo");
    require(automaticSizing(1440, 2560) == DisplaySizing::Solo,
        "An upright 1440p monitor was not Solo");

    require(displaySizing(QStringLiteral("Connected"), 1706, 1066) == DisplaySizing::Connected,
        "Choosing Connected for a tablet was not kept");
    require(displaySizing(QStringLiteral("solo"), 2560, 1440) == DisplaySizing::Solo,
        "Choosing Solo for a monitor was not kept");
    // Choices saved before Solo had its name say "active" and still read as Solo.
    require(displaySizing(QStringLiteral("active"), 2560, 1440) == DisplaySizing::Solo
        && displaySizing(QStringLiteral(" Active "), 2560, 1440) == DisplaySizing::Solo,
        "A choice saved as active was not read as Solo");
    require(displaySizing(QString(), 2560, 1440) == DisplaySizing::Connected
        && displaySizing(QStringLiteral("sideways"), 1706, 1066) == DisplaySizing::Solo,
        "An empty or unknown choice did not let Kadunce choose");
    require(sizingName(DisplaySizing::Connected) == QStringLiteral("connected")
        && sizingName(DisplaySizing::Solo) == QStringLiteral("solo")
        && parseSizing(sizingName(DisplaySizing::Solo)) == DisplaySizing::Solo,
        "A sizing did not survive being written and read back");

    // A choice follows its display's identity to a new name, and a name a
    // dock has since given another display carries no choice to it.
    const QHash<QString, QString> choices{{QStringLiteral("DP-1"), QStringLiteral("solo")}};
    const QHash<QString, QString> identities{{QStringLiteral("DP-1"), QStringLiteral("dell")}};
    require(chosenSizing(choices, identities, QStringLiteral("DP-1"), QStringLiteral("dell")) == QStringLiteral("solo"),
        "A display's choice was not kept under its own name");
    require(chosenSizing(choices, identities, QStringLiteral("DP-3"), QStringLiteral("dell")) == QStringLiteral("solo"),
        "A display renumbered by a dock lost its choice");
    require(chosenSizing(choices, identities, QStringLiteral("DP-1"), QStringLiteral("lg")).isEmpty(),
        "Another display given the old name took its choice");
    // Choices kept before identities were are read by name.
    require(chosenSizing(choices, {}, QStringLiteral("DP-1"), QStringLiteral("lg")) == QStringLiteral("solo")
        && chosenSizing(choices, {}, QStringLiteral("DP-1"), QString()) == QStringLiteral("solo"),
        "A choice kept by name alone was not read");
    require(chosenSizing(choices, identities, QStringLiteral("HDMI-A-1"), QStringLiteral("lg")).isEmpty(),
        "A display never chosen for carried a choice");
    std::cout << "display sizing: ok\n";
    return 0;
}
