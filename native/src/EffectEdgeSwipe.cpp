/* SPDX-License-Identifier: GPL-2.0-or-later */
// Edge swipes (INPUT.md § Edge swipes): a swipe in from a touchscreen's bare
// side edge sends the key Settings set for that edge to the window in focus.
// The key is typed by a keyboard of Kadunce's own, added to KWin's input like
// any other, so shortcuts and clients see it exactly as a key pressed on a
// keyboard (DECISIONS.md, 24 Nov 2026).
#include "EffectInternal.h"

#include <core/inputdevice.h>
#include <input.h>
#include <keyboard_input.h>
#include <xkb.h>

#include <KConfigGroup>
#include <KSharedConfig>
#include <QKeySequence>
#include <QPointer>

#include <linux/input-event-codes.h>

namespace Kadunce
{
namespace
{
class EdgeSwipeKeyboard : public KWin::InputDevice
{
public:
    QString name() const override { return QStringLiteral("Kadunce edge swipes"); }
    bool isEnabled() const override { return true; }
    void setEnabled(bool) override {}
    bool isKeyboard() const override { return true; }
    bool isPointer() const override { return false; }
    bool isTouchpad() const override { return false; }
    bool isTouch() const override { return false; }
    bool isTabletTool() const override { return false; }
    bool isTabletPad() const override { return false; }
    bool isTabletModeSwitch() const override { return false; }
    bool isLidSwitch() const override { return false; }
    void key(quint32 code, bool pressed)
    {
        Q_EMIT keyChanged(code, pressed ? KWin::KeyboardKeyState::Pressed : KWin::KeyboardKeyState::Released,
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()), this);
    }
};

QPointer<EdgeSwipeKeyboard> s_keyboard;

QString edgeSwipeEntry(int side)
{
    const KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-kadunce"));
    const QString value = group.readEntry(side < 0 ? "EdgeSwipeLeft" : "EdgeSwipeRight", QString()).trimmed();
    return value.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0 ? QString() : value;
}

// The evdev codes of a key combination: its modifiers, then its key, found
// in the keymap in use; none when the key has no place in it.
QList<quint32> evdevCodes(QKeyCombination combination)
{
    QList<quint32> codes;
    const Qt::KeyboardModifiers mods = combination.keyboardModifiers();
    if (mods & Qt::MetaModifier) codes << KEY_LEFTMETA;
    if (mods & Qt::ControlModifier) codes << KEY_LEFTCTRL;
    if (mods & Qt::AltModifier) codes << KEY_LEFTALT;
    if (mods & Qt::ShiftModifier) codes << KEY_LEFTSHIFT;
    auto *keyboard = KWin::input() ? KWin::input()->keyboard() : nullptr;
    auto *xkb = keyboard ? keyboard->xkb() : nullptr;
    if (!xkb) return {};
    for (const xkb_keysym_t sym : KWin::Xkb::keysymsFromQtKey(QKeyCombination(combination.key()))) {
        if (const auto code = xkb->keycodeFromKeysym(sym); code && code->keyCode >= 8) {
            codes << code->keyCode - 8;
            return codes;
        }
    }
    return {};
}
} // namespace

bool Effect::edgeSwipeSetForInput(int side) const
{
    return !edgeSwipeEntry(side).isEmpty();
}

void Effect::sendEdgeSwipeKeyFromInput(int side)
{
    const QString entry = edgeSwipeEntry(side);
    const QKeySequence sequence = QKeySequence::fromString(entry, QKeySequence::PortableText);
    const QList<quint32> codes = sequence.isEmpty() ? QList<quint32>() : evdevCodes(sequence[0]);
    if (codes.isEmpty()) {
        qInfo() << "Kadunce edge swipe has no key it can type for" << entry;
        return;
    }
    if (!s_keyboard && KWin::input()) {
        s_keyboard = new EdgeSwipeKeyboard;
        KWin::input()->addInputDevice(s_keyboard);
    }
    if (!s_keyboard) return;
    for (const quint32 code : codes) s_keyboard->key(code, true);
    for (auto it = codes.crbegin(); it != codes.crend(); ++it) s_keyboard->key(*it, false);
}

void Effect::dropEdgeSwipeKeyboard()
{
    if (!s_keyboard) return;
    if (KWin::input()) KWin::input()->removeInputDevice(s_keyboard);
    delete s_keyboard.data();
}
} // namespace Kadunce
