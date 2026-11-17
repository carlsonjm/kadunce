/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QString>
#include <QList>
#include <QStringList>

#include <optional>

namespace Kadunce
{
// CARD-LIFECYCLE.md §2, A card that sizes itself: what is known of where a
// window came from, every name lower case.
struct WindowOrigin {
    // The desktop file or Wayland application id it names.
    QString app;
    // Its window class.
    QString windowClass;
    // The name of the program that owns the window.
    QString program;
    // The programs that started that one, nearest first.
    QStringList ancestors;
};

// A program's name as the process table gives it: lower case, with no
// directory and no ".sh".
[[nodiscard]] inline QString programName(QString name)
{
    name = name.trimmed().toLower().section(QLatin1Char('/'), -1);
    if (name.endsWith(QStringLiteral(".sh"))) name.chop(3);
    return name;
}

// Launchers whose own windows are ordinary cards and whose games and apps
// size themselves.
[[nodiscard]] inline bool isLauncherProgram(const QString &program)
{
    static const QStringList launchers{
        QStringLiteral("steam"), QStringLiteral("lutris"), QStringLiteral("heroic"),
        QStringLiteral("bolt"), QStringLiteral("bolt-launcher"), QStringLiteral("prismlauncher"),
        QStringLiteral("waydroid")};
    return launchers.contains(programName(program));
}

// Whether the window is one of the launchers' own: Steam draws its library
// in a helper it starts.
[[nodiscard]] inline bool isLauncherWindow(const WindowOrigin &origin)
{
    return isLauncherProgram(origin.program) || origin.windowClass == QStringLiteral("steam")
        || programName(origin.program) == QStringLiteral("steamwebhelper");
}

// The launcher a window's app belongs to, by the name its choice is kept
// under: a Steam game, an Android app and RuneLite say so themselves, else
// the nearest launcher that started its program. Empty for a launcher's own
// windows and for any app no launcher started.
[[nodiscard]] inline QString launcherFamily(const WindowOrigin &origin)
{
    if (origin.windowClass.startsWith(QStringLiteral("steam_app_"))) return QStringLiteral("steam");
    if (origin.app.startsWith(QStringLiteral("waydroid."))) return QStringLiteral("waydroid");
    if (origin.windowClass.contains(QStringLiteral("runelite")) || origin.app.contains(QStringLiteral("runelite")))
        return QStringLiteral("bolt");
    if (isLauncherWindow(origin)) return {};
    for (const QString &ancestor : origin.ancestors) {
        if (!isLauncherProgram(ancestor)) continue;
        const QString name = programName(ancestor);
        return name == QStringLiteral("bolt-launcher") ? QStringLiteral("bolt") : name;
    }
    return {};
}

// Whether a launcher started the window's program, or the window says it is
// one a launcher starts.
[[nodiscard]] inline bool startedByLauncher(const WindowOrigin &origin)
{
    return !launcherFamily(origin).isEmpty();
}

// The key a launcher's choice is kept under in kaduncerc [SizesItself], beside
// each app's own by its id.
[[nodiscard]] inline QString launcherChoiceKey(const QString &family)
{
    return family.isEmpty() ? QString() : QStringLiteral("launcher:") + family;
}

// What was found for one window: under which names, whether it sizes
// itself, and the processes from its own to the session's, nearest first.
struct SizingAnswer {
    QString named;
    bool sizes = false;
    QList<qint64> lineage;
};

// A window of an app that sizes itself, closed: the processes it came from,
// the display it stood on, and when, in milliseconds of a steady clock.
struct ClosedSizer {
    QList<qint64> lineage;
    QString display;
    qint64 at = 0;
};

// The display a new window of an app that sizes itself opens on: where a
// window of its own process stands, else where one of its process or of the
// ones that started it closed within `recent` milliseconds, else where a
// window of the nearest process that started it stands. `standing` names the
// display of an open window of a process, if one has one.
template<typename Standing>
[[nodiscard]] inline QString launchDisplay(const QList<qint64> &lineage, const QList<ClosedSizer> &closed,
                                           qint64 now, qint64 recent, Standing standing)
{
    if (lineage.isEmpty()) return {};
    if (const QString own = standing(lineage.first()); !own.isEmpty()) return own;
    for (auto it = closed.crbegin(); it != closed.crend(); ++it) {
        if (now - it->at > recent) continue;
        for (const qint64 pid : lineage)
            if (it->lineage.value(0) == pid) return it->display;
    }
    for (int index = 1; index < lineage.size(); ++index)
        if (const QString there = standing(lineage.at(index)); !there.isEmpty()) return there;
    return {};
}

// Whether Cards leaves the window at the size it gives itself. A choice made
// for its application stands over one made for its launcher, and either over
// what Kadunce expects of it.
[[nodiscard]] inline bool sizesItself(const WindowOrigin &origin, std::optional<bool> chosen,
                                      std::optional<bool> launcherChosen = std::nullopt)
{
    if (chosen) return *chosen;
    if (launcherChosen && startedByLauncher(origin)) return *launcherChosen;
    return startedByLauncher(origin);
}
}
