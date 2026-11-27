/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
// What the card stage's source files share. Not for the Effect.
#include "CardStageController.h"
#include "LogName.h"

#include "ActiveStep.h"
#include "RowCardTarget.h"
#include "KeyboardRoom.h"
#include "HeldCardGeometry.h"
#include "MotionTime.h"
#include "PaneArrival.h"
#include "BentoCompositeGeometry.h"
#include "OwnershipHandoff.h"
#include "NeighborStackPose.h"
#include "RowPageMotion.h"
#include "StackBrowseMotion.h"
#include "SpreadStroke.h"
#include "SpreadLayout.h"
#include "FocusedPairLayout.h"
#include "WindowStateRestore.h"
#include "NativePlacement.h"
#include "ReleasePlace.h"

#include <core/output.h>
#include <effect/effecthandler.h>
#include <window.h>
#include <workspace.h>

#include <QDebug>
#include <QEasingCurve>
#include <QScopedValueRollback>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace Kadunce
{

// The version this build is, as the journal names it (CMakeLists.txt).
#ifndef KADUNCE_REVISION
#define KADUNCE_REVISION "unversioned"
#endif

constexpr auto Revision = KADUNCE_REVISION;
constexpr int PreviewTransitionDuration = 280;
constexpr int ArrivalExpandDuration = 220;
// A Shuffle panel's slide in from its edge and out again.
constexpr int PanelSlideInDuration = 320;
constexpr int PanelSlideOutDuration = 120;
// Growing further than this, the card takes longer than the panel and eases in.
inline constexpr double PanelSlideOutFar = 32.0;
inline constexpr int PanelSlideOutSettle = 200;
// How often the card's client is asked for its next size on the way.
inline constexpr int WorkAreaStepInterval = 8;
constexpr double LauncherGuestCommitDistance = 58.0;
constexpr double LauncherGuestDragPreview = 0.18;

} // namespace Kadunce
