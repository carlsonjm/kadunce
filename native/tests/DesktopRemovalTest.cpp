/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of removing a workspace. Its windows have moved to a
// neighbour, and the session that held them lets them go whether or not it
// was the one shown, so none stays hidden until Cards is switched off.
#include "DesktopRemoval.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

const QString Removed = QStringLiteral("removed");
const QString Neighbour = QStringLiteral("neighbour");
} // namespace

int main()
{
    // A workspace never shown has no session, and nothing to let go.
    const auto unknown = planDesktopRemoval(false, Removed, Neighbour, Neighbour);
    require(!unknown.release && !unknown.forget && !unknown.switchFirst, "no session, nothing to do");

    // Another workspace removed: its session lets go and is forgotten.
    const auto other = planDesktopRemoval(true, Removed, Neighbour, Neighbour);
    require(other.release && other.forget && !other.switchFirst, "another workspace lets its windows go");

    // The workspace shown removed, KWin announcing the removal before the
    // switch: the workspace shown now becomes current, then the removed
    // session lets go and is forgotten.
    const auto shown = planDesktopRemoval(true, Removed, Removed, Neighbour);
    require(shown.switchFirst, "the workspace shown now becomes current first");
    require(shown.release, "the removed workspace shown lets its windows go");
    require(shown.forget, "the removed workspace shown is forgotten");

    // KWin still naming the removed workspace as shown: its session still
    // lets go, and stays current until the switch arrives.
    const auto still = planDesktopRemoval(true, Removed, Removed, Removed);
    require(still.release && !still.forget && !still.switchFirst,
            "a removed workspace still shown lets go and stays current");
    const auto none = planDesktopRemoval(true, Removed, Removed, QString());
    require(none.release && !none.forget && !none.switchFirst,
            "with no workspace shown the removed one still lets go");

    std::cout << "desktop-removal: ok\n";
    return 0;
}
