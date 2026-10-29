/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <KLocalizedString>
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

// Names in a line, the last after "and": "Notes", "Notes and Files",
// "Notes, Files and Kate". Empty names are left out.
[[nodiscard]] inline QString namesInLine(const QStringList &names)
{
    QStringList kept;
    for (const auto &name : names) {
        const QString trimmed = name.trimmed();
        if (!trimmed.isEmpty()) kept.append(trimmed);
    }
    if (kept.size() <= 1) return kept.value(0);
    const QString last = kept.takeLast();
    return i18n("%1 and %2", kept.join(QStringLiteral(", ")), last);
}

// A pane's card names the layout it belongs to: the card's own application,
// then the applications it shares the layout with, in the layout's order.
[[nodiscard]] inline QString pairedApplicationName(
    const QString &applicationName, const QStringList &partnerNames)
{
    const QString partners = namesInLine(partnerNames);
    return partners.isEmpty() ? applicationName
                              : i18n("%1 · with %2", applicationName, partners);
}

// A Stack's face names the cards waiting behind it, nearest first, as many as
// Spread shows: what the panes beside it already show is not repeated.
[[nodiscard]] inline QString stackedApplicationName(
    const QString &applicationName, const QStringList &behindNames)
{
    const QString behind = namesInLine(behindNames);
    return behind.isEmpty() ? applicationName
                            : i18n("%1 · then %2", applicationName, behind);
}

[[nodiscard]] inline QString stackPositionLabel(int position, int count)
{
    return count > 1 && position >= 0 && position < count
        ? QStringLiteral("%1 / %2").arg(position + 1).arg(count)
        : QString();
}

} // namespace Kadunce
