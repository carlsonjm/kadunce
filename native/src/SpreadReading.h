/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

namespace Kadunce
{

// One Spread entry as a screen reader hears it.
struct SpreadReadingEntry {
    QString application;
    QString title;
    // Cards in the entry's Stack, its face included; 1 for a card alone.
    int stacked = 1;
    bool sleeping = false;
    // The applications in the Bento group this entry is, in pane order;
    // empty for a card or a Stack.
    QStringList group;
};

// What Spread says for an entry, as Table names its cards: the application
// and the window's title, then whether it is a Stack and how many cards it
// holds, and whether the card is sleeping. The Bento group is named by the
// applications laid out in it.
inline QString spreadEntryName(const SpreadReadingEntry &entry)
{
    QStringList parts;
    if (!entry.group.isEmpty()) {
        QStringList applications;
        for (const QString &application : entry.group)
            if (!application.isEmpty()) applications.append(application);
        parts.append(QObject::tr("Bento group"));
        if (!applications.isEmpty()) parts.append(applications.join(QObject::tr(" and ")));
        return parts.join(QStringLiteral(", "));
    }
    if (!entry.application.isEmpty()) parts.append(entry.application);
    if (!entry.title.isEmpty() && entry.title != entry.application) parts.append(entry.title);
    if (entry.stacked > 1) parts.append(QObject::tr("Stack of %1 cards").arg(entry.stacked));
    if (entry.sleeping) parts.append(QObject::tr("sleeping"));
    if (parts.isEmpty()) parts.append(QObject::tr("Card"));
    return parts.join(QStringLiteral(", "));
}

// Where an entry stands in the row, counted from the row's first entry.
inline QString spreadEntryPlace(int index, int count)
{
    return QObject::tr("%1 of %2").arg(index + 1).arg(count);
}

// What a screen reader says for Spread's chosen entry: its name and where it
// stands in the row, or nothing when the choice is outside the row.
inline QString spreadLine(const QList<SpreadReadingEntry> &entries, int selected)
{
    if (selected < 0 || selected >= entries.size()) return QString();
    return QObject::tr("%1, %2").arg(spreadEntryName(entries.at(selected)), spreadEntryPlace(selected, entries.size()));
}

} // namespace Kadunce
