// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <Qt>

namespace Kadunce {

// The keys Kadunce answers (INPUT.md, and its controls map). They sit on Meta,
// which applications leave to the system, so Ctrl stays with applications. The
// arrows are taken only while cards are shown, and the plain keys only while
// Spread is: anywhere else they stay KDE's or the application's.
enum class KeyAction {
    None,
    Spread,
    Table,
    Previous,
    Next,
    StackPrevious,
    StackNext,
    Bento,
    Release,
    Open,
    Back,
};

struct KeyContext {
    // Cards are on the tablet, as the Active card or in Spread.
    bool cardsShown = false;
    // Spread is shown and nothing else, such as the search launcher, has the
    // keys.
    bool spreadShown = false;
    // Table answers its key; a test can leave it out.
    bool table = false;
};

inline KeyAction keyActionFor(int key, Qt::KeyboardModifiers modifiers, const KeyContext &context)
{
    const Qt::KeyboardModifiers relevant = modifiers
        & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    if (relevant == Qt::MetaModifier) {
        switch (key) {
        case Qt::Key_S: return KeyAction::Spread;
        case Qt::Key_W: return context.table ? KeyAction::Table : KeyAction::None;
        case Qt::Key_B: return KeyAction::Bento;
        case Qt::Key_Escape: return KeyAction::Release;
        case Qt::Key_Left: return context.cardsShown ? KeyAction::Previous : KeyAction::None;
        case Qt::Key_Right: return context.cardsShown ? KeyAction::Next : KeyAction::None;
        case Qt::Key_Up: return context.cardsShown ? KeyAction::StackPrevious : KeyAction::None;
        case Qt::Key_Down: return context.cardsShown ? KeyAction::StackNext : KeyAction::None;
        default: return KeyAction::None;
        }
    }
    if (relevant == Qt::NoModifier && context.spreadShown) {
        switch (key) {
        case Qt::Key_Left: return KeyAction::Previous;
        case Qt::Key_Right: return KeyAction::Next;
        case Qt::Key_Up: return KeyAction::StackPrevious;
        case Qt::Key_Down: return KeyAction::StackNext;
        case Qt::Key_Return:
        case Qt::Key_Enter: return KeyAction::Open;
        case Qt::Key_Escape: return KeyAction::Back;
        default: return KeyAction::None;
        }
    }
    return KeyAction::None;
}

// Held down, only moving along the row or a Stack repeats.
inline bool keyActionRepeats(KeyAction action)
{
    return action == KeyAction::Previous || action == KeyAction::Next
        || action == KeyAction::StackPrevious || action == KeyAction::StackNext;
}

} // namespace Kadunce
