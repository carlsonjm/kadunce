/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "TablePresenter.h"
#include "MotionTime.h"

#include <effect/effecthandler.h>
#include <effect/offscreenquickview.h>

#include <QFont>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMetaMethod>
#include <QQuickItem>
#include <QQuickWindow>
#include <QUrl>
#include <qpa/qwindowsysteminterface.h>

namespace Kadunce
{

// Long enough for Table.qml's 260 ms pour to finish, at the default speed.
constexpr int ClosingMs = 300;

TablePresenter::TablePresenter()
{
    m_closing.setSingleShot(true);
    m_closing.setInterval(ClosingMs);
    QObject::connect(&m_closing, &QTimer::timeout, &m_closing, [this] { hide(); });
    m_typed.setSingleShot(true);
    m_typed.setInterval(0);
    QObject::connect(&m_typed, &QTimer::timeout, &m_typed, [this] {
        if (m_editing && m_onTyped) m_onTyped(editedText());
    });
}

TablePresenter::~TablePresenter() = default;

void TablePresenter::show(const QRect &geometry, qreal scale, const QVariantMap &model)
{
    if (!m_scene) {
        m_scene = std::make_unique<KWin::OffscreenQuickScene>();
        // Rendered in the compositor's own frames, as KWin's Overview is. A
        // view left to repaint itself waits for 10 ms without a change, which
        // a moving finger never leaves, so a carried card trailed it.
        m_scene->setAutomaticRepaint(false);
        const auto changed = [this] { m_dirty = true; };
        QObject::connect(m_scene.get(), &KWin::OffscreenQuickView::renderRequested, m_scene.get(), changed);
        QObject::connect(m_scene.get(), &KWin::OffscreenQuickView::sceneChanged, m_scene.get(), changed);
        m_scene->setSource(QUrl(QStringLiteral("qrc:/kadunce/table/Table.qml")),
                           {{QStringLiteral("model"), model}});
    }
    m_closing.stop();
    m_scene->setDevicePixelRatio(scale);
    if (m_scene->geometry() != KWin::Rect(geometry)) m_scene->setGeometry(KWin::Rect(geometry));
    m_scene->show();
    if (QQuickItem *root = m_scene->rootItem()) {
        root->setProperty("model", model);
        root->setProperty("motionFactor", KWin::effects ? KWin::effects->animationTimeFactor() : 1.0);
        root->setProperty("shown", true);
    }
}

void TablePresenter::close()
{
    if (!m_scene || !m_scene->isVisible() || m_closing.isActive()) return;
    if (QQuickItem *root = m_scene->rootItem()) root->setProperty("shown", false);
    m_closing.start(motionDuration(ClosingMs, KWin::effects ? KWin::effects->animationTimeFactor() : 1.0));
}

void TablePresenter::hide()
{
    endEditing();
    m_closing.stop();
    if (!m_scene) return;
    if (QQuickItem *root = m_scene->rootItem()) root->setProperty("shown", false);
    m_scene->hide();
}

bool TablePresenter::visible() const
{
    return m_scene && m_scene->isVisible();
}

void TablePresenter::prePaint(KWin::OutputFrame *frame)
{
    if (!m_scene || !m_dirty || !m_scene->isVisible()) return;
    m_dirty = false;
    m_scene->update(frame);
}

void TablePresenter::beginEditing(const QString &text, std::function<void(const QString &)> typed)
{
    QQuickItem *root = m_scene ? m_scene->rootItem() : nullptr;
    if (!root) return;
    m_onTyped = std::move(typed);
    if (!m_editing) {
        // Table's scene has no window of its own for the platform to focus,
        // so it is made the focused window directly, as KWin's own scenes
        // are when their search field takes the keys. The keyboard's text
        // goes to the focused window's focused item.
        m_previousFocus = QGuiApplication::focusWindow();
        QWindowSystemInterface::handleFocusWindowChanged(m_scene->window(), Qt::OtherFocusReason);
        // QTimer::start() is a slot, so the QML property's change signal can
        // reach it without a QObject of Table's own.
        const QMetaObject *meta = root->metaObject();
        const QMetaMethod changed = meta->property(meta->indexOfProperty("renameText")).notifySignal();
        const QMetaMethod start = m_typed.metaObject()->method(m_typed.metaObject()->indexOfSlot("start()"));
        m_typedConnection = QObject::connect(root, changed, &m_typed, start);
    }
    m_editing = true;
    QMetaObject::invokeMethod(root, "beginEditing", Q_ARG(QVariant, text));
}

void TablePresenter::endEditing()
{
    if (!m_editing) return;
    m_editing = false;
    m_onTyped = nullptr;
    m_typed.stop();
    QObject::disconnect(m_typedConnection);
    if (QQuickItem *root = m_scene ? m_scene->rootItem() : nullptr) QMetaObject::invokeMethod(root, "endEditing");
    if (m_scene && QGuiApplication::focusWindow() == m_scene->window())
        QWindowSystemInterface::handleFocusWindowChanged(m_previousFocus.data(), Qt::OtherFocusReason);
    m_previousFocus.clear();
}

QString TablePresenter::editedText() const
{
    const QQuickItem *root = m_scene ? m_scene->rootItem() : nullptr;
    return root ? root->property("renameText").toString() : QString();
}

bool TablePresenter::hasTextFocus() const
{
    const QObject *focus = QGuiApplication::focusObject();
    return m_editing && m_scene && QGuiApplication::focusWindow() == m_scene->window() && focus
        && focus->inherits("QQuickTextInput");
}

void TablePresenter::forwardKey(QKeyEvent *event)
{
    if (m_scene && m_editing) m_scene->forwardKeyEvent(event);
}

double TablePresenter::measure(const QString &text, double pixelSize, bool medium)
{
    QFont font;
    font.setPixelSize(int(pixelSize));
    font.setWeight(medium ? QFont::Medium : QFont::Normal);
    return QFontMetricsF(font).horizontalAdvance(text);
}

} // namespace Kadunce
