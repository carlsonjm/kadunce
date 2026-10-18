// SPDX-License-Identifier: GPL-2.0-or-later
#include "KadunceKeys.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    const KeyContext desktop{};
    const KeyContext active{true, false, false};
    const KeyContext spread{true, true, false};
    const KeyContext table{true, false, true};
    // Meta+S, Meta+B and Meta+Esc are Kadunce's wherever it runs; Ctrl is left
    // to applications for Save, Bold and moving by word.
    for (const auto &context : {desktop, active, spread}) {
        check(keyActionFor(Qt::Key_S, Qt::MetaModifier, context) == KeyAction::Spread, "Meta+S did not open Spread");
        check(keyActionFor(Qt::Key_B, Qt::MetaModifier, context) == KeyAction::Bento, "Meta+B did not start Bento");
        check(keyActionFor(Qt::Key_B, Qt::MetaModifier | Qt::ShiftModifier, context) == KeyAction::ZoneMode,
              "Meta+Shift+B did not switch the monitors' zones");
        check(keyActionFor(Qt::Key_Escape, Qt::MetaModifier, context) == KeyAction::Release, "Meta+Esc did not let go");
        for (int key : {Qt::Key_S, Qt::Key_B, Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down, Qt::Key_Escape})
            check(keyActionFor(key, Qt::ControlModifier, context) == KeyAction::None, "A Ctrl key was taken from applications");
    }
    // The arrows are taken only while cards are shown.
    check(keyActionFor(Qt::Key_Left, Qt::MetaModifier, desktop) == KeyAction::None
              && keyActionFor(Qt::Key_Up, Qt::MetaModifier, desktop) == KeyAction::None,
          "Meta's arrows were taken with no cards shown");
    // On the Active card Left and Right go along the row; Up and Down are
    // taken so KDE does not tile the card, and step nothing: down lets a card
    // go, so no Meta+Down steps a Stack. Tab goes round it.
    check(keyActionFor(Qt::Key_Left, Qt::MetaModifier, active) == KeyAction::Previous
              && keyActionFor(Qt::Key_Right, Qt::MetaModifier, active) == KeyAction::Next
              && keyActionFor(Qt::Key_Up, Qt::MetaModifier, active) == KeyAction::Hold
              && keyActionFor(Qt::Key_Down, Qt::MetaModifier, active) == KeyAction::Hold,
          "Meta's arrows did not move through the cards");
    check(keyActionFor(Qt::Key_Tab, Qt::MetaModifier, active) == KeyAction::StackNext
              && keyActionFor(Qt::Key_Tab, Qt::MetaModifier | Qt::ShiftModifier, active) == KeyAction::StackPrevious
              && keyActionFor(Qt::Key_Backtab, Qt::MetaModifier | Qt::ShiftModifier, active) == KeyAction::StackPrevious
              && keyActionFor(Qt::Key_Backtab, Qt::MetaModifier, active) == KeyAction::StackPrevious
              && keyActionFor(Qt::Key_Tab, Qt::MetaModifier, spread) == KeyAction::StackNext,
          "Meta+Tab did not go round the Stack");
    check(keyActionFor(Qt::Key_Tab, Qt::MetaModifier, desktop) == KeyAction::None
              && keyActionFor(Qt::Key_Tab, Qt::MetaModifier | Qt::ShiftModifier, desktop) == KeyAction::None,
          "Meta+Tab was taken with no cards shown");
    // A pane: the arrows move the focus, with Shift the pane itself; Shift+Up
    // puts it deeper in a Stack and Shift+Down lets it go; Tab steps its
    // Stack and the brackets move a divider.
    const KeyContext pane{false, false, false, true};
    check(keyActionFor(Qt::Key_Left, Qt::MetaModifier, pane) == KeyAction::FocusLeft
              && keyActionFor(Qt::Key_Right, Qt::MetaModifier, pane) == KeyAction::FocusRight
              && keyActionFor(Qt::Key_Up, Qt::MetaModifier, pane) == KeyAction::FocusUp
              && keyActionFor(Qt::Key_Down, Qt::MetaModifier, pane) == KeyAction::FocusDown,
          "Meta's arrows did not move the focus between panes");
    check(keyActionFor(Qt::Key_Left, Qt::MetaModifier | Qt::ShiftModifier, pane) == KeyAction::CarryLeft
              && keyActionFor(Qt::Key_Right, Qt::MetaModifier | Qt::ShiftModifier, pane) == KeyAction::CarryRight
              && keyActionFor(Qt::Key_Up, Qt::MetaModifier | Qt::ShiftModifier, pane) == KeyAction::Deeper
              && keyActionFor(Qt::Key_Down, Qt::MetaModifier | Qt::ShiftModifier, pane) == KeyAction::LetGo,
          "Meta+Shift's arrows did not move the pane");
    check(keyActionFor(Qt::Key_Tab, Qt::MetaModifier, pane) == KeyAction::StackNext
              && keyActionFor(Qt::Key_BracketLeft, Qt::MetaModifier, pane) == KeyAction::DividerLeft
              && keyActionFor(Qt::Key_BracketRight, Qt::MetaModifier, pane) == KeyAction::DividerRight
              && keyActionFor(Qt::Key_Down, Qt::NoModifier, pane) == KeyAction::None,
          "A pane's Stack or divider key was not answered");
    // A pane wins over the cards of Spread's display, and Spread over a pane.
    const KeyContext paneBesideActive{true, false, false, true};
    const KeyContext spreadOverPane{true, true, false, true};
    check(keyActionFor(Qt::Key_Left, Qt::MetaModifier, paneBesideActive) == KeyAction::FocusLeft
              && keyActionFor(Qt::Key_Left, Qt::MetaModifier, spreadOverPane) == KeyAction::Previous
              && keyActionFor(Qt::Key_Left, Qt::MetaModifier | Qt::ShiftModifier, spreadOverPane) == KeyAction::None,
          "A pane and Spread disagreed over a key");
    check(keyActionFor(Qt::Key_Down, Qt::MetaModifier | Qt::ShiftModifier, spread) == KeyAction::LetGo
              && keyActionFor(Qt::Key_Down, Qt::MetaModifier | Qt::ShiftModifier, spreadOverPane) == KeyAction::LetGo
              && keyActionFor(Qt::Key_Up, Qt::MetaModifier | Qt::ShiftModifier, spread) == KeyAction::None,
          "Meta+Shift+Down did not let the centred card of Spread go");
    // The Active card alone on a Connected display moves as a pane does, and
    // its plain arrows still go along the row.
    const KeyContext lone{true, false, false, false, true};
    check(keyActionFor(Qt::Key_Left, Qt::MetaModifier | Qt::ShiftModifier, lone) == KeyAction::CarryLeft
              && keyActionFor(Qt::Key_Right, Qt::MetaModifier | Qt::ShiftModifier, lone) == KeyAction::CarryRight
              && keyActionFor(Qt::Key_Left, Qt::MetaModifier, lone) == KeyAction::Previous
              && keyActionFor(Qt::Key_Up, Qt::MetaModifier | Qt::ShiftModifier, lone) == KeyAction::None,
          "The lone card did not move by the keys");
    // The Active card in front of a Stack lets go of it by Shift+Down.
    const KeyContext stacked{true, false, false, false, false, true};
    check(keyActionFor(Qt::Key_Down, Qt::MetaModifier | Qt::ShiftModifier, stacked) == KeyAction::LetGo
              && keyActionFor(Qt::Key_Down, Qt::MetaModifier | Qt::ShiftModifier, lone) == KeyAction::None
              && keyActionFor(Qt::Key_Up, Qt::MetaModifier | Qt::ShiftModifier, stacked) == KeyAction::None,
          "Meta+Shift+Down did not let the Active card go from its Stack");
    // Off a pane, Meta+Shift's arrows and the brackets stay KDE's, but for
    // Shift+Down in Spread, which lets the centred card go from its Stack.
    for (const auto &context : {desktop, active, spread}) {
        for (int key : {Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down})
            if (key != Qt::Key_Down || !context.spreadShown)
                check(keyActionFor(key, Qt::MetaModifier | Qt::ShiftModifier, context) == KeyAction::None,
                      "Meta+Shift's arrows were taken off a pane");
        check(keyActionFor(Qt::Key_BracketLeft, Qt::MetaModifier, context) == KeyAction::None,
              "A bracket was taken off a pane");
    }
    // Plain keys only in Spread, and only with no other key held.
    check(keyActionFor(Qt::Key_Left, Qt::NoModifier, active) == KeyAction::None
              && keyActionFor(Qt::Key_Return, Qt::NoModifier, active) == KeyAction::None,
          "A plain key was taken from the Active card's application");
    check(keyActionFor(Qt::Key_Left, Qt::NoModifier, spread) == KeyAction::Previous
              && keyActionFor(Qt::Key_Return, Qt::NoModifier, spread) == KeyAction::Open
              && keyActionFor(Qt::Key_Enter, Qt::NoModifier, spread) == KeyAction::Open
              && keyActionFor(Qt::Key_Escape, Qt::NoModifier, spread) == KeyAction::Back,
          "Spread's plain keys did not move, open or go back");
    check(keyActionFor(Qt::Key_Left, Qt::ShiftModifier, spread) == KeyAction::None
              && keyActionFor(Qt::Key_A, Qt::NoModifier, spread) == KeyAction::None,
          "Spread took a key it does not answer");
    // Meta+W is Table's where the build carries it; Meta+G and Meta+E stay free
    // for Tettegouche, and Alt+Tab stays KDE's.
    check(keyActionFor(Qt::Key_W, Qt::MetaModifier, table) == KeyAction::Table
              && keyActionFor(Qt::Key_W, Qt::MetaModifier, active) == KeyAction::None,
          "Meta+W did not follow the build's Table");
    for (const auto &context : {desktop, active, spread, table}) {
        check(keyActionFor(Qt::Key_G, Qt::MetaModifier, context) == KeyAction::None
                  && keyActionFor(Qt::Key_E, Qt::MetaModifier, context) == KeyAction::None
                  && keyActionFor(Qt::Key_Tab, Qt::AltModifier, context) == KeyAction::None,
              "A key left to Tettegouche or KDE was taken");
    }
    // Keyboard lock state never changes what a key means.
    check(keyActionFor(Qt::Key_S, Qt::MetaModifier | Qt::KeypadModifier, desktop) == KeyAction::Spread,
          "A keypad state hid Meta+S");
    // Held down, only moving repeats.
    check(keyActionRepeats(KeyAction::Next) && keyActionRepeats(KeyAction::StackPrevious)
              && keyActionRepeats(KeyAction::FocusLeft) && keyActionRepeats(KeyAction::Deeper)
              && !keyActionRepeats(KeyAction::CarryLeft) && !keyActionRepeats(KeyAction::LetGo)
              && !keyActionRepeats(KeyAction::Spread) && !keyActionRepeats(KeyAction::Release),
          "A held key repeated what should happen once");
    std::cout << "Kadunce's keys sit on Meta and in Spread, and leave Ctrl, Alt+Tab and Tettegouche's keys alone\n";
}
