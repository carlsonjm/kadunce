/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
// What the desktop stage's source files share. Not for the Effect.
#include "DesktopStageController.h"
#include "LogName.h"
#include "DesktopZones.h"
#include "BentoCompositeGeometry.h"
#include "BentoSessionTransfer.h"
#include "DisplaySizing.h"
#include "OwnershipHandoff.h"
#include "NativePlacement.h"
#include "WindowStateRestore.h"
#include "RestoreOutputPlan.h"

#include <core/output.h>
#include <effect/effecthandler.h>
#include <window.h>
#include <workspace.h>

#include <QDebug>
#include <QSet>
#include <QScopeGuard>
#include <QScopedValueRollback>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace Kadunce
{

inline BentoPixelRect pixelRect(const QRectF &rect)
{
    return {int(std::lround(rect.x())), int(std::lround(rect.y())),
            std::max(1, int(std::lround(rect.width()))), std::max(1, int(std::lround(rect.height())))};
}

// A pane short of its rect, as a terminal keeping whole rows is or an
// application still answering the size it was given, lies within it, and so
// covers nothing beside it.
inline bool withinPixel(const KWin::RectF &frame, const BentoPixelRect &pixel)
{
    return frame.x() >= pixel.x - 1.5 && frame.y() >= pixel.y - 1.5
        && frame.x() + frame.width() <= pixel.x + pixel.width + 1.5
        && frame.y() + frame.height() <= pixel.y + pixel.height + 1.5;
}


// The version this build is, as the journal names it (CMakeLists.txt).
#ifndef KADUNCE_REVISION
#define KADUNCE_REVISION "unversioned"
#endif

constexpr auto Revision = KADUNCE_REVISION;

} // namespace Kadunce
