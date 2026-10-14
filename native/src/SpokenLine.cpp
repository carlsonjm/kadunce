/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "SpokenLine.h"

#include <effect/offscreenquickview.h>

#include <QAccessible>
#include <QGuiApplication>
#include <QObject>
#include <QQuickItem>
#include <QQuickWindow>
#include <QUrl>
#include <qpa/qwindowsysteminterface.h>

namespace Kadunce
{

SpokenLine::SpokenLine() = default;

SpokenLine::~SpokenLine()
{
    hide();
}

bool SpokenLine::listening()
{
    return QAccessible::isActive();
}

void SpokenLine::say(const QRect &geometry, const QString &line)
{
    if (!m_scene) {
        m_scene = std::make_unique<KWin::OffscreenQuickScene>();
        // Never drawn: it is rendered only when asked to, and nothing asks.
        m_scene->setAutomaticRepaint(false);
        m_scene->setSource(QUrl(QStringLiteral("qrc:/kadunce/spoken/SpokenLine.qml")));
    }
    QQuickItem *root = m_scene->rootItem();
    if (!root) return;
    if (m_scene->geometry() != KWin::Rect(geometry)) m_scene->setGeometry(KWin::Rect(geometry));
    m_scene->show();
    if (!m_focused) {
        // The screen reader follows the focused window's focused item. The
        // compositor's own focus, and where keys go, stay as they were.
        QWindow *previous = QGuiApplication::focusWindow();
        m_previousFocus = previous == m_scene->window() ? nullptr : previous;
        QWindowSystemInterface::handleFocusWindowChanged(m_scene->window(), Qt::OtherFocusReason);
        m_focused = true;
    }
    if (line == m_line) return;
    m_line = line;
    QMetaObject::invokeMethod(root, "say", Q_ARG(QVariant, line));
}

void SpokenLine::hide()
{
    if (!m_scene) return;
    if (m_focused) {
        m_focused = false;
        // Table's name field may have taken the focus since; it keeps it.
        if (QGuiApplication::focusWindow() == m_scene->window())
            QWindowSystemInterface::handleFocusWindowChanged(m_previousFocus.data(), Qt::OtherFocusReason);
        m_previousFocus.clear();
    }
    m_line.clear();
    if (QQuickItem *root = m_scene->rootItem()) QMetaObject::invokeMethod(root, "reset");
    m_scene->hide();
}

bool SpokenLine::visible() const
{
    return m_scene && m_scene->isVisible();
}

} // namespace Kadunce
