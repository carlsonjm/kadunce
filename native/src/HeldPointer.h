// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QByteArray>

namespace Kadunce {

// What the pointer shows where Kadunce holds it (INPUT.md). A divider's
// reach, the gaps beside the Active card and the gutters around cards and
// panes are Kadunce's: no window there takes the pointer, and its shape says
// what a press does.
enum class HeldPointer {
    None,
    Arrow,
    PageLeft,
    PageRight,
    ResizeColumns,
    ResizeRows,
    Carrying,
};

struct HeldPointerPlace {
    bool tableOpen = false;
    bool tableCarrying = false;
    // 1 on a divider between side-by-side panes, 2 between stacked ones.
    int divider = 0;
    // -1 or 1 in the gap left or right of the Active card.
    int activeSide = 0;
    bool cardGap = false;
};

inline HeldPointer heldPointer(const HeldPointerPlace &place)
{
    if (place.tableOpen) return place.tableCarrying ? HeldPointer::Carrying : HeldPointer::Arrow;
    if (place.divider == 1) return HeldPointer::ResizeColumns;
    if (place.divider == 2) return HeldPointer::ResizeRows;
    if (place.activeSide < 0) return HeldPointer::PageLeft;
    if (place.activeSide > 0) return HeldPointer::PageRight;
    return place.cardGap ? HeldPointer::Arrow : HeldPointer::None;
}

// The cursor theme's name for each. The page tabs are Kadunce's own names; a
// theme without them shows the arrow.
inline QByteArray heldPointerName(HeldPointer pointer)
{
    switch (pointer) {
    case HeldPointer::PageLeft: return QByteArrayLiteral("kadunce-page-left");
    case HeldPointer::PageRight: return QByteArrayLiteral("kadunce-page-right");
    case HeldPointer::ResizeColumns: return QByteArrayLiteral("col-resize");
    case HeldPointer::ResizeRows: return QByteArrayLiteral("row-resize");
    case HeldPointer::Carrying: return QByteArrayLiteral("grabbing");
    case HeldPointer::Arrow:
    case HeldPointer::None: break;
    }
    return QByteArrayLiteral("default");
}

} // namespace Kadunce
