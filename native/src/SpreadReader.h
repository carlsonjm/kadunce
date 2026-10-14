/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "SpreadReading.h"

#include <QPointer>
#include <QRect>

#include <memory>

class QWindow;

namespace KWin
{
class OffscreenQuickScene;
}

namespace Kadunce
{

// Names Spread for a screen reader. Spread is painted by the compositor, so
// it has nothing a screen reader can read; this keeps an undrawn scene
// beside it holding one named item per entry, and gives the chosen entry the
// focus, as KWin's own task switcher does: its scene is made the focused
// window without the compositor's focus moving, so the screen reader hears
// each card as Spread chooses it, and nothing else changes.
class SpreadReader
{
public:
    SpreadReader();
    ~SpreadReader();
    SpreadReader(const SpreadReader &) = delete;
    SpreadReader &operator=(const SpreadReader &) = delete;

    // Whether a screen reader is listening. Without one nothing is made.
    [[nodiscard]] static bool listening();

    void show(const QRect &geometry, const SpreadReading &reading);
    void hide();
    [[nodiscard]] bool visible() const;

private:
    std::unique_ptr<KWin::OffscreenQuickScene> m_scene;
    SpreadReading m_reading;
    QPointer<QWindow> m_previousFocus;
    bool m_focused = false;
};

} // namespace Kadunce
