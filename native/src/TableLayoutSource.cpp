// SPDX-License-Identifier: GPL-2.0-or-later
#include "TableLayoutSource.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDebug>

#include <algorithm>

namespace Kadunce
{

namespace
{
// Split Rock Core, as Search and Settings call it (split-rock docs/CORE.md).
const QString CoreService = QStringLiteral("io.github.carlsonjm.splitrock.Core");
const QString CorePath = QStringLiteral("/Core");
const QString CoreInterface = QStringLiteral("io.github.carlsonjm.SplitRock.Core");
// Opening waits on Cards placing each app, so Core answers in its own time.
constexpr int OpenTimeoutMs = 60000;

// An aa{sv} as it comes off the bus, or as a list already unpacked.
QList<QVariantMap> mapsOf(const QVariant &value)
{
    QList<QVariantMap> list;
    if (value.canConvert<QDBusArgument>()) {
        const QDBusArgument argument = value.value<QDBusArgument>();
        if (argument.currentType() != QDBusArgument::ArrayType) return list;
        argument.beginArray();
        while (!argument.atEnd()) {
            QVariantMap map;
            argument >> map;
            list.append(map);
        }
        argument.endArray();
        return list;
    }
    for (const QVariant &item : value.toList()) list.append(item.toMap());
    return list;
}
}

TableLayoutSource::TableLayoutSource(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    m_watcher = new QDBusServiceWatcher(CoreService, bus,
        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, &TableLayoutSource::read);
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] {
        ++m_reads;
        readAnswered({});
    });
    bus.connect(CoreService, CorePath, CoreInterface, QStringLiteral("LayoutsChanged"), this, SLOT(read()));
    read();
}

void TableLayoutSource::read()
{
    const int reading = ++m_reads;
    const QDBusMessage message = QDBusMessage::createMethodCall(CoreService, CorePath, CoreInterface, QStringLiteral("Layouts"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, reading](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        if (reading != m_reads) return;
        const QDBusMessage reply = call->reply();
        // Absent or broken, Core has no Layouts to offer.
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            readAnswered({});
            return;
        }
        QList<TableSavedLayout> layouts;
        for (const QVariantMap &layout : mapsOf(reply.arguments().constFirst())) {
            TableSavedLayout read = tableSavedLayout(layout.value(QStringLiteral("id")).toString(),
                layout.value(QStringLiteral("name")).toString(), mapsOf(layout.value(QStringLiteral("groups"))));
            if (!read.id.isEmpty() && !read.name.isEmpty() && read.windows > 0) layouts.append(read);
        }
        readAnswered(layouts);
    });
}

void TableLayoutSource::readAnswered(const QList<TableSavedLayout> &layouts)
{
    const bool same = layouts.size() == m_layouts.size()
        && std::equal(layouts.cbegin(), layouts.cend(), m_layouts.cbegin(), [](const auto &a, const auto &b) {
               return a.id == b.id && a.name == b.name && a.frontApp == b.frontApp && a.windows == b.windows
                   && a.displays == b.displays && a.pictured == b.pictured && a.picturedApps == b.picturedApps;
           });
    if (same) return;
    m_layouts = layouts;
    qInfo() << "Kadunce Table has" << m_layouts.size() << "saved Layouts to hang under +";
    Q_EMIT changed();
}

void TableLayoutSource::open(const QString &id, std::function<void(bool, const QString &)> answered)
{
    QDBusMessage message = QDBusMessage::createMethodCall(CoreService, CorePath, CoreInterface, QStringLiteral("OpenLayout"));
    message << id;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, OpenTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [answered](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<bool> reply = *call;
        if (reply.isError()) answered(false, reply.error().message());
        else answered(reply.value(), QString());
    });
}

} // namespace Kadunce
