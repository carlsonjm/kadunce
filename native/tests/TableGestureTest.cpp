// SPDX-License-Identifier: GPL-2.0-or-later
#include "TableGesture.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
using Kind = TableAction::Kind;

namespace {
void check(bool ok, const char *message)
{
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
// Three workspaces across a 1000-wide display, the + tab after them, and each
// workspace's cards hanging below its tab.
TableGesture opened(int current = 0, bool sticky = false)
{
    TableGesture table;
    table.open(current, 0, sticky);
    table.setLayout({100, 300, 500}, 650, {{80, 240}, {280, 440, 600}, {500}});
    return table;
}
// A stroke from the top edge: y is a fraction of the display's height.
void stroke(TableGesture &table, double x, double y, qint64)
{
    table.move(QPointF(x, y * 1000), y);
}
}

int main()
{
    {
        TableGesture table = opened();
        check(table.isOpen() && table.scrubbing() && table.shownWorkspace() == 0, "opens on the current workspace");
        stroke(table, 110, 0.12, 50);
        stroke(table, 310, 0.14, 120);
        check(table.hovered() == 1 && table.shownWorkspace() == 1, "the finger across the tabs previews each");
        check(table.trayWorkspace() == 1 && table.card() == -1,
              "the scrubbed tab's cards hang, none of them chosen");
        stroke(table, 490, 0.15, 400);
        const auto action = table.release(QPointF(490, 150), 420);
        check(action.kind == Kind::Enter && action.workspace == 2 && !table.isOpen(), "lifting on a tab enters it");
    }
    {
        TableGesture table = opened(1);
        stroke(table, 300, 0.04, 40);
        const auto action = table.release(QPointF(300, 40), 90);
        check(action.kind == Kind::StayOpen && table.sticky() && table.hovered() == 1, "a flick leaves the tabs as a menu bar");
        check(table.trayWorkspace() == 1, "the menu bar hangs the hovered workspace's cards");
    }
    {
        TableGesture table = opened();
        stroke(table, 320, 0.12, 80);
        check(!table.cancelling(), "not cancelling on the way down");
        stroke(table, 320, 0.03, 400);
        check(table.cancelling() && table.shownWorkspace() == 0, "back at the edge shows where the person is");
        const auto action = table.release(QPointF(320, 30), 420);
        check(action.kind == Kind::Close && !table.isOpen(), "lifting at the edge cancels");
    }
    {
        TableGesture table = opened();
        stroke(table, 300, 0.12, 50);
        stroke(table, 300, 0.40, 120);
        check(table.level() == TableLevel::Cards && table.locked() == 1, "past the depth line the tab under the finger locks");
        check(table.trayWorkspace() == 1 && table.card() == 0, "its cards hang and the nearest is under the finger");
        stroke(table, 450, 0.42, 200);
        check(table.card() == 1 && table.shownWorkspace() == 1 && table.shownCard() == 1, "sliding across previews each card");
        stroke(table, 110, 0.20, 260);
        check(table.level() == TableLevel::Tabs && table.card() == -1 && table.hovered() == 0,
              "pushing back above the line returns to the tabs");
        stroke(table, 300, 0.12, 300);
        stroke(table, 300, 0.40, 320);
        stroke(table, 590, 0.40, 340);
        const auto action = table.release(QPointF(590, 400), 380);
        check(action.kind == Kind::Activate && action.workspace == 1 && action.card == 2, "lifting on a card opens it");
    }
    {
        TableGesture table = opened();
        stroke(table, 300, 0.12, 50);
        stroke(table, 280, 0.40, 100);
        stroke(table, 282, 0.41, 2000);
        check(!table.carrying() && table.card() == 0, "resting on a card to look at it never lifts it");
        stroke(table, 280, 0.46, 2100);
        check(table.carrying() && table.carryCard() == 0 && table.carryFrom() == 1,
              "pulled past the line under the cards, the card under the finger lifts");
        stroke(table, 510, 0.10, 950);
        check(table.over() == TableGesture::Over::Workspace && table.overWorkspace() == 2, "carried to a tab, the tab is the target");
        const auto action = table.release(QPointF(510, 100), 1000);
        check(action.kind == Kind::Move && action.workspace == 1 && action.card == 0 && action.destination == 2,
              "released on a tab, the card moves there");
        table.showAfterMove(2, 1);
        check(table.isOpen() && table.sticky() && table.trayWorkspace() == 2 && table.card() == 1,
              "Table stays open on the workspace the card went to");
    }
    {
        TableGesture table = opened();
        stroke(table, 300, 0.12, 50);
        stroke(table, 280, 0.40, 100);
        stroke(table, 280, 0.50, 700);
        check(table.carrying(), "lifts");
        stroke(table, 660, 0.10, 750);
        check(table.over() == TableGesture::Over::Plus, "carried to +, + is the target");
        const auto action = table.release(QPointF(660, 100), 800);
        check(action.kind == Kind::Create && action.workspace == 1 && action.card == 0, "released on +, a workspace is made for it");
    }
    {
        TableGesture table = opened();
        stroke(table, 300, 0.12, 50);
        stroke(table, 280, 0.40, 100);
        stroke(table, 280, 0.50, 700);
        check(table.carrying(), "lifts");
        stroke(table, 300, 0.02, 750);
        check(table.cancelling() && table.over() == TableGesture::Over::Nothing, "carried back to the edge targets nothing");
        const auto action = table.release(QPointF(300, 20), 800);
        check(action.kind == Kind::StayOpen && table.sticky() && !table.cancelling(), "a cancelled carry keeps the menu bar");
    }
    {
        TableGesture table = opened();
        stroke(table, 300, 0.12, 50);
        stroke(table, 440, 0.40, 100);
        stroke(table, 440, 0.50, 700);
        check(table.carrying(), "lifts");
        stroke(table, 450, 0.42, 750);
        const auto action = table.release(QPointF(450, 450), 800);
        check(action.kind == Kind::Activate && action.workspace == 1 && action.card == 1, "released on its own place, it opens");
    }
    {
        TableGesture table = opened(0, true);
        check(table.sticky() && table.trayWorkspace() == 0, "a menu bar shows the current workspace's cards");
        table.press(QPointF(300, 20), -1);
        auto action = table.release(QPointF(302, 22), 50, {.tab = 1, .insideRows = true});
        check(action.kind == Kind::StayOpen && table.hovered() == 1 && table.shownWorkspace() == 1, "a tap previews a tab");
        table.press(QPointF(300, 20), -1);
        action = table.release(QPointF(300, 20), 150, {.tab = 1, .insideRows = true});
        check(action.kind == Kind::Enter && action.workspace == 1 && !table.isOpen(), "a second tap enters it");
    }
    {
        // Held still on a menu bar's tab, the finger renames it, and letting
        // go afterwards enters nothing.
        TableGesture table = opened(0, true);
        table.press(QPointF(300, 20), -1, 1, 1000);
        check(table.hold(1000 + table.config().holdMs - 1).kind == Kind::None, "a press is not yet a hold");
        check(table.holdLeft(1000 + table.config().holdMs - 20) == 20, "a timer woken early waits out the rest");
        auto action = table.hold(1000 + table.config().holdMs);
        check(action.kind == Kind::Rename && action.workspace == 1 && table.hovered() == 1 && table.isOpen(),
              "a tab held still in a menu bar is renamed");
        check(table.hold(1000 + 2 * table.config().holdMs).kind == Kind::None, "a hold renames once");
        table.move(QPointF(420, 30), 0.03);
        action = table.release(QPointF(420, 30), 1800, {.tab = 2, .insideRows = true});
        check(action.kind == Kind::StayOpen && table.isOpen() && table.hovered() == 1, "letting go after a hold enters nothing");
    }
    {
        TableGesture table = opened(0, true);
        table.press(QPointF(300, 20), -1, 1, 0);
        table.move(QPointF(330, 20), 0.02);
        check(table.hold(table.config().holdMs * 2).kind == Kind::None, "a finger that moved off its press is not a hold");
    }
    {
        // A scrub rests on tabs to look at them: never a rename, however long.
        TableGesture table = opened();
        stroke(table, 310, 0.14, 100);
        check(table.hold(100000).kind == Kind::None && table.scrubbing(), "a scrub resting on a tab never renames it");
    }
    {
        TableGesture table = opened(0, true);
        auto action = table.contextOn(2);
        check(action.kind == Kind::Rename && action.workspace == 2 && table.hovered() == 2, "a right-click renames a tab");
        table.key(TableGesture::Key::Left);
        action = table.key(TableGesture::Key::Rename);
        check(action.kind == Kind::Rename && action.workspace == 1, "F2 renames the tab in view");
    }
    {
        TableGesture table = opened(0, true);
        table.press(QPointF(300, 20), -1);
        auto action = table.release(QPointF(360, 20), 90, {.tab = 1, .insideRows = true});
        check(action.kind == Kind::StayOpen && table.hovered() == 0, "a moved finger is not a tap");
        table.press(QPointF(240, 400), 1);
        action = table.release(QPointF(240, 400), 150, {.card = 1, .insideRows = true});
        check(action.kind == Kind::Activate && action.workspace == 0 && action.card == 1, "a tap on a hanging card opens it");
    }
    {
        TableGesture table = opened(0, true);
        table.press(QPointF(300, 20), -1);
        (void)table.release(QPointF(300, 20), 30, {.tab = 2, .insideRows = true});
        table.press(QPointF(900, 900), -1);
        const auto action = table.release(QPointF(900, 900), 80, {});
        check(action.kind == Kind::Enter && action.workspace == 2 && !table.isOpen(),
              "a tap on the preview enters the workspace it shows");
    }
    {
        TableGesture table = opened(0, true);
        table.hover(-1, 1);
        check(table.level() == TableLevel::Cards && table.shownCard() == 1, "a pointer over a hanging card previews it");
        table.press(QPointF(900, 900), -1);
        const auto action = table.release(QPointF(900, 900), 30, {});
        check(action.kind == Kind::Activate && action.workspace == 0 && action.card == 1,
              "a tap on a card's preview makes it Active");
    }
    {
        TableGesture table = opened(1, true);
        table.setThresholds(0.12, 0.04, 0.3);
        table.press(QPointF(500, 500), -1);
        table.move(QPointF(500, 300), 0.3);
        table.move(QPointF(500, 20), 0.02);
        const auto action = table.release(QPointF(500, 20), 60, {});
        check(action.kind == Kind::Close && !table.isOpen(), "a menu bar pushed back up to the edge closes");
    }
    {
        TableGesture table = opened(0);
        table.setThresholds(0.12, 0.04, 0.3);
        stroke(table, 300, 0.08, 50);
        check(table.level() == TableLevel::Tabs && table.hovered() == 1, "above the depth line the tabs are scrubbed");
        stroke(table, 300, 0.14, 80);
        check(table.level() == TableLevel::Cards && table.locked() == 1, "the depth line can sit just under the tabs");
        stroke(table, 300, 0.03, 120);
        check(table.cancelling(), "above the tabs a lift cancels");
    }
    {
        TableGesture table = opened(0, true);
        table.hover(2, -1);
        check(table.hovered() == 2 && table.shownWorkspace() == 2, "a pointer over a tab previews it");
        table.step(-1);
        check(table.hovered() == 1, "the wheel steps back a tab");
        table.step(-5);
        check(table.hovered() == 0, "and stops at the first");
        table.step(9);
        check(table.onPlus() && table.shownWorkspace() == 0 && table.trayWorkspace() == -1,
              "and at +, after the last, which shows where the person is and hangs nothing");
        table.step(-1);
        check(!table.onPlus() && table.hovered() == 2, "back from + is the last tab");
        table.hover(-1, -1, true);
        check(table.onPlus(), "a pointer over + rests on it");
    }
    {
        TableGesture table = opened(0, true);
        table.setLayout({100, 300, 500}, std::numeric_limits<double>::quiet_NaN(), {{80, 240}, {280, 440, 600}, {500}});
        table.step(9);
        check(!table.onPlus() && table.hovered() == 2, "with no + the wheel stops at the last tab");
        stroke(table, 660, 0.1, 50);
        check(!table.onPlus(), "and nothing reaches a + that is not there");
    }
    {
        TableGesture table = opened();
        stroke(table, 310, 0.10, 50);
        stroke(table, 640, 0.12, 120);
        check(table.onPlus() && table.shownWorkspace() == 0 && table.trayWorkspace() == -1,
              "the finger slides on to +, showing where the person is");
        stroke(table, 640, 0.40, 300);
        check(table.onPlus() && table.level() == TableLevel::Tabs && !table.carrying(), "nothing hangs under + to pull into");
        const auto action = table.release(QPointF(640, 400), 400);
        check(action.kind == Kind::New && !table.isOpen(), "lifting on + makes an empty workspace");
    }
    {
        TableGesture table = opened(0, true);
        table.press(QPointF(650, 60), -1);
        TableHit hit;
        hit.plus = true;
        hit.insideRows = true;
        const auto action = table.release(QPointF(650, 60), 100, hit);
        check(action.kind == Kind::New && !table.isOpen(), "one tap on + makes an empty workspace");
    }
    {
        TableGesture table = opened(1);
        auto action = table.key(TableGesture::Key::Right);
        check(action.kind == Kind::StayOpen && table.sticky() && table.hovered() == 2, "right chooses the next workspace");
        table.key(TableGesture::Key::Left);
        action = table.key(TableGesture::Key::Down);
        check(table.level() == TableLevel::Cards && table.locked() == 1 && table.card() == 0, "down hangs its cards");
        table.key(TableGesture::Key::Right);
        table.step(1);
        check(table.card() == 2, "right and the wheel step across the cards");
        table.key(TableGesture::Key::Up);
        check(table.level() == TableLevel::Tabs && table.hovered() == 1, "up returns to the tabs");
        table.key(TableGesture::Key::Down);
        action = table.key(TableGesture::Key::Enter);
        check(action.kind == Kind::Activate && action.workspace == 1 && action.card == 0, "enter opens the chosen card");
        table.open(0, 0);
        table.setLayout({100, 300, 500}, 650, {{80, 240}, {280, 440, 600}, {500}});
        table.key(TableGesture::Key::Right);
        action = table.key(TableGesture::Key::Enter);
        check(action.kind == Kind::Enter && action.workspace == 1, "enter on a tab enters it");
        table.open(0, 0);
        table.setLayout({100, 300, 500}, 650, {{80, 240}, {280, 440, 600}, {500}});
        for (int i = 0; i < 3; ++i) table.key(TableGesture::Key::Right);
        check(table.onPlus(), "right past the last tab reaches +");
        table.key(TableGesture::Key::Down);
        check(table.level() == TableLevel::Tabs && table.onPlus(), "down on + hangs nothing");
        action = table.key(TableGesture::Key::Enter);
        check(action.kind == Kind::New, "enter on + makes an empty workspace");
        table.open(0, 0);
        action = table.key(TableGesture::Key::Escape);
        check(action.kind == Kind::Close && !table.isOpen(), "escape cancels");
    }
    {
        TableGesture table = opened(0, true);
        table.press(QPointF(240, 400), 1);
        table.move(QPointF(244, 404), 0.404);
        check(!table.carrying() && table.card() == 1, "a menu bar's card pressed and held still stays chosen");
        stroke(table, 250, 0.35, 600);
        check(table.carrying() && table.carryFrom() == 0 && table.carryCard() == 1, "dragged, it lifts");
        stroke(table, 300, 0.10, 650);
        const auto action = table.release(QPointF(300, 100), 700);
        check(action.kind == Kind::Move && action.destination == 1, "and carrying it to a tab moves it");
    }
    {
        // The tablet's own lines, 915 high: tabs from 36.6, cards from 95.6,
        // the lift line at 181.6. Flicks the hand makes are 12 to 60 mm long.
        for (const double travel : {60.0, 120.0, 200.0, 300.0}) {
            TableGesture table = opened(0);
            table.setThresholds(95.6 / 915, 36.6 / 915, 181.6 / 915);
            for (int step = 1; step <= 3; ++step) {
                const double y = 24 + (travel - 24) * step / 3.0;
                table.move(QPointF(90, y), y / 915);
            }
            const auto action = table.release(QPointF(90, travel), 150);
            check(action.kind == Kind::StayOpen && table.isOpen() && table.sticky() && !table.carrying()
                      && table.level() == TableLevel::Tabs && table.hovered() == 0,
                  "a flick leaves the tabs open however far it goes, and chooses and moves nothing");
        }
    }
    {
        TableGesture table = opened();
        stroke(table, 300, 0.12, 50);
        stroke(table, 280, 0.40, 100);
        stroke(table, 280, 0.50, 400);
        check(table.carrying(), "lifts");
        table.refuseCarry();
        stroke(table, 282, 0.52, 450);
        check(!table.carrying() && table.card() == 0, "a card that cannot be carried does not lift again");
        stroke(table, 440, 0.52, 500);
        check(table.carrying() && table.carryCard() == 1, "the next card still lifts");
    }
    std::cout << "Table gesture checks passed\n";
    return 0;
}
