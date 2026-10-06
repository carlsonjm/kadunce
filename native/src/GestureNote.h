/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "DeliberateEdgeEntry.h"

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>

#include <algorithm>
#include <optional>

namespace Kadunce
{
// Why a released gesture changed nothing. A refused gesture springs back to
// where it began, and without a word that reads as broken, so letting go names
// the reason in a short note beside the release.
enum class GestureRefusal {
    // A side edge pairs the window with a partner, and there is none.
    NoPartner,
    // The windows' smallest sizes do not fit the layout the gesture would make.
    NoRoom,
    // The window is not one Kadunce arranges, such as a dialog.
    NotArranged,
    // The destination stopped accepting the window before it was let go.
    Unplaced,
};

[[nodiscard]] inline QString gestureRefusalText(GestureRefusal refusal)
{
    switch (refusal) {
    case GestureRefusal::NoPartner:
        return QObject::tr("Nothing to pair it with");
    case GestureRefusal::NoRoom:
        return QObject::tr("Not enough room for it here");
    case GestureRefusal::NotArranged:
        return QObject::tr("This window can't be arranged");
    case GestureRefusal::Unplaced:
        break;
    }
    return QObject::tr("It can't go there");
}

// The refusal an edge decision states by itself. Pairing and placement that
// pass this decision can still be refused by the layout solve, which the
// caller reports as NoRoom.
[[nodiscard]] constexpr std::optional<GestureRefusal> edgeEntryRefusal(
    EdgeEntryOutcome outcome, bool carriedEligible)
{
    if (outcome == EdgeEntryOutcome::Unchanged) return GestureRefusal::NoPartner;
    if (outcome == EdgeEntryOutcome::Refuse && !carriedEligible)
        return GestureRefusal::NotArranged;
    return std::nullopt;
}

// How long a note stays: long enough to read four words, then it fades. Reading
// time is not motion, so Plasma's animation speed does not shorten it.
struct GestureNoteTiming {
    static constexpr int FadeInMs = 120;
    static constexpr int HoldMs = 2000;
    static constexpr int FadeOutMs = 300;
    static constexpr int TotalMs = FadeInMs + HoldMs + FadeOutMs;
};

[[nodiscard]] constexpr double gestureNoteOpacity(qint64 elapsedMs)
{
    using T = GestureNoteTiming;
    if (elapsedMs < 0 || elapsedMs >= T::TotalMs) return 0.0;
    if (elapsedMs < T::FadeInMs) return double(elapsedMs) / T::FadeInMs;
    const qint64 fading = elapsedMs - T::FadeInMs - T::HoldMs;
    if (fading <= 0) return 1.0;
    return 1.0 - double(fading) / T::FadeOutMs;
}

// Where the note sits: centred over the point that was let go, above it so the
// hand does not cover it, or below it where the display's top leaves no room,
// and always whole on that display.
[[nodiscard]] inline QRectF gestureNoteBox(const QRectF &display, QPointF contact,
                                           QSizeF size)
{
    constexpr double margin = 16.0;
    constexpr double gap = 40.0;
    if (!display.isValid() || size.isEmpty()) return {};
    const double width = std::min(size.width(), std::max(0.0, display.width() - 2 * margin));
    const double height = size.height();
    double top = contact.y() - gap - height;
    if (top < display.top() + margin) top = contact.y() + gap;
    top = std::clamp(top, display.top() + margin,
                     std::max(display.top() + margin, display.bottom() - margin - height));
    const double left = std::clamp(contact.x() - width / 2, display.left() + margin,
                                   std::max(display.left() + margin,
                                            display.right() - margin - width));
    return QRectF(left, top, width, height);
}
} // namespace Kadunce
