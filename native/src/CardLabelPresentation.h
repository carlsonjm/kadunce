/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

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

[[nodiscard]] inline QString bentoApplicationNames(
    const QStringList &visiblePaneNames)
{
    QStringList names;
    names.reserve(visiblePaneNames.size());
    for (const auto &name : visiblePaneNames) {
        const QString trimmed = name.trimmed();
        if (!trimmed.isEmpty()) names.append(trimmed);
    }
    return names.join(QStringLiteral(" · "));
}

[[nodiscard]] inline QString stackPositionLabel(int position, int count,
                                                bool bentoGroup)
{
    return !bentoGroup && count > 1 && position >= 0 && position < count
        ? QStringLiteral("%1 / %2").arg(position + 1).arg(count)
        : QString();
}

} // namespace Kadunce
