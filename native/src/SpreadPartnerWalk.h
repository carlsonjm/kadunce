/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "SpreadModel.h"

namespace Kadunce
{
// CARD-LIFECYCLE.md §3 names a Bento partner by walking Spread entry order from
// the entry holding the carried card. The order is cyclic and its steps are
// entries, so a stack costs one step however many members it holds.
//
// `SpreadModel`'s layout and insertion-selection policy are frozen, so this
// reads the order through its public surface instead of extending it. Both
// helpers are read-only: a prepared carry embeds the workspace revision, and
// naming a partner must not move selection or the drawn pair side.

// The offset of the entry holding `cardId`, relative to the selected entry, or
// -1 when the Spread does not hold that card.
[[nodiscard]] inline int spreadEntryOffset(const SpreadModel &model, int cardId)
{
    if (cardId <= 0) return -1;
    for (int offset = 0; offset < model.count(); ++offset) {
        if (model.sameStack(model.idAtOffset(offset), cardId)) return offset;
    }
    return -1;
}

// The selected face of the entry `steps` away from the entry holding `cardId`.
// Negative steps walk toward the left shoulder, positive toward the right,
// matching the drawn neighbourhood. Returns 0 when the card is not in Spread.
[[nodiscard]] inline int spreadFaceAtStepsFrom(const SpreadModel &model,
                                               int cardId, int steps)
{
    const int offset = spreadEntryOffset(model, cardId);
    return offset < 0 ? 0 : model.idAtOffset(offset + steps);
}
} // namespace Kadunce
