/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QList>
#include <QSizeF>
#include <QString>

#include <cmath>

namespace Kadunce
{
// Which display holds cards. One display does, so there is one Spread: the
// display the person chose while it is attached, else the one a touchscreen
// drives, else the largest.
//
// A display's name says nothing about whether it can be touched. A desktop's
// touch monitor is named like any monitor, and a laptop's built-in panel is
// named the same whether or not it has a digitizer. The touchscreen itself is
// the evidence, so by default cards go where KWin sends its contacts.
struct TouchDisplayOutput {
    QString name;
    bool internal = false;
    // Millimetres, as the display reports them.
    QSizeF physicalSize;
    // Logical pixels, for a display that reports no physical size.
    QSizeF size;
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

// The display a touchscreen drives. Where two drive different displays, the
// built-in one wins, so docking a tablet to a touch monitor does not move its
// cards; otherwise the first touched display in screen order. Empty when no
// touchscreen drives any display.
inline QString touchOutputName(const QList<TouchDisplayOutput> &outputs,
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

// The largest display: by physical size where both report one, else by
// logical size, so a large monitor wins over a dense laptop panel. A tie
// goes to the earlier display in screen order. Empty with no display.
inline QString largestOutputName(const QList<TouchDisplayOutput> &outputs)
{
    const auto area = [](const QSizeF &size) {
        return size.isValid() ? size.width() * size.height() : 0.0;
    };
    const auto larger = [&area](const TouchDisplayOutput &candidate, const TouchDisplayOutput &current) {
        const qreal candidatePhysical = area(candidate.physicalSize);
        const qreal currentPhysical = area(current.physicalSize);
        if (candidatePhysical > 0 && currentPhysical > 0) return candidatePhysical > currentPhysical;
        return area(candidate.size) > area(current.size);
    };
    int chosen = -1;
    for (int i = 0; i < outputs.size(); ++i)
        if (chosen < 0 || larger(outputs.at(i), outputs.at(chosen))) chosen = i;
    return chosen < 0 ? QString() : outputs.at(chosen).name;
}

// What the person chose in kaduncerc's [Spread] group.
struct SpreadDisplayChoice {
    // The display Spread lives on, by name; empty to let Kadunce choose.
    QString display;
    // Whether a machine with no touchscreen, or whose chosen display is
    // away, still has a card display. Without it only a touched or chosen
    // display holds cards, and every other display keeps Bento.
    bool withoutTouchscreen = true;
};

// The one display that holds cards. The chosen display while it is attached;
// when it goes away, or none was chosen, the touchscreen's display, else the
// largest. A chosen display that comes back takes Spread back. Empty only with
// no display, or when the person keeps cards to a touchscreen and none drives
// one.
inline QString cardOutputName(const QList<TouchDisplayOutput> &outputs,
    const QList<TouchDisplayDevice> &devices, const SpreadDisplayChoice &choice = {})
{
    if (!choice.display.isEmpty()) {
        for (const auto &output : outputs)
            if (output.name == choice.display) return output.name;
    }
    const QString touched = touchOutputName(outputs, devices);
    if (!touched.isEmpty() || !choice.withoutTouchscreen) return touched;
    return largestOutputName(outputs);
}
} // namespace Kadunce
