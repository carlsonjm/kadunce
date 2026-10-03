/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QTimer>

#include <QPointer>
#include <QRect>
#include <QString>
#include <QVariantMap>

#include <functional>
#include <memory>

class QKeyEvent;
class QWindow;

namespace KWin
{
class OffscreenQuickScene;
class OutputFrame;
}

namespace Kadunce
{

// Draws Table (table/Table.qml) over the display that holds it, as KWin draws
// its own overlays: above every window, taking no input. Kadunce hands it the
// whole picture each time something changes.
class TablePresenter
{
public:
    TablePresenter();
    ~TablePresenter();
    TablePresenter(const TablePresenter &) = delete;
    TablePresenter &operator=(const TablePresenter &) = delete;

    void show(const QRect &geometry, qreal scale, const QVariantMap &model);
    // Table closed: the band and the rows pour back out, then the scene hides.
    void close();
    void hide();
    [[nodiscard]] bool visible() const;
    // Renders what changed, in the frame being prepared for Table's display.
    void prePaint(KWin::OutputFrame *frame);

    // How wide text is as Table draws it, at a pixel size, medium or regular.
    static double measure(const QString &text, double pixelSize, bool medium);

    // A tab's name is being typed, starting from text: Table takes the
    // compositor's text focus, so the keys and the keyboard's text reach it,
    // and says each change to typed.
    void beginEditing(const QString &text, std::function<void(const QString &)> typed);
    void endEditing();
    [[nodiscard]] QString editedText() const;
    // Whether the name's field is where the compositor's text goes now.
    [[nodiscard]] bool hasTextFocus() const;
    // A key from the keyboard Table holds, for the name being typed.
    void forwardKey(QKeyEvent *event);

private:
    std::unique_ptr<KWin::OffscreenQuickScene> m_scene;
    bool m_dirty = false;
    QTimer m_closing;
    // The name's changes, gathered and said once a turn.
    QTimer m_typed;
    std::function<void(const QString &)> m_onTyped;
    QMetaObject::Connection m_typedConnection;
    QPointer<QWindow> m_previousFocus;
    bool m_editing = false;
};

} // namespace Kadunce
