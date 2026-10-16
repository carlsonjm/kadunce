/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
// Small helpers the Effect's own source files share. Not for the stages.

#include "MotionTime.h"

#include <core/rect.h>
#include <effect/effecthandler.h>

#include <QJsonObject>

namespace Kadunce {

// A custom motion's duration at Plasma's animation speed (MotionTime.h).
inline int motion(int base)
{
    return motionDuration(base, KWin::effects ? KWin::effects->animationTimeFactor() : 1.0);
}

// A rectangle as the D-Bus context and the guest protocol give it.
inline QJsonObject geometryContext(const KWin::Rect &geometry)
{
    return {
        {QStringLiteral("x"), geometry.x()},
        {QStringLiteral("y"), geometry.y()},
        {QStringLiteral("width"), geometry.width()},
        {QStringLiteral("height"), geometry.height()},
    };
}

} // namespace Kadunce
