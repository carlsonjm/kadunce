#pragma once

#include <core/output.h>
#include <window.h>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <vector>

namespace Kadunce {
// Owned by the caller, never by the client. Destroying this observer disconnects
// every callback before effect unload. It only minimizes; never retries geometry.
// Cancellation deliberately leaves native visibility unchanged.
class RestoredMinimization final : public QObject
{
public:
    enum class Result { Pending, Minimized, Canceled };
    struct Target {
        KWin::RectF geometry;
        KWin::MaximizeMode maximizeMode;
        KWin::QuickTileMode quickTileMode;
        bool fullScreen;
    };
    RestoredMinimization(KWin::Window *client, const Target &target)
        : m_client(client), m_output(client ? client->output() : nullptr), m_target(target)
    {
        if (!client || !m_output || !target.geometry.isValid()) { cancel(); return; }
        const auto observe = [this] { check(); };
        m_connections.push_back(connect(client, &KWin::Window::frameGeometryChanged,
            this, observe, Qt::QueuedConnection));
        m_connections.push_back(connect(client, &KWin::Window::maximizedChanged,
            this, observe, Qt::QueuedConnection));
        m_connections.push_back(connect(client, &KWin::Window::fullScreenChanged,
            this, observe, Qt::QueuedConnection));
        m_connections.push_back(connect(client, &KWin::Window::closed, this, [this] { cancel(); }));
        m_connections.push_back(connect(client, &KWin::Window::interactiveMoveResizeStarted,
            this, [this] { cancel(); }));
        m_connections.push_back(connect(client, &KWin::Window::outputChanged,
            this, [this] { cancel(); }));
        m_connections.push_back(connect(client, &KWin::Window::activeChanged,
            this, [this] { if (m_client && m_client->isActive()) cancel(); }));
        m_connections.push_back(connect(m_output, &QObject::destroyed, this, [this] { cancel(); }));
        // This deadline only cancels. It never substitutes elapsed time for a
        // successful size/state observation or triggers a second resize.
        m_deadline.setSingleShot(true);
        connect(&m_deadline, &QTimer::timeout, this, [this] { cancel(); });
        m_deadline.start(2000);
        QTimer::singleShot(0, this, observe);
    }
    ~RestoredMinimization() override { cancel(); }
    Result result() const { return m_result; }
    KWin::Window *client() const { return m_client; }
    KWin::LogicalOutput *output() const { return m_output; }
    void cancel() { if (m_result == Result::Pending) finish(Result::Canceled); }
private:
    void finish(Result result) {
        m_result = result;
        m_deadline.stop();
        for (const auto &connection : m_connections) QObject::disconnect(connection);
        m_connections.clear();
    }
    void check() {
        if (m_result != Result::Pending) return;
        if (!m_client || !m_output || m_client->output() != m_output
            || m_client->isInteractiveMove() || m_client->isInteractiveResize()) { cancel(); return; }
        // A changed requested destination is a newer native/user operation.
        if (m_client->moveResizeGeometry() != m_target.geometry
            || m_client->requestedMaximizeMode() != m_target.maximizeMode
            || m_client->isRequestedFullScreen() != m_target.fullScreen) { cancel(); return; }
        if (m_client->frameGeometry() != m_target.geometry
            || m_client->maximizeMode() != m_target.maximizeMode
            || m_client->quickTileMode() != m_target.quickTileMode
            || m_client->isFullScreen() != m_target.fullScreen) return;
        QPointer<KWin::Window> client = m_client;
        // Retire before the setter: minimizedChanged can synchronously destroy
        // this owner or begin another workspace transaction.
        finish(Result::Minimized);
        client->setMinimized(true);
    }
    QPointer<KWin::Window> m_client;
    QPointer<KWin::LogicalOutput> m_output;
    Target m_target;
    Result m_result = Result::Pending;
    QTimer m_deadline;
    std::vector<QMetaObject::Connection> m_connections;
};
}
