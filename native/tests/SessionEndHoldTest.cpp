/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of the sign-out hold. Plasma's sign-out passes through
// saving, a brief normal and quitting while every window closes; no empty
// workspace is tidied away in any of it, and a cancelled sign-out tidies
// again once it has stayed normal.
#include "SessionEndHold.h"

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
    // A session that has only ever been normal holds nothing.
    {
        SessionEndHold hold;
        hold.noteState(true, 0);
        require(!hold.holds(0), "a normal session tidies");
        require(mayDissolveWorkspace(QStringLiteral("d2"), false, hold.holds(0), {}),
                "an empty workspace left by the person goes");
    }

    // Plasma's sign-out: saving, a step back to normal, quitting, then the
    // closing round. Every moment of it holds.
    {
        SessionEndHold hold;
        hold.noteState(false, 1000); // saving
        require(hold.holds(1000), "saving holds");
        hold.noteState(true, 1003); // the step back
        require(hold.holds(1004), "the brief step back to normal still holds");
        hold.noteState(false, 1006); // quitting
        require(hold.holds(1006), "quitting holds");
        // Windows close over the next seconds; the state stays quitting.
        require(hold.holds(1006 + 30000), "the closing round holds however long it takes");
        require(!mayDissolveWorkspace(QStringLiteral("d2"), false, hold.holds(31006), {}),
                "a workspace emptied by the closing round stays");
    }

    // The step back to normal outlived only by less than the wait still holds;
    // a step back that is followed by quitting restarts the wait.
    {
        SessionEndHold hold;
        hold.noteState(false, 0);
        hold.noteState(true, 10);
        require(hold.holds(10 + SessionEndHold::GoesOnMs - 1), "normal for less than the wait holds");
        hold.noteState(false, 1500);
        hold.noteState(true, 1600);
        require(hold.holds(10 + SessionEndHold::GoesOnMs + 1),
                "a later quit restarts the wait from its own step back");
        require(!hold.holds(1600 + SessionEndHold::GoesOnMs), "normal for the whole wait goes on");
    }

    // A cancelled sign-out: once normal for the whole wait, tidying resumes.
    {
        SessionEndHold hold;
        hold.noteState(false, 0);
        hold.noteState(true, 5000);
        require(hold.holds(6999), "a cancel is not trusted before the wait");
        require(!hold.holds(7000), "a cancel that stays normal goes on");
        require(!hold.ending(), "and the session is no longer ending");
        // A repeated normal state does not restart anything.
        hold.noteState(true, 7100);
        require(!hold.holds(7100), "repeated normal states change nothing");
    }

    // Table's tabs and a bring-back keep their workspaces whatever the session.
    {
        const QSet<QString> awaited{QStringLiteral("d3")};
        require(!mayDissolveWorkspace(QStringLiteral("d2"), true, false, {}), "Table open keeps workspaces");
        require(!mayDissolveWorkspace(QStringLiteral("d3"), false, false, awaited),
                "a workspace a bring-back is filling stays");
        require(mayDissolveWorkspace(QStringLiteral("d2"), false, false, awaited),
                "a workspace no bring-back names may go");
    }

    std::cout << "session-end-hold: ok\n";
    return 0;
}
