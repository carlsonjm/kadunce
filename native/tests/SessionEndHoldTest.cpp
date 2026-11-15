/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of the sign-out hold. Plasma's sign-out passes through
// saving and a brief normal, then closes every window and stops the session
// with no further state; no empty workspace is tidied away and nothing is
// given back in any of it, and a sign-out that waits on the person or is
// cancelled goes on.
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

    // Plasma's sign-out as it runs: its program comes to the bus, the session
    // saves and steps back to normal, the windows are recorded for well over
    // a second, every window closes at once, the program leaves and the
    // session's services stop, and the compositor is stopped a second later.
    {
        SessionEndHold hold;
        hold.noteSignOutProgram(true, 950);
        hold.noteState(false, 1000);
        hold.noteState(true, 1004);
        hold.noteWindowClosed(1280);
        require(hold.holds(1004 + SessionEndHold::GoesOnMs + 300),
                "normal for the old wait holds while the sign-out program runs");
        hold.noteWindowClosed(2371);
        hold.noteWindowClosed(2414);
        hold.noteSignOutProgram(false, 2420);
        hold.noteSessionStopping(2476);
        require(hold.holds(3300), "the session being stopped holds until the compositor goes");
        require(hold.holds(2420 + SessionEndHold::GoesOnMs + 1), "and is no cancel, though the program left");
        require(hold.holds(2476 + SessionEndHold::StoppingLongestMs - 1), "for as long as stopping may take");
        require(!hold.holds(2476 + SessionEndHold::StoppingLongestMs), "a session never stopped is given back");
    }

    // Cancelled: the program leaves and nothing of the session stops.
    {
        SessionEndHold hold;
        hold.noteSignOutProgram(true, 950);
        hold.noteState(false, 1000);
        hold.noteState(true, 1004);
        hold.noteSignOutProgram(false, 9000);
        require(hold.goesOnAt(9000) == 9000 + SessionEndHold::GoesOnMs, "a cancel is told from the program leaving");
        require(hold.holds(9000 + SessionEndHold::GoesOnMs - 1), "and is not trusted before the wait");
        require(!hold.holds(9000 + SessionEndHold::GoesOnMs), "a cancelled sign-out goes on");
    }

    // Restart and shut down keep the program to the end; the services say it.
    {
        SessionEndHold hold;
        hold.noteSignOutProgram(true, 0);
        hold.noteState(false, 50);
        hold.noteState(true, 54);
        hold.noteWindowClosed(1400);
        hold.noteSessionStopping(1450);
        require(hold.holds(1400 + SessionEndHold::WaitingMs + 1), "a restart is not a sign-out waiting on anyone");
    }

    // A window that asks inside itself opens nothing: the closing round goes
    // quiet with the program still there, and the session is shown again.
    {
        SessionEndHold hold;
        hold.noteSignOutProgram(true, 0);
        hold.noteState(false, 50);
        hold.noteState(true, 54);
        require(hold.holds(54 + SessionEndHold::WaitingMs - 1), "recording the windows is not a wait on anyone");
        hold.noteWindowClosed(1400);
        require(hold.holds(54 + SessionEndHold::WaitingMs), "a window closing restarts the quiet");
        require(hold.holds(1400 + SessionEndHold::WaitingMs - 1), "quiet for less than the wait holds");
        require(!hold.holds(1400 + SessionEndHold::WaitingMs), "a closing round gone quiet waits on the person");
    }

    // A window that opens to ask is the sign-out waiting, at once.
    {
        SessionEndHold hold;
        hold.noteSignOutProgram(true, 0);
        hold.noteState(false, 50);
        hold.noteState(true, 54);
        hold.noteWindowClosed(1400);
        require(hold.noteAsked(1450), "the first question is news");
        require(!hold.noteAsked(1900), "a later one is not");
        require(hold.holds(1450 + SessionEndHold::AskedMs - 1), "an answer given at once still ends behind the hold");
        require(!hold.holds(1450 + SessionEndHold::AskedMs), "a question left standing gives the session back");
    }

    // A program that never leaves is not believed for ever.
    {
        SessionEndHold hold;
        hold.noteSignOutProgram(true, 0);
        hold.noteState(false, 50);
        hold.noteState(true, 54);
        hold.noteWindowClosed(50 + SessionEndHold::ProgramLongestMs - 100);
        require(hold.holds(50 + SessionEndHold::ProgramLongestMs - 1), "windows still closing hold");
        require(!hold.holds(50 + SessionEndHold::ProgramLongestMs), "past its longest the program decides nothing");
    }

    // What happens outside an ending is no part of the next one.
    {
        SessionEndHold hold;
        hold.noteWindowClosed(10);
        require(!hold.noteAsked(20), "a window opening in a session that is not ending asks nothing of it");
        hold.noteSessionStopping(30);
        hold.noteSignOutProgram(true, 40);
        hold.noteSignOutProgram(false, 60);
        hold.noteState(false, 1000);
        hold.noteState(true, 1004);
        require(hold.goesOnAt(1004) == 1004 + SessionEndHold::GoesOnMs, "earlier marks do not move a later ending");
        require(!hold.holds(1004 + SessionEndHold::GoesOnMs), "which goes on by the states alone");
        // A second ending starts clean of the first one's marks.
        hold.noteSignOutProgram(true, 5000);
        hold.noteState(false, 5050);
        hold.noteSessionStopping(5060);
        hold.noteState(true, 5070);
        require(!hold.holds(5060 + SessionEndHold::StoppingLongestMs), "an ending given back is over");
        hold.noteState(false, 30000);
        hold.noteState(true, 30004);
        require(hold.goesOnAt(30004) == 30004 + SessionEndHold::WaitingMs, "and the next begins without its marks");
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
