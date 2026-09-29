/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "NativeMoveTakeover.h"
#include <effect/effectwindow.h>
#include <memory>

namespace Kadunce {
class CardStageController;
class DesktopStageController;

// Read-only source reservation: no membership removal, geometry mutation or
// native cancellation. The originating controller must validate immediately
// before/after takeover. Dropping it does not require rollback.
class PreparedCarrySource {
public:
    const CarryOrigin &origin() const { return m_origin; }
    const NativeMoveSnapshot &restoreSnapshot() const { return m_restore; }
    bool isDesktopWindow() const { return m_desktopWindow; }
private:
    friend class CardStageController;
    friend class DesktopStageController;
    CarryOrigin m_origin;
    NativeMoveSnapshot m_restore;
    QPointer<KWin::EffectWindow> m_window;
    std::weak_ptr<const int> m_owner;
    quint64 m_generation = 0;
    KWin::Rect m_outputGeometry;
    bool m_desktopWindow = false;
};
}
