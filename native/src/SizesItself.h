/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QString>
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

// Whether a launcher started the window's program, or the window says it is
// one a launcher starts: a Steam game, an Android app, RuneLite.
[[nodiscard]] inline bool startedByLauncher(const WindowOrigin &origin)
{
    if (origin.windowClass.startsWith(QStringLiteral("steam_app_"))
        || origin.app.startsWith(QStringLiteral("waydroid."))
        || origin.windowClass.contains(QStringLiteral("runelite"))
        || origin.app.contains(QStringLiteral("runelite")))
        return true;
    if (isLauncherWindow(origin)) return false;
    for (const QString &ancestor : origin.ancestors)
        if (isLauncherProgram(ancestor)) return true;
    return false;
}

// Whether Cards leaves the window at the size it gives itself. A choice made
// for its application stands over what Kadunce expects of it.
[[nodiscard]] inline bool sizesItself(const WindowOrigin &origin, std::optional<bool> chosen)
{
    return chosen.value_or(startedByLauncher(origin));
}
}
