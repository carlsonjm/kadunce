/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of the refused-gesture note: which refusal an edge
// decision names, how long the note is readable, and that it always sits whole
// on the display it was let go on.
#include "GestureNote.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
const QRectF Display(0, 0, 2560, 1600);
const QSizeF Note(220, 36);
} // namespace

int main()
{
    // The Active card at a side edge with nothing beside it changes nothing.
    require(edgeEntryRefusal(EdgeEntryOutcome::Unchanged, true) == GestureRefusal::NoPartner,
        "A side snap without a partner did not say there is nothing to pair with");
    // A dialog at an edge is not arranged.
    require(edgeEntryRefusal(EdgeEntryOutcome::Refuse, false) == GestureRefusal::NotArranged,
        "An ineligible window's refusal did not say it cannot be arranged");
    // Outcomes that go on to a layout solve are not refused by the decision.
    for (const auto outcome : {EdgeEntryOutcome::AdoptDisplay, EdgeEntryOutcome::MakeActive,
                               EdgeEntryOutcome::PairIntoBento,
                               EdgeEntryOutcome::ComposeDisplayBento}) {
        require(!edgeEntryRefusal(outcome, true), "An admitting edge was reported as refused");
    }
    // Every refusal has words, and they differ.
    const QString texts[] = {gestureRefusalText(GestureRefusal::NoPartner),
                             gestureRefusalText(GestureRefusal::NoRoom),
                             gestureRefusalText(GestureRefusal::NotArranged),
                             gestureRefusalText(GestureRefusal::Unplaced)};
    for (int i = 0; i < 4; ++i) {
        require(!texts[i].isEmpty(), "A refusal had no words");
        for (int j = i + 1; j < 4; ++j) require(texts[i] != texts[j], "Two refusals read the same");
    }

    // The note fades in, holds long enough to read, then fades away.
    using T = GestureNoteTiming;
    require(gestureNoteOpacity(0) == 0.0, "The note appeared at full strength");
    require(gestureNoteOpacity(T::FadeInMs) == 1.0, "The note was not whole after fading in");
    require(gestureNoteOpacity(T::FadeInMs + T::HoldMs) == 1.0, "The note faded before it was read");
    require(gestureNoteOpacity(T::FadeInMs + T::HoldMs + T::FadeOutMs / 2) > 0.0
                && gestureNoteOpacity(T::FadeInMs + T::HoldMs + T::FadeOutMs / 2) < 1.0,
        "The note did not fade out");
    require(gestureNoteOpacity(T::TotalMs) == 0.0, "The note outlasted its time");
    require(T::HoldMs >= 1500, "The note is too brief to read");

    // At a side edge mid-height, the note sits above the release and whole.
    const QRectF left = gestureNoteBox(Display, QPointF(4, 800), Note);
    require(Display.contains(left), "A note at the left edge left the display");
    require(left.bottom() < 800, "A note at the side covered the release point");
    const QRectF right = gestureNoteBox(Display, QPointF(2556, 800), Note);
    require(Display.contains(right), "A note at the right edge left the display");
    // At the top edge there is no room above, so it sits below the release.
    const QRectF top = gestureNoteBox(Display, QPointF(1280, 4), Note);
    require(Display.contains(top) && top.top() > 4, "A note at the top edge was not below it");
    require(qAbs(top.center().x() - 1280) < 0.5, "A note was not centred over the release");
    // On a second display, it stays on that display.
    const QRectF second(2560, 0, 1920, 1080);
    require(second.contains(gestureNoteBox(second, QPointF(2562, 500), Note)),
        "A note on a second display left it");
    // A note wider than a narrow display is narrowed to fit.
    const QRectF narrow(0, 0, 200, 400);
    require(narrow.contains(gestureNoteBox(narrow, QPointF(100, 200), Note)),
        "A note wider than its display spilled over");
    return 0;
}
