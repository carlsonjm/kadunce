/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QString>

namespace Kadunce {

// Kadunce reaches into KWin's own internals, which change between releases,
// so it plays only on the KWin it was built against. A different KWin leaves
// Cards off, and Plasma runs as it does without them, until a rebuild for it
// arrives. A KWin that does not say its version is taken as the one built for.
inline bool kwinMatches(const QString &built, const QString &running)
{
    const QString runs = running.trimmed();
    return built.isEmpty() || runs.isEmpty() || runs == built;
}

} // namespace Kadunce
