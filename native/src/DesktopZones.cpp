/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "DesktopZones.h"

#include <core/output.h>
#include <core/rect.h>
#include <window.h>
#include <workspace.h>

#include <QMetaObject>
#include <QSequentialIterable>
#include <QVariant>

namespace Kadunce {

namespace {

// KWin keeps its tiles out of its installed headers but publishes them to
// scripting through Qt's meta-object system, which is what is read here. A
// tile derives from QObject alone, so a tile pointer is a QObject pointer.
QObject *asObject(void *tile)
{
    return static_cast<QObject *>(tile);
}

QList<QObject *> childZones(QObject *tile)
{
    QList<QObject *> children;
    const QVariant value = tile->property("tiles");
    if (!value.isValid() || !value.canConvert<QSequentialIterable>()) return children;
    for (const QVariant &child : value.value<QSequentialIterable>())
        if (auto *object = child.value<QObject *>()) children.append(object);
    return children;
}

QRectF rectProperty(QObject *tile, const char *name)
{
    const QVariant value = tile->property(name);
    if (value.canConvert<KWin::RectF>()) return QRectF(value.value<KWin::RectF>());
    return value.toRectF();
}

void collectLeaves(QObject *tile, QList<QObject *> &leaves)
{
    const auto children = childZones(tile);
    if (children.isEmpty()) {
        leaves.append(tile);
        return;
    }
    for (auto *child : children) collectLeaves(child, leaves);
}

// KWin's layout for a display nobody has drawn on: three columns, a quarter,
// a half and a quarter, the full height.
bool isUntouchedDefault(const QList<QObject *> &leaves)
{
    if (leaves.size() != 3) return false;
    constexpr double widths[] = {0.25, 0.5, 0.25};
    double left = 0;
    for (int index = 0; index < 3; ++index) {
        const QRectF relative = rectProperty(leaves.at(index), "relativeGeometry");
        if (std::abs(relative.x() - left) > 0.002 || std::abs(relative.width() - widths[index]) > 0.002
            || std::abs(relative.y()) > 0.002 || std::abs(relative.height() - 1.0) > 0.002) return false;
        left += widths[index];
    }
    return true;
}

} // namespace

QRectF zoneWindowRect(QObject *tile)
{
    if (!tile) return {};
    // KWin's Tile::windowGeometry: half the padding between zones and the
    // whole of it against the display's edges.
    const QRectF absolute = rectProperty(tile, "absoluteGeometry");
    const QRectF relative = rectProperty(tile, "relativeGeometry");
    const double padding = tile->property("padding").toDouble();
    if (!absolute.isValid()) return {};
    const double left = relative.left() > 0.0 ? padding / 2 : padding;
    const double top = relative.top() > 0.0 ? padding / 2 : padding;
    const double right = relative.right() < 1.0 ? padding / 2 : padding;
    const double bottom = relative.bottom() < 1.0 ? padding / 2 : padding;
    return absolute.adjusted(left, top, -right, -bottom);
}

QList<DrawnZone> drawnZones(KWin::LogicalOutput *output)
{
    QList<DrawnZone> zones;
    if (!output || !KWin::workspace()) return zones;
    QObject *root = asObject(KWin::workspace()->rootTile(output));
    if (!root) return zones;
    QList<QObject *> leaves;
    collectLeaves(root, leaves);
    if (leaves.size() < 2 || isUntouchedDefault(leaves)) return zones;
    for (auto *leaf : std::as_const(leaves)) {
        const QRectF rect = zoneWindowRect(leaf);
        if (rect.width() >= 1 && rect.height() >= 1) zones.append({leaf, rect});
    }
    return zones.size() >= 2 ? zones : QList<DrawnZone>{};
}

bool placeInZone(QObject *tile, KWin::Window *window)
{
    if (!tile || !window) return false;
    bool placed = false;
    QMetaObject::invokeMethod(tile, "manage", Qt::DirectConnection,
        Q_RETURN_ARG(bool, placed), Q_ARG(KWin::Window *, window));
    return placed;
}

QObject *zoneOf(KWin::Window *window)
{
    return window ? asObject(window->requestedTile()) : nullptr;
}

void leaveZone(KWin::Window *window)
{
    QObject *tile = zoneOf(window);
    if (!tile) return;
    bool removed = false;
    QMetaObject::invokeMethod(tile, "unmanage", Qt::DirectConnection,
        Q_RETURN_ARG(bool, removed), Q_ARG(KWin::Window *, window));
}

} // namespace Kadunce
