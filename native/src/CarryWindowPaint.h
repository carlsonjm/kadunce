/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "CarryPaintPlan.h"
#include <core/renderviewport.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>

namespace Kadunce {
// Caller has already matched the explicitly owned window and marked prepaint
// transformed/translucent. Never use this path for its passive neighbours.
// No membership changes, native placement, output assignment or redirection here.
inline bool paintCarryWindow(const KWin::RenderTarget &renderTarget,
    const KWin::RenderViewport &viewport, KWin::EffectWindow *window, int mask,
    const KWin::Region &region, KWin::WindowPaintData &data,
    const CarryPaintPlan &plan)
{
    if (!window || window->isDeleted() || plan.clip.isEmpty()) return false;
    KWin::Rect logicalRegion = window->expandedGeometry().toRect();
    // Match the established Spread cover transform. If native cancellation
    // restores a different client size, paint still occupies the frozen pickup
    // envelope; no corrective moveResize is issued.
    KWin::Effect::setPositionTransformations(data, logicalRegion, window,
        KWin::RectF(plan.target).toRect(), Qt::KeepAspectRatioByExpanding);
    const KWin::Region clip(viewport.mapToDeviceCoordinatesAligned(KWin::RectF(plan.clip)));
    KWin::effects->paintWindow(renderTarget, viewport, window,
        mask | KWin::Effect::PAINT_WINDOW_TRANSFORMED, region & clip, data);
    return true;
}
} // namespace Kadunce
