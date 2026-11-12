/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QSet>
#include <QString>
#include <QtGlobal>

namespace Kadunce
{
// The real keyboards the person has typed on, while they stay attached.
//
// A tap on a text field brings the on-screen keys up only while this is
// empty: someone who has typed on a keyboard they plugged in or paired is
// using it, and a finger on the screen is choosing where to type, not asking
// for keys. The keys come back by themselves once every keyboard typed on has
// gone, unplugged, out of range or asleep, or the tablet's own cover has come
// off.
//
// A device counts only once it has typed a letter, a digit or a mark. Many
// devices call themselves keyboards without being one a person types on: a
// gaming mouse's receiver, a game controller's dongle, a power button. Those
// send media keys, Enter, Escape or arrows, never text, so they never count,
// however long they stay plugged in.
class TypedKeyboards
{
public:
    // Whether the evdev key `code` types text: a digit, letter or mark on a
    // standard keyboard's main block. The space bar, Enter, Backspace, Tab,
    // Escape, modifiers, arrows and every function or media key do not, since
    // controllers and mouse receivers send those as buttons.
    [[nodiscard]] static constexpr bool typesText(quint32 code)
    {
        return (code >= 2 && code <= 13) // 1 to 0, minus, equals
            || (code >= 16 && code <= 27) // Q to P, brackets
            || (code >= 30 && code <= 41) // A to L, semicolon, quote, grave
            || (code >= 43 && code <= 53) // backslash, Z to M, comma, dot, slash
            || code == 86; // the extra key beside left Shift on ISO keyboards
    }

    // Whether a keyboard at the udev path `sysPath` is one a person types on.
    // KWin's own devices, which carry the keys from Shuffle Keyboard, a remote
    // desktop or a fake input client, have no path. A program's keyboard made
    // with uinput sits under /sys/devices/virtual/input. A Bluetooth keyboard
    // also sits under /sys/devices/virtual, but beneath uhid, through which
    // BlueZ hands the kernel its keys, so it counts.
    [[nodiscard]] static bool isRealKeyboardPath(const QString &sysPath)
    {
        if (sysPath.isEmpty()) return false;
        if (!sysPath.contains(QLatin1String("/devices/virtual/"))) return true;
        return sysPath.contains(QLatin1String("/devices/virtual/misc/uhid/"));
    }

    // A key `code` went down on `device`. True when this makes the device
    // count for the first time.
    bool keyPressed(quintptr device, quint32 code)
    {
        if (!device || !typesText(code) || m_typed.contains(device)) return false;
        m_typed.insert(device);
        return true;
    }

    // `device` went away. True when it was the last keyboard typed on, so the
    // keys are free to answer a tap again.
    bool removed(quintptr device)
    {
        return m_typed.remove(device) && m_typed.isEmpty();
    }

    // The tablet's cover came off or folded back: whatever was typed on, the
    // person now holds a tablet.
    void forget() { m_typed.clear(); }

    [[nodiscard]] bool any() const { return !m_typed.isEmpty(); }
    [[nodiscard]] qsizetype count() const { return m_typed.size(); }

private:
    QSet<quintptr> m_typed;
};
} // namespace Kadunce
