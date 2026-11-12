/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "SearchTyping.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDebug>

#include <functional>

namespace Kadunce
{
namespace
{
const QString Service = QStringLiteral("io.github.carlsonjm.Tettegouche");
const QString Path = QStringLiteral("/Launcher");
const QString Interface = QStringLiteral("io.github.carlsonjm.Tettegouche");
// Long enough for Search to open and take the keys, short enough that a
// Search that never opened does not keep taking what is typed.
constexpr qint64 CarryMs = 1500;

void call(const QString &method, const QVariantList &arguments, QObject *context,
          std::function<void(const QDBusMessage &)> answered = {})
{
    auto message = QDBusMessage::createMethodCall(Service, Path, Interface, method);
    message.setArguments(arguments);
    message.setAutoStartService(false);
    if (!answered) {
        QDBusConnection::sessionBus().send(message);
        return;
    }
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context,
        [answered = std::move(answered)](QDBusPendingCallWatcher *finished) {
            finished->deleteLater();
            answered(finished->reply());
        });
}
} // namespace

SearchTyping::SearchTyping(QObject *parent)
    : QObject(parent)
{
    auto bus = QDBusConnection::sessionBus();
    m_watcher = new QDBusServiceWatcher(Service, bus,
        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, [this] { setPresent(true); });
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] { setPresent(false); });
    // Tettegouche may already be running; asked of the bus itself, so it is
    // never started.
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("NameHasOwner"));
    message.setArguments({Service});
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *finished) {
        finished->deleteLater();
        const QDBusPendingReply<bool> reply = *finished;
        if (reply.isValid() && reply.value()) setPresent(true);
    });
}

void SearchTyping::setPresent(bool present)
{
    m_present = present;
    m_sent.invalidate();
}

bool SearchTyping::carrying() const
{
    return m_sent.isValid() && m_sent.elapsed() < CarryMs;
}

void SearchTyping::type(const QString &text)
{
    const bool first = !carrying();
    if (first) {
        m_openedPlain = false;
        qInfo() << "Kadunce opens Search in Spread with what was typed";
    }
    m_sent.start();
    // Sent one after another on one connection, the pieces arrive in order.
    call(QStringLiteral("typeToSearch"), {text}, this, [this](const QDBusMessage &reply) {
        if (reply.type() != QDBusMessage::ErrorMessage
            || reply.errorName() != QLatin1String("org.freedesktop.DBus.Error.UnknownMethod")
            || m_openedPlain) {
            return;
        }
        // A Tettegouche from before typeToSearch opens as its key opens it,
        // ready to type, without the first letters.
        m_openedPlain = true;
        call(QStringLiteral("toggleToType"), {}, this);
    });
}
} // namespace Kadunce
