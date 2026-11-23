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
// The same, workspace 1's middle card a Stack of three whose cards hang at
// 360, 440 and 520 once it is rested on, and workspace 2's card a Stack of two.
TableGesture withStacks(int current = 0, bool sticky = false)
{
    TableGesture table;
    table.open(current, 0, sticky);
    table.setLayout({100, 300, 500}, 650, {{80, 240}, {280, 440, 600}, {500}},
                    {{{}, {}}, {{}, {360, 440, 520}, {}}, {{460, 540}}});
    table.setThresholds(0.30, 0.05, 0.45, 0.65);
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
        check(action.kind == Kind::New && table.isOpen() && table.sticky() && !table.onPlus(),
              "lifting on + makes an empty workspace, and Table stays open");
        table.setLayout({100, 300, 500, 640}, 760, {{80, 240}, {280, 440, 600}, {500}, {}});
        table.showNewTab(3);
        check(table.hovered() == 3 && table.level() == TableLevel::Tabs && table.card() == -1,
              "the new workspace's tab is in view");
        table.press(QPointF(640, 60), -1);
        TableHit tab;
        tab.tab = 3;
        tab.insideRows = true;
        const auto entered = table.release(QPointF(640, 60), 600, tab);
        check(entered.kind == Kind::Enter && entered.workspace == 3 && !table.isOpen(), "a tap on the new tab enters it");
    }
    {
        TableGesture table = opened(0, true);
        table.press(QPointF(650, 60), -1);
        TableHit hit;
        hit.plus = true;
        hit.insideRows = true;
        const auto action = table.release(QPointF(650, 60), 100, hit);
        check(action.kind == Kind::New && table.isOpen(), "one tap on + makes an empty workspace, Table open");
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
        check(action.kind == Kind::New && table.isOpen() && !table.onPlus(), "enter on + makes an empty workspace");
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
    {
        using Key = TableGesture::Key;
        TableGesture table = opened(1);
        table.key(Key::Down);
        table.key(Key::Right);
        auto action = table.key(Key::CarryRight);
        check(action.kind == Kind::StayOpen && table.keyCarrying() && table.carryFrom() == 1 && table.carryCard() == 1
                  && table.over() == TableGesture::Over::Workspace && table.overWorkspace() == 2,
              "shift's right lifts the chosen card and carries it to the next tab");
        check(table.level() == TableLevel::Tabs && table.shownWorkspace() == 2 && table.trayWorkspace() == 2,
              "the displays show the workspace the card is over, its cards hanging");
        table.key(Key::Left);
        table.key(Key::Enter);
        check(table.isOpen() && table.keyCarrying() && table.overWorkspace() == 2, "plain keys wait while the keys carry");
        action = table.key(Key::Drop);
        check(action.kind == Kind::Move && action.workspace == 1 && action.card == 1 && action.destination == 2
                  && table.isOpen() && !table.carrying(),
              "letting go of shift moves it there, and Table stays open");
        table.showAfterMove(2, 1);
        table.key(Key::CarryRight);
        check(table.over() == TableGesture::Over::Plus && !table.onPlus(), "past the last tab the card is over +");
        table.key(Key::CarryRight);
        check(table.over() == TableGesture::Over::Plus, "and stays there");
        action = table.key(Key::Drop);
        check(action.kind == Kind::Create && action.workspace == 2 && action.card == 1, "let go on +, a workspace is made for it");
    }
    {
        using Key = TableGesture::Key;
        TableGesture table = opened(0);
        table.key(Key::Down);
        table.key(Key::CarryRight);
        table.key(Key::CarryLeft);
        auto action = table.key(Key::Drop);
        check(action.kind == Kind::StayOpen && table.isOpen() && !table.carrying() && table.level() == TableLevel::Cards
                  && table.locked() == 0 && table.card() == 0,
              "let go on its own tab, the card goes back still chosen");
        table.key(Key::CarryLeft);
        check(table.keyCarrying() && table.overWorkspace() == 0, "carried left past the first tab, it stays on it");
        table.key(Key::CarryRight);
        action = table.key(Key::Escape);
        check(action.kind == Kind::StayOpen && table.isOpen() && !table.carrying() && table.locked() == 0 && table.card() == 0,
              "escape puts the carried card back, chosen");
        action = table.key(Key::Drop);
        check(action.kind == Kind::StayOpen && !table.carrying(), "letting go of shift with nothing carried does nothing");
        table.key(Key::Up);
        table.key(Key::CarryRight);
        check(!table.carrying() && table.hovered() == 1, "on the tabs shift's arrows only choose");
        table.key(Key::Down);
        table.key(Key::CarryRight);
        table.press(QPointF(300, 60), -1, 1);
        check(!table.carrying() && table.locked() == 1, "a finger on Table puts back what the keys carry");
    }
    {
        // + with saved Layouts: the empty workspace and two Layouts hang under it.
        TableGesture table = opened(0, true);
        table.setPlusCards({560, 700, 840});
        table.press(QPointF(650, 60), -1);
        TableHit plus;
        plus.plus = true;
        plus.insideRows = true;
        auto action = table.release(QPointF(650, 60), 100, plus);
        check(action.kind == Kind::StayOpen && table.isOpen() && table.plusTray() && table.plusCard() == -1
                  && table.shownWorkspace() == 0 && table.trayWorkspace() == -1,
              "a tap on + with Layouts hangs its cards and makes nothing");
        table.hover(-1, -1, false, 2);
        check(table.plusCard() == 2, "the pointer on a Layout card chooses it");
        table.press(QPointF(700, 200), -1, -1, 200, 1);
        check(table.plusCard() == 1 && !table.carrying(), "a touch on a Layout card chooses it, lifting nothing");
        table.move(QPointF(760, 260), 0.26);
        check(!table.carrying(), "a card under + is never carried");
        TableHit layout;
        layout.plusCard = 1;
        layout.insideRows = true;
        table.press(QPointF(700, 200), -1, -1, 300, 1);
        action = table.release(QPointF(700, 200), 320, layout);
        check(action.kind == Kind::OpenLayout && action.card == 0 && !table.isOpen(),
              "a tap on the first Layout opens it, and Table closes");
    }
    {
        TableGesture table = opened(0, true);
        table.setPlusCards({560, 700});
        TableHit plus;
        plus.plus = true;
        plus.insideRows = true;
        table.press(QPointF(650, 60), -1);
        (void)table.release(QPointF(650, 60), 100, plus);
        TableHit empty;
        empty.plusCard = 0;
        empty.insideRows = true;
        table.press(QPointF(560, 200), -1, -1, 200, 0);
        const auto action = table.release(QPointF(560, 200), 220, empty);
        check(action.kind == Kind::New && table.isOpen() && !table.onPlus(),
              "Empty workspace makes the workspace + always made, Table open");
    }
    {
        TableGesture table = opened(0, true);
        table.setPlusCards({560, 700});
        TableHit plus;
        plus.plus = true;
        plus.insideRows = true;
        table.press(QPointF(650, 60), -1);
        (void)table.release(QPointF(650, 60), 100, plus);
        auto action = table.key(TableGesture::Key::Escape);
        check(action.kind == Kind::Close && !table.isOpen(), "Escape from + makes nothing");
        table = opened(0, true);
        table.setPlusCards({560, 700});
        table.press(QPointF(650, 60), -1);
        (void)table.release(QPointF(650, 60), 100, plus);
        table.press(QPointF(400, 800), -1);
        action = table.release(QPointF(400, 800), 200, {});
        check(action.kind == Kind::Enter && action.workspace == 0 && !table.isOpen(),
              "a tap outside + returns to where the person was, making nothing");
        table = opened(0, true);
        table.setPlusCards({560, 700});
        table.press(QPointF(650, 60), -1);
        (void)table.release(QPointF(650, 60), 100, plus);
        TableHit tab;
        tab.tab = 1;
        tab.insideRows = true;
        table.press(QPointF(300, 60), -1);
        action = table.release(QPointF(300, 60), 200, tab);
        check(action.kind == Kind::StayOpen && !table.plusTray() && table.hovered() == 1,
              "a tap on another tab puts the cards under + away");
    }
    {
        // A finger pulled on to + and down to its cards opens the one it lifts on.
        TableGesture table = opened();
        table.setPlusCards({560, 700, 840});
        stroke(table, 310, 0.10, 50);
        stroke(table, 640, 0.12, 120);
        check(table.plusTray() && table.plusCard() == -1, "the finger on + hangs its cards");
        stroke(table, 830, 0.40, 300);
        check(table.plusCard() == 2 && !table.carrying(), "down among them it chooses the nearest");
        stroke(table, 830, 0.60, 350);
        check(!table.carrying(), "past the lift line a card under + still does not lift");
        auto action = table.release(QPointF(830, 600), 400);
        check(action.kind == Kind::OpenLayout && action.card == 1, "lifting on a Layout card opens it");
        table = opened();
        table.setPlusCards({560, 700});
        stroke(table, 310, 0.10, 50);
        stroke(table, 640, 0.12, 120);
        action = table.release(QPointF(640, 120), 400);
        check(action.kind == Kind::StayOpen && table.isOpen() && table.sticky() && table.plusTray(),
              "lifting on + with Layouts leaves its cards hanging, nothing made");
    }
    {
        TableGesture table = opened();
        table.setPlusCards({560, 700});
        for (int i = 0; i < 3; ++i) table.key(TableGesture::Key::Right);
        check(table.plusTray() && table.plusCard() == -1, "the keys on + hang its cards");
        table.key(TableGesture::Key::Down);
        check(table.plusCard() == 0, "down chooses Empty workspace");
        table.key(TableGesture::Key::Right);
        check(table.plusCard() == 1, "right steps to the Layout");
        auto action = table.key(TableGesture::Key::Enter);
        check(action.kind == Kind::OpenLayout && action.card == 0, "enter opens the chosen Layout");
    }
    {
        // A card carried on to + still makes a workspace for it, Layouts or not.
        TableGesture table = opened();
        table.setPlusCards({560, 700});
        stroke(table, 300, 0.12, 50);
        stroke(table, 440, 0.40, 300);
        stroke(table, 440, 0.60, 350);
        check(table.carrying() && !table.plusTray(), "a lifted card is carried, nothing hangs under +");
        stroke(table, 640, 0.10, 400);
        check(table.over() == TableGesture::Over::Plus && !table.plusTray(), "carried on to +");
        const auto action = table.release(QPointF(640, 100), 500);
        check(action.kind == Kind::Create && action.workspace == 1 && action.card == 1,
              "let go on +, it makes a workspace for the card");
    }
    {
        // A Stack rested on hangs its cards; the finger goes down among them
        // and lifting on one opens that card.
        TableGesture table = withStacks();
        stroke(table, 300, 0.12, 50);
        stroke(table, 440, 0.38, 200);
        check(table.card() == 1 && table.wantsRest() && table.membersOf() == -1, "a Stack chosen waits to hang its cards");
        check(table.hangMembers() && table.membersOf() == 1 && !table.wantsRest(), "rested on, its cards hang");
        stroke(table, 440, 0.55, 300);
        check(table.level() == TableLevel::Members && table.member() == 1 && !table.carrying(),
              "past the line under the cards the finger is among the Stack's cards, nothing lifted");
        stroke(table, 525, 0.56, 350);
        check(table.member() == 2 && table.shownMember() == 2 && table.shownCard() == 1, "each is shown as it is passed");
        const auto action = table.release(QPointF(525, 560), 500);
        check(action.kind == Kind::Activate && action.workspace == 1 && action.card == 1 && action.member == 2,
              "lifting on one opens that card of the Stack");
    }
    {
        // Back up into the cards, or on to another card, the Stack folds.
        TableGesture table = withStacks();
        stroke(table, 300, 0.12, 50);
        stroke(table, 440, 0.38, 200);
        table.hangMembers();
        stroke(table, 440, 0.55, 300);
        stroke(table, 440, 0.38, 350);
        check(table.level() == TableLevel::Cards && table.membersOf() == 1 && table.member() == -1,
              "back among the cards the Stack is chosen again, its cards still hanging");
        stroke(table, 600, 0.38, 400);
        check(table.card() == 2 && table.membersOf() == -1, "on another card the Stack's cards fold");
    }
    {
        // A Stack pulled past the line before it hangs lifts whole, as before.
        TableGesture table = withStacks();
        stroke(table, 300, 0.12, 50);
        stroke(table, 440, 0.38, 200);
        stroke(table, 440, 0.50, 250);
        check(table.carrying() && table.carryCard() == 1 && table.carryMember() == -1, "a quick pull lifts the whole Stack");
    }
    {
        // A hanging card pulled past its own line lifts out of the Stack; let go
        // in its workspace's row it stands on its own.
        TableGesture table = withStacks();
        stroke(table, 300, 0.12, 50);
        stroke(table, 440, 0.38, 200);
        table.hangMembers();
        stroke(table, 520, 0.55, 300);
        stroke(table, 520, 0.70, 350);
        check(table.carrying() && table.carryCard() == 1 && table.carryMember() == 2, "the hanging card lifts out");
        stroke(table, 560, 0.50, 400);
        check(table.trayWorkspace() == 1, "its own workspace's row hangs");
        const auto action = table.release(QPointF(560, 500), 600);
        check(action.kind == Kind::TakeOut && action.workspace == 1 && action.card == 1 && action.member == 2
                  && table.isOpen() && table.membersOf() == -1,
              "let go in its row, it leaves the Stack and Table stays open");
    }
    {
        // A card of a Stack carried to another tab moves there alone.
        TableGesture table = withStacks();
        stroke(table, 300, 0.12, 50);
        stroke(table, 440, 0.38, 200);
        table.hangMembers();
        stroke(table, 360, 0.55, 300);
        stroke(table, 360, 0.70, 350);
        stroke(table, 110, 0.12, 400);
        const auto action = table.release(QPointF(110, 120), 600);
        check(action.kind == Kind::Move && action.workspace == 1 && action.card == 1 && action.member == 0
                  && action.destination == 0, "on another tab it moves there, out of its Stack");
    }
    {
        // A card carried over another tab, then down among that workspace's
        // cards, hangs that row; held over a card there, let go, it stacks.
        TableGesture table = withStacks();
        stroke(table, 300, 0.12, 50);
        stroke(table, 280, 0.38, 200);
        stroke(table, 280, 0.50, 250);
        check(table.carrying() && table.carryCard() == 0, "lifts");
        stroke(table, 500, 0.12, 300);
        stroke(table, 500, 0.38, 350);
        check(table.trayWorkspace() == 2 && table.overWorkspace() == 2, "down among the cards of the tab it passed");
        check(table.aimAt(0) && table.aim() == 0 && !table.aimArmed(), "over a card it aims at it");
        table.armAim();
        check(table.aimArmed(), "held long enough it would stack");
        const auto action = table.release(QPointF(500, 380), 700);
        check(action.kind == Kind::Stack && action.workspace == 1 && action.card == 0 && action.destination == 2
                  && action.target == 0 && action.member == -1 && table.isOpen(),
              "let go, it stacks there and Table stays open");
    }
    {
        // Let go there unarmed, it moves to that workspace; a new aim disarms.
        TableGesture table = withStacks();
        stroke(table, 300, 0.12, 50);
        stroke(table, 280, 0.38, 200);
        stroke(table, 280, 0.50, 250);
        stroke(table, 500, 0.12, 300);
        stroke(table, 500, 0.38, 350);
        table.aimAt(0);
        table.armAim();
        table.aimAt(-1);
        check(!table.aimArmed(), "moved off the card, nothing is armed");
        const auto action = table.release(QPointF(500, 380), 700);
        check(action.kind == Kind::Move && action.destination == 2, "let go in another row, it moves there");
    }
    {
        // Back in its own row without passing a tab, it opens as before.
        TableGesture table = withStacks();
        stroke(table, 300, 0.12, 50);
        stroke(table, 280, 0.38, 200);
        stroke(table, 280, 0.50, 250);
        stroke(table, 300, 0.40, 300);
        const auto action = table.release(QPointF(300, 400), 600);
        check(action.kind == Kind::Activate && action.workspace == 1 && action.card == 0, "let go in its own place, it opens");
    }
    {
        // The keys: down on a Stack hangs its cards, the arrows choose, Enter
        // opens; Meta+Shift+Down takes one out and Table stays open.
        TableGesture table = withStacks(1);
        table.key(TableGesture::Key::Down);
        table.key(TableGesture::Key::Right);
        check(table.level() == TableLevel::Cards && table.card() == 1, "the keys choose the Stack");
        table.key(TableGesture::Key::Down);
        check(table.level() == TableLevel::Members && table.membersOf() == 1 && table.member() == 0,
              "down on a Stack hangs its cards, the front one chosen");
        table.key(TableGesture::Key::Right);
        check(table.member() == 1, "right steps along them");
        auto action = table.key(TableGesture::Key::TakeOut);
        check(action.kind == Kind::TakeOut && action.card == 1 && action.member == 1 && table.isOpen()
                  && table.level() == TableLevel::Cards, "Meta+Shift+Down takes it out");
        table.key(TableGesture::Key::Down);
        table.key(TableGesture::Key::Up);
        check(table.level() == TableLevel::Cards && table.member() == -1, "up goes back to the cards");
        table.key(TableGesture::Key::Down);
        table.key(TableGesture::Key::Right);
        table.key(TableGesture::Key::Right);
        action = table.key(TableGesture::Key::Enter);
        check(action.kind == Kind::Activate && action.card == 1 && action.member == 2, "enter opens the chosen card");
    }
    {
        // A menu bar: the pointer resting on a Stack hangs its cards; a click
        // on one opens it, and one dragged lifts out.
        TableGesture table = withStacks(1, true);
        table.hover(-1, 1);
        check(table.wantsRest(), "the pointer resting on a Stack waits");
        table.hangMembers();
        table.hover(-1, -1, false, -1, 0);
        check(table.level() == TableLevel::Members && table.member() == 0, "the pointer over a hanging card shows it");
        TableHit hit;
        hit.member = 0;
        table.press(QPointF(360, 600), -1, -1, 100, -1, 0);
        auto action = table.release(QPointF(361, 600), 200, hit);
        check(action.kind == Kind::Activate && action.card == 1 && action.member == 0, "a click opens it");
        table = withStacks(1, true);
        table.hover(-1, 1);
        table.hangMembers();
        table.press(QPointF(440, 600), -1, -1, 100, -1, 1);
        table.move(QPointF(470, 640), 0.64);
        check(table.carrying() && table.carryMember() == 1, "dragged, it lifts out of its Stack");
        table.hover(-1, -1);
    }
    std::cout << "Table gesture checks passed\n";
    return 0;
}
