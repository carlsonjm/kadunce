// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "SavedLayout.h"

#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace Kadunce
{

// A saved Layout as Table hangs it under +: its name, the app whose icon it
// shows, how many windows it brings, and the picture of the desk it was kept
// from. Values only, read from what Split Rock Core's Layouts() answers.
struct TableSavedLayout {
    QString id;
    QString name;
    // The front app of its first group, by desktop id.
    QString frontApp;
    int windows = 0;
    // The desk's width over its height; 0 for a Layout kept without places,
    // which shows its icon and no picture.
    double aspect = 0.0;
    // Each display it stood on and each window seen there, in shares of the
    // whole desk, and each window's app by desktop id.
    QList<QRectF> displays;
    QList<QRectF> pictured;
    QStringList picturedApps;
};

// One Layout from its id, name and groups as Core gives them. A group names
// its display, where its front window was drawn on it (pictureX and on, in
// shares of the display) and the display's place (displayX and on), as
// Settings' Layouts page draws it.
[[nodiscard]] inline TableSavedLayout tableSavedLayout(const QString &id, const QString &name,
                                                       const QList<QVariantMap> &groups)
{
    TableSavedLayout layout;
    layout.id = id;
    layout.name = name.trimmed();
    const QList<SavedLayouts::Group> read = SavedLayouts::groupsFrom(groups);
    for (const auto &group : read) layout.windows += int(group.apps.size());
    if (!read.isEmpty()) layout.frontApp = read.first().apps.value(0);

    QStringList names;
    QList<QRectF> displays;
    QList<QRectF> windows;
    QStringList apps;
    for (const QVariantMap &group : groups) {
        const QString display = group.value(QStringLiteral("display")).toString().trimmed();
        const QRectF screen(group.value(QStringLiteral("displayX")).toDouble(), group.value(QStringLiteral("displayY")).toDouble(),
                            group.value(QStringLiteral("displayWidth")).toDouble(),
                            group.value(QStringLiteral("displayHeight")).toDouble());
        // One group without its display's place, and the desk cannot be drawn.
        if (display.isEmpty() || !(screen.width() > 0.0) || !(screen.height() > 0.0)) return layout;
        const int at = int(names.indexOf(display));
        const QRectF on = at >= 0 ? displays[at] : screen;
        if (at < 0) {
            names.append(display);
            displays.append(screen);
        }
        const double width = group.value(QStringLiteral("pictureWidth")).toDouble();
        const double height = group.value(QStringLiteral("pictureHeight")).toDouble();
        const QStringList groupApps = group.value(QStringLiteral("apps")).toStringList();
        if (!group.value(QStringLiteral("shown")).toBool() || !(width > 0.0) || !(height > 0.0) || groupApps.isEmpty()) continue;
        windows.append(QRectF(on.x() + group.value(QStringLiteral("pictureX")).toDouble() * on.width(),
                              on.y() + group.value(QStringLiteral("pictureY")).toDouble() * on.height(),
                              width * on.width(), height * on.height()));
        apps.append(groupApps.first().trimmed());
    }
    if (displays.isEmpty()) return layout;
    QRectF desk = displays.first();
    for (const QRectF &display : std::as_const(displays)) desk = desk.united(display);
    const auto share = [&desk](const QRectF &rect) {
        return QRectF((rect.x() - desk.x()) / desk.width(), (rect.y() - desk.y()) / desk.height(),
                      rect.width() / desk.width(), rect.height() / desk.height());
    };
    layout.aspect = desk.width() / desk.height();
    for (const QRectF &display : std::as_const(displays)) layout.displays.append(share(display));
    for (const QRectF &window : std::as_const(windows)) layout.pictured.append(share(window).intersected(QRectF(0, 0, 1, 1)));
    layout.picturedApps = apps;
    return layout;
}

} // namespace Kadunce
