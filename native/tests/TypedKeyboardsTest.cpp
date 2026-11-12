/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of which keyboards keep the on-screen keys from
// answering a tap: only one that has typed text, and only while attached.
#include "TypedKeyboards.h"

#include <cstdlib>
#include <iostream>

using Kadunce::TypedKeyboards;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

// evdev key codes, as linux/input-event-codes.h names them.
constexpr quint32 KeyEsc = 1, Key1 = 2, Key0 = 11, KeyBackspace = 14, KeyTab = 15, KeyQ = 16,
                  KeyEnter = 28, KeyLeftCtrl = 29, KeyA = 30, KeyGrave = 41, KeyLeftShift = 42,
                  KeyZ = 44, KeySlash = 53, KeySpace = 57, KeyF1 = 59, Key102nd = 86,
                  KeyUp = 103, KeyVolumeUp = 115, KeyLeftMeta = 125;
constexpr quintptr Falchion = 0x1000, MouseReceiver = 0x2000, Cover = 0x3000, Controller = 0x4000;
} // namespace

int main()
{
    // Letters, digits and marks type; nothing else does.
    {
        for (quint32 code : {Key1, Key0, KeyQ, KeyA, KeyGrave, KeyZ, KeySlash, Key102nd})
            require(TypedKeyboards::typesText(code), "a letter, digit or mark types text");
        for (quint32 code : {KeyEsc, KeyBackspace, KeyTab, KeyEnter, KeyLeftCtrl, KeyLeftShift,
                             KeySpace, KeyF1, KeyUp, KeyVolumeUp, KeyLeftMeta})
            require(!TypedKeyboards::typesText(code), "a control, modifier or media key types nothing");
    }
    // Keyboards a person types on, by where udev finds them.
    {
        require(TypedKeyboards::isRealKeyboardPath(QStringLiteral(
                    "/sys/devices/pci0000:00/0000:00:08.1/0000:c4:00.3/usb1/1-1/1-1:1.0/0003:0B05:1A30.0001/input/input5/event5")),
                "a USB keyboard counts");
        require(TypedKeyboards::isRealKeyboardPath(QStringLiteral(
                    "/sys/devices/virtual/misc/uhid/0005:0B05:1B08.0010/input/input53/event20")),
                "a Bluetooth keyboard, through uhid, counts");
        require(TypedKeyboards::isRealKeyboardPath(QStringLiteral("/sys/devices/platform/i8042/serio0/input/input3/event3")),
                "a built-in keyboard counts");
        require(!TypedKeyboards::isRealKeyboardPath(QStringLiteral("/sys/devices/virtual/input/input60/event25")),
                "a program's uinput keyboard does not");
        require(!TypedKeyboards::isRealKeyboardPath(QString()), "KWin's own devices, with no path, do not");
    }
    // Nothing typed yet: taps bring the keys.
    {
        TypedKeyboards typed;
        require(!typed.any(), "nothing counts before anything types");
    }
    // A mouse receiver and a controller that call themselves keyboards send
    // buttons, never text, and never count.
    {
        TypedKeyboards typed;
        require(!typed.keyPressed(MouseReceiver, KeyVolumeUp), "a mouse's media button does not count");
        require(!typed.keyPressed(Controller, KeyEnter), "a controller's Enter does not count");
        require(!typed.keyPressed(Controller, KeyEsc), "nor its Escape");
        require(!typed.keyPressed(Controller, KeySpace), "nor its space");
        require(!typed.keyPressed(Controller, KeyUp), "nor its arrows");
        require(!typed.any(), "so taps still bring the keys");
    }
    // Typing on a real keyboard keeps taps from bringing the keys until it goes.
    {
        TypedKeyboards typed;
        require(typed.keyPressed(Falchion, KeyA), "the first letter makes the keyboard count");
        require(!typed.keyPressed(Falchion, KeyZ), "it counts once");
        require(typed.any(), "taps no longer bring the keys");
        require(!typed.removed(MouseReceiver), "a receiver unplugged changes nothing");
        require(typed.any(), "the keyboard typed on is still there");
        require(typed.removed(Falchion), "unplugging the keyboard typed on frees the keys");
        require(!typed.any(), "taps bring the keys again");
        require(!typed.removed(Falchion), "a second removal is nothing");
    }
    // Two keyboards typed on: the keys wait for both.
    {
        TypedKeyboards typed;
        typed.keyPressed(Falchion, KeyQ);
        typed.keyPressed(Cover, Key1);
        require(typed.count() == 2, "both count");
        require(!typed.removed(Cover), "the cover off leaves the other keyboard");
        require(typed.any(), "so the keys still wait");
        require(typed.removed(Falchion), "and the last one gone frees them");
    }
    // The tablet's cover detaching forgets every keyboard.
    {
        TypedKeyboards typed;
        typed.keyPressed(Falchion, KeyA);
        typed.keyPressed(Cover, KeyA);
        typed.forget();
        require(!typed.any(), "holding a tablet, taps bring the keys");
        require(typed.keyPressed(Falchion, KeyA), "typing again counts again");
    }
    // A device with no identity never counts.
    {
        TypedKeyboards typed;
        require(!typed.keyPressed(0, KeyA), "no device, nothing to count");
        require(!typed.any(), "and nothing waits");
    }
    return 0;
}
