// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <Qt>

namespace Kadunce {

// The keys Kadunce answers (INPUT.md, and its controls map). They sit on Meta,
// which applications leave to the system, so Ctrl stays with applications. The
// arrows say where: with Meta alone the focus goes there, and with Shift the
// window goes there. They are taken only while cards or a layout are shown,
// and the plain keys only while Spread is: anywhere else they stay KDE's or
// the application's.
enum class KeyAction {
    None,
    // Taken and doing nothing: KDE's own meaning would break the cards.
    Hold,
    Spread,
    Table,
    Previous,
    Next,
    StackPrevious,
    StackNext,
    Bento,
    ZoneMode,
    Release,
    Open,
    Back,
    FocusLeft,
    FocusRight,
    FocusUp,
    FocusDown,
    CarryLeft,
    CarryRight,
    // A pane's card goes one place deeper in its Stack, or, in none, joins
    // the Stack of the pane used just before it.
    Deeper,
    // A pane's card leaves its Stack, or, in none, leaves the layout.
    LetGo,
    DividerLeft,
    DividerRight,
};

struct KeyContext {
    // Cards are on the tablet, as the Active card or in Spread.
    bool cardsShown = false;
    // Spread is shown and nothing else, such as the search launcher, has the
    // keys.
    bool spreadShown = false;
    // Table answers its key; a test can leave it out.
    bool table = false;
    // The focused window is a pane of a display's layout, outside Spread.
    bool pane = false;
    // The Active card stands alone on a display sized Connected, where it
    // moves as a pane does.
    bool lone = false;
};

inline KeyAction keyActionFor(int key, Qt::KeyboardModifiers modifiers, const KeyContext &context)
{
    const Qt::KeyboardModifiers relevant = modifiers
        & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    // Spread answers for the row it shows, whichever window has the focus.
    const bool pane = context.pane && !context.spreadShown;
    const bool stack = context.cardsShown || pane;
    const bool carry = pane || (context.lone && !context.spreadShown);
    if (relevant == Qt::MetaModifier) {
        switch (key) {
        case Qt::Key_S: return KeyAction::Spread;
        case Qt::Key_W: return context.table ? KeyAction::Table : KeyAction::None;
        case Qt::Key_B: return KeyAction::Bento;
        case Qt::Key_Escape: return KeyAction::Release;
        case Qt::Key_Left: return pane ? KeyAction::FocusLeft
            : context.cardsShown ? KeyAction::Previous : KeyAction::None;
        case Qt::Key_Right: return pane ? KeyAction::FocusRight
            : context.cardsShown ? KeyAction::Next : KeyAction::None;
        case Qt::Key_Up: return pane ? KeyAction::FocusUp
            : context.cardsShown ? KeyAction::Hold : KeyAction::None;
        case Qt::Key_Down: return pane ? KeyAction::FocusDown
            : context.cardsShown ? KeyAction::Hold : KeyAction::None;
        // A Stack is a ring, so Tab goes round it.
        case Qt::Key_Tab: return stack ? KeyAction::StackNext : KeyAction::None;
        // Shift, spent on making Tab into Backtab, may not be reported.
        case Qt::Key_Backtab: return stack ? KeyAction::StackPrevious : KeyAction::None;
        case Qt::Key_BracketLeft: return pane ? KeyAction::DividerLeft : KeyAction::None;
        case Qt::Key_BracketRight: return pane ? KeyAction::DividerRight : KeyAction::None;
        default: return KeyAction::None;
        }
    }
    if (relevant == (Qt::MetaModifier | Qt::ShiftModifier)) {
        switch (key) {
        // Monitors switch between filling themselves and the zones drawn there.
        case Qt::Key_B: return KeyAction::ZoneMode;
        case Qt::Key_Tab:
        case Qt::Key_Backtab: return stack ? KeyAction::StackPrevious : KeyAction::None;
        case Qt::Key_Left: return carry ? KeyAction::CarryLeft : KeyAction::None;
        case Qt::Key_Right: return carry ? KeyAction::CarryRight : KeyAction::None;
        case Qt::Key_Up: return pane ? KeyAction::Deeper : KeyAction::None;
        case Qt::Key_Down: return pane ? KeyAction::LetGo : KeyAction::None;
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

// Held down, moving the focus, along the row or round a Stack, a card deeper
// and a divider repeat; carrying a window and everything else happen once.
inline bool keyActionRepeats(KeyAction action)
{
    switch (action) {
    case KeyAction::Previous:
    case KeyAction::Next:
    case KeyAction::StackPrevious:
    case KeyAction::StackNext:
    case KeyAction::FocusLeft:
    case KeyAction::FocusRight:
    case KeyAction::FocusUp:
    case KeyAction::FocusDown:
    case KeyAction::Deeper:
    case KeyAction::DividerLeft:
    case KeyAction::DividerRight:
    case KeyAction::Hold: return true;
    default: return false;
    }
}

} // namespace Kadunce
