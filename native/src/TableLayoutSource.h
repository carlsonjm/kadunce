// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "TableSavedLayouts.h"

#include <QList>
#include <QObject>
#include <QString>

#include <functional>

class QDBusServiceWatcher;

namespace Kadunce
{

// The saved Layouts Split Rock Core keeps, for the cards under Table's +.
// Read from Core's Layouts() whenever Core starts or says they changed, and
// never waited for: every call is asynchronous, so the compositor goes on
// drawing. Without Split Rock there are none, and + is as it always was.
class TableLayoutSource : public QObject
{
    Q_OBJECT
public:
    explicit TableLayoutSource(QObject *parent = nullptr);

    [[nodiscard]] const QList<TableSavedLayout> &layouts() const { return m_layouts; }
    // Asks Core to open the Layout on the current workspace. answered runs
    // once with Core's answer, false when Core refused it or never answered.
    void open(const QString &id, std::function<void(bool, const QString &)> answered);

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void read();

private:
    void readAnswered(const QList<TableSavedLayout> &layouts);

    QDBusServiceWatcher *m_watcher = nullptr;
    QList<TableSavedLayout> m_layouts;
    // A later read wins over an earlier one still on its way.
    int m_reads = 0;
};

} // namespace Kadunce
