/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QList>
#include <QSizeF>
#include <QString>

#include <cmath>

namespace Kadunce
{
// Which display holds cards: the one a touchscreen drives.
//
// A display's name says nothing about whether it can be touched. A desktop's
// touch monitor is named like any monitor, and a laptop's built-in panel is
// named the same whether or not it has a digitizer. The touchscreen itself is
// the evidence, so cards go where KWin sends its contacts, and a machine with
// no touchscreen has no card display at all: it keeps Bento and the desktop.
struct TouchDisplayOutput {
    QString name;
    bool internal = false;
    // Millimetres, as the display reports them.
    QSizeF physicalSize;
};

struct TouchDisplayDevice {
    // The display the user chose for it, empty when they chose none.
    QString outputName;
    // Millimetres, as libinput reports the digitizer.
    QSizeF size;
};

// KWin's own placement of a touchscreen (libinput Connection::
// applyScreenToDevice): the chosen display by name, else a display whose
// physical size matches the digitizer's, else the built-in display, else the
// first. Deciding it the same way keeps the cards on the display the contacts
// arrive on. Returns -1 when there is no display.
inline int touchDeviceOutput(const QList<TouchDisplayOutput> &outputs,
    const TouchDisplayDevice &device)
{
    if (!device.outputName.isEmpty()) {
        for (int i = 0; i < outputs.size(); ++i)
            if (outputs.at(i).name == device.outputName) return i;
    }
    const auto matches = [&device](const TouchDisplayOutput &output) {
        return std::round(device.size.width()) == std::round(output.physicalSize.width())
            && std::round(device.size.height()) == std::round(output.physicalSize.height());
    };
    for (int i = 0; i < outputs.size(); ++i)
        if (matches(outputs.at(i))) return i;
    for (int i = 0; i < outputs.size(); ++i)
        if (outputs.at(i).internal) return i;
    return outputs.isEmpty() ? -1 : 0;
}

// One touchscreen holds cards. Where two drive different displays, the
// built-in one wins, so docking a tablet to a touch monitor does not move its
// cards; otherwise the first touched display in screen order. Empty when no
// touchscreen drives any display.
inline QString cardOutputName(const QList<TouchDisplayOutput> &outputs,
    const QList<TouchDisplayDevice> &devices)
{
    const auto precedes = [&outputs](int candidate, int current) {
        const bool internal = outputs.at(candidate).internal;
        if (internal != outputs.at(current).internal) return internal;
        return candidate < current;
    };
    int chosen = -1;
    for (const auto &device : devices) {
        const int index = touchDeviceOutput(outputs, device);
        if (index >= 0 && (chosen < 0 || precedes(index, chosen))) chosen = index;
    }
    return chosen < 0 ? QString() : outputs.at(chosen).name;
}
} // namespace Kadunce
