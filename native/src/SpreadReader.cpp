/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "SpreadReader.h"

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

SpreadReader::SpreadReader() = default;

SpreadReader::~SpreadReader()
{
    hide();
}

bool SpreadReader::listening()
{
    return QAccessible::isActive();
}

void SpreadReader::show(const QRect &geometry, const SpreadReading &reading)
{
    if (!m_scene) {
        m_scene = std::make_unique<KWin::OffscreenQuickScene>();
        // Never drawn: it is rendered only when asked to, and nothing asks.
        m_scene->setAutomaticRepaint(false);
        m_scene->setSource(QUrl(QStringLiteral("qrc:/kadunce/spread/SpreadReader.qml")));
        if (QQuickWindow *window = m_scene->window()) window->setTitle(QObject::tr("Spread"));
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
        root->setProperty("label", QObject::tr("Spread"));
        m_focused = true;
    }
    if (reading == m_reading) return;
    // The names first, so the focus lands on the chosen entry's new name.
    if (reading.names != m_reading.names || reading.places != m_reading.places) {
        root->setProperty("names", reading.names);
        root->setProperty("places", reading.places);
    }
    m_reading = reading;
    root->setProperty("current", reading.current);
    QMetaObject::invokeMethod(root, "focusCurrent");
}

void SpreadReader::hide()
{
    if (!m_scene) return;
    if (m_focused) {
        m_focused = false;
        // Table may have taken the focus since; it is then left with it.
        if (QGuiApplication::focusWindow() == m_scene->window())
            QWindowSystemInterface::handleFocusWindowChanged(m_previousFocus.data(), Qt::OtherFocusReason);
        m_previousFocus.clear();
    }
    m_reading = {};
    if (QQuickItem *root = m_scene->rootItem()) root->setProperty("current", -1);
    m_scene->hide();
}

bool SpreadReader::visible() const
{
    return m_scene && m_scene->isVisible();
}

} // namespace Kadunce
