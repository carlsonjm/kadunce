/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QString>
#include <QStringList>

namespace Kadunce
{
// Who may drive Kadunce over D-Bus: bring a window forward, raise the keys,
// open Spread, Table or a layout, or place an application
// (TETTEGOUCHE-CONTEXT.md § Who may call). Asking what Kadunce shows, claiming
// an edge or a context line for oneself, and the settings stay open to any
// caller; none of them brings anything forward.
//
// A caller is known by the program it runs, as its process names it: one of
// the programs the suite's surfaces live in, installed for the whole system
// or in the person's own programs folder. A copy under any other name or in
// any other folder is a stranger.
[[nodiscard]] inline QStringList programsThatMayDrive()
{
    return {
        QStringLiteral("plasmashell"),
        QStringLiteral("kscreenlocker_greet"),
        QStringLiteral("tettegouche"),
        QStringLiteral("shuffle-keyboard"),
        QStringLiteral("plasma-keyboard"),
    };
}

// `program` is the target of /proc/<pid>/exe; a program replaced on disk
// while it runs, as an update does, still counts as the one it was.
[[nodiscard]] inline bool programMayDrive(QString program, const QString &home)
{
    static const QString replaced = QStringLiteral(" (deleted)");
    if (program.endsWith(replaced)) program.chop(replaced.size());
    if (!program.startsWith(QLatin1Char('/')) || program.contains(QStringLiteral("/../"))
        || program.contains(QStringLiteral("/./")))
        return false;
    const qsizetype slash = program.lastIndexOf(QLatin1Char('/'));
    const QString folder = program.left(slash + 1);
    const QString name = program.mid(slash + 1);
    if (!programsThatMayDrive().contains(name)) return false;
    if (folder.startsWith(QStringLiteral("/usr/"))) return true;
    return !home.isEmpty() && folder == home + QStringLiteral("/.local/bin/");
}
} // namespace Kadunce
