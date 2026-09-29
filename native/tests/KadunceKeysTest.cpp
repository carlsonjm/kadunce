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
        check(keyActionFor(Qt::Key_Escape, Qt::MetaModifier, context) == KeyAction::Release, "Meta+Esc did not let go");
        for (int key : {Qt::Key_S, Qt::Key_B, Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down, Qt::Key_Escape})
            check(keyActionFor(key, Qt::ControlModifier, context) == KeyAction::None, "A Ctrl key was taken from applications");
    }
    // The arrows are taken only while cards are shown.
    check(keyActionFor(Qt::Key_Left, Qt::MetaModifier, desktop) == KeyAction::None
              && keyActionFor(Qt::Key_Up, Qt::MetaModifier, desktop) == KeyAction::None,
          "Meta's arrows were taken with no cards shown");
    check(keyActionFor(Qt::Key_Left, Qt::MetaModifier, active) == KeyAction::Previous
              && keyActionFor(Qt::Key_Right, Qt::MetaModifier, active) == KeyAction::Next
              && keyActionFor(Qt::Key_Up, Qt::MetaModifier, active) == KeyAction::StackPrevious
              && keyActionFor(Qt::Key_Down, Qt::MetaModifier, active) == KeyAction::StackNext,
          "Meta's arrows did not move through the cards");
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
              && !keyActionRepeats(KeyAction::Spread) && !keyActionRepeats(KeyAction::Release),
          "A held key repeated what should happen once");
    std::cout << "Kadunce's keys sit on Meta and in Spread, and leave Ctrl, Alt+Tab and Tettegouche's keys alone\n";
}
