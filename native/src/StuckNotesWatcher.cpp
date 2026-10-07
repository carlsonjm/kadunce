/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "StuckNotesWatcher.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QDebug>
#include <QVariantMap>

namespace Kadunce
{
namespace
{
const QString Service = QStringLiteral("io.github.carlsonjm.gooseberry");
const QString Path = QStringLiteral("/StuckNotes");
const QString Interface = QStringLiteral("io.github.carlsonjm.Gooseberry.StuckNotes");

QVariant plain(const QVariant &value);

// A D-Bus value as plain lists, maps and basic values.
QVariant demarshal(const QDBusArgument &argument)
{
    switch (argument.currentType()) {
    case QDBusArgument::BasicType:
    case QDBusArgument::VariantType:
        return plain(argument.asVariant());
    case QDBusArgument::ArrayType: {
        QVariantList list;
        argument.beginArray();
        while (!argument.atEnd()) list.append(demarshal(argument));
        argument.endArray();
        return list;
    }
    case QDBusArgument::MapType: {
        QVariantMap map;
        argument.beginMap();
        while (!argument.atEnd()) {
            argument.beginMapEntry();
            const QVariant key = demarshal(argument);
            const QVariant value = demarshal(argument);
            argument.endMapEntry();
            map.insert(key.toString(), value);
        }
        argument.endMap();
        return map;
    }
    case QDBusArgument::StructureType: {
        QVariantList fields;
        argument.beginStructure();
        while (!argument.atEnd()) fields.append(demarshal(argument));
        argument.endStructure();
        return fields;
    }
    case QDBusArgument::MapEntryType:
    case QDBusArgument::UnknownType:
        break;
    }
    return {};
}

QVariant plain(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QDBusVariant>())
        return plain(value.value<QDBusVariant>().variant());
    if (value.metaType() == QMetaType::fromType<QDBusArgument>())
        return demarshal(value.value<QDBusArgument>());
    if (value.metaType() == QMetaType::fromType<QVariantList>()) {
        QVariantList list;
        for (const QVariant &item : value.toList()) list.append(plain(item));
        return list;
    }
    return value;
}

QVariantList windowsOf(const QDBusMessage &message)
{
    const QList<QVariant> arguments = message.arguments();
    return arguments.isEmpty() ? QVariantList() : plain(arguments.first()).toList();
}
} // namespace

StuckNotesWatcher::StuckNotesWatcher(QObject *parent)
    : QObject(parent)
{
    auto bus = QDBusConnection::sessionBus();
    m_watcher = new QDBusServiceWatcher(Service, bus,
        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, [this] { fetch(); });
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] { vanish(); });
    // Matched by path and interface from any sender, since naming the service
    // here would have the bus library look up its owner while waiting.
    bus.connect(QString(), Path, Interface, QStringLiteral("WindowsChanged"),
                this, SLOT(receiveWindowsChanged(QDBusMessage)));
    // Gooseberry may already be running; a call to a name nobody owns comes
    // back as an error, and never starts it.
    fetch();
}

void StuckNotesWatcher::fetch()
{
    auto message = QDBusMessage::createMethodCall(Service, Path, Interface, QStringLiteral("Windows"));
    message.setAutoStartService(false);
    const quint64 generation = ++m_generation;
    auto *call = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *finished) {
        finished->deleteLater();
        if (generation != m_generation) return;
        const QDBusMessage reply = finished->reply();
        if (reply.type() != QDBusMessage::ReplyMessage) {
            if (!m_owner.isEmpty()) vanish();
            return;
        }
        if (m_owner != reply.service())
            qInfo() << "Kadunce reads stuck notes from Gooseberry at" << reply.service();
        m_owner = reply.service();
        Q_EMIT windowsChanged(windowsOf(reply));
    });
}

void StuckNotesWatcher::receiveWindowsChanged(const QDBusMessage &message)
{
    if (m_owner.isEmpty() || message.service() != m_owner) {
        // Someone new speaks for Gooseberry, or it started before it was
        // seen: ask it directly rather than trust an unknown sender.
        fetch();
        return;
    }
    ++m_generation;
    Q_EMIT windowsChanged(windowsOf(message));
}

void StuckNotesWatcher::vanish()
{
    ++m_generation;
    const bool had = !m_owner.isEmpty();
    m_owner.clear();
    if (had) qInfo() << "Kadunce no longer sees Gooseberry's stuck notes";
    Q_EMIT windowsChanged({});
}

void StuckNotesWatcher::send(const QString &method, const QVariantList &arguments)
{
    if (m_owner.isEmpty()) return;
    auto message = QDBusMessage::createMethodCall(Service, Path, Interface, method);
    message.setAutoStartService(false);
    message.setArguments(arguments);
    QDBusConnection::sessionBus().asyncCall(message);
}

void StuckNotesWatcher::hide()
{
    send(QStringLiteral("Hide"), {});
}

void StuckNotesWatcher::stickTo(const QString &noteId, const QString &windowId,
                                const QString &caption, const QString &app)
{
    qInfo() << "Kadunce asks Gooseberry to stick note" << noteId << "to" << windowId;
    send(QStringLiteral("StickTo"), {noteId, windowId, caption, app});
}
} // namespace Kadunce
