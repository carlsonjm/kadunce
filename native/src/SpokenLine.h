/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QPointer>
#include <QRect>
#include <QString>

#include <memory>

class QWindow;

namespace KWin
{
class OffscreenQuickScene;
}

namespace Kadunce
{

// Says what Spread and Table show to a screen reader. Kadunce paints them,
// so they have nothing a screen reader can read; this keeps an undrawn scene
// beside them that speaks one line at a time, as KWin's own task switcher is
// spoken: the scene is made the focused window without the compositor's
// focus moving, and each new line takes the focus. Where keys and touches
// go never changes.
class SpokenLine
{
public:
    SpokenLine();
    ~SpokenLine();
    SpokenLine(const SpokenLine &) = delete;
    SpokenLine &operator=(const SpokenLine &) = delete;

    // Whether a screen reader is listening. Without one nothing is made.
    [[nodiscard]] static bool listening();

    // Speaks line, unless it is the line last spoken.
    void say(const QRect &geometry, const QString &line);
    void hide();
    [[nodiscard]] bool visible() const;

private:
    std::unique_ptr<KWin::OffscreenQuickScene> m_scene;
    QString m_line;
    QPointer<QWindow> m_previousFocus;
    bool m_focused = false;
};

} // namespace Kadunce
