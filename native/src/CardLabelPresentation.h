/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace Kadunce
{

struct CardLabelPresentation {
    QString applicationName;
    QString stackPosition;
};

[[nodiscard]] inline QString humanApplicationName(
    const QString &desktopServiceName, const QString &resourceClass,
    const QString &windowClass, const QString &caption)
{
    for (const auto &candidate : {desktopServiceName, resourceClass,
                                  windowClass, caption}) {
        const QString name = candidate.trimmed();
        if (!name.isEmpty()) return name;
    }
    return {};
}

// A pane's card names the layout it belongs to: the card's own application,
// then the applications it shares the layout with, in the layout's order.
[[nodiscard]] inline QString pairedApplicationName(
    const QString &applicationName, const QStringList &partnerNames)
{
    QStringList partners;
    for (const auto &name : partnerNames) {
        const QString trimmed = name.trimmed();
        if (!trimmed.isEmpty()) partners.append(trimmed);
    }
    if (partners.isEmpty()) return applicationName;
    if (partners.size() == 1)
        return QObject::tr("%1 · with %2").arg(applicationName, partners.first());
    const QString last = partners.takeLast();
    return QObject::tr("%1 · with %2 and %3")
        .arg(applicationName, partners.join(QStringLiteral(", ")), last);
}

[[nodiscard]] inline QString stackPositionLabel(int position, int count)
{
    return count > 1 && position >= 0 && position < count
        ? QStringLiteral("%1 / %2").arg(position + 1).arg(count)
        : QString();
}

} // namespace Kadunce
