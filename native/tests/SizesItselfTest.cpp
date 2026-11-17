// SPDX-License-Identifier: GPL-2.0-or-later
#include "SizesItself.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
WindowOrigin origin(const char *app, const char *windowClass, const char *program,
                    std::initializer_list<const char *> ancestors = {}) {
    QStringList started;
    for (const char *name : ancestors) started.append(QString::fromLatin1(name));
    return {QString::fromLatin1(app), QString::fromLatin1(windowClass), QString::fromLatin1(program), started};
}
int main() {
    // A game Steam started, found by its class or by who started it.
    check(sizesItself(origin("", "steam_app_1172470", "r5apex.exe"), std::nullopt), "a Steam game by class does not size itself");
    check(sizesItself(origin("", "game.x86_64", "game.x86_64", {"reaper", "steam", "systemd"}), std::nullopt),
          "a game started under Steam does not size itself");
    // Steam's own windows, its library drawn by a helper it starts, stay cards.
    check(!sizesItself(origin("steam", "steam", "steamwebhelper", {"steam", "steam.sh", "systemd"}), std::nullopt),
          "Steam's library sizes itself");
    check(!sizesItself(origin("", "", "steam"), std::nullopt), "Steam itself sizes itself");
    // RuneLite under Bolt, and RuneLite on its own.
    check(sizesItself(origin("", "net-runelite-client-runelite", "java", {"bolt", "bwrap"}), std::nullopt),
          "RuneLite from Bolt does not size itself");
    check(sizesItself(origin("net.runelite.runelite", "runelite", "runelite"), std::nullopt), "RuneLite does not size itself");
    check(!sizesItself(origin("com.adamcake.bolt", "bolt", "bolt"), std::nullopt), "Bolt sizes itself");
    // Minecraft from Prism, a game from Lutris or Heroic.
    check(sizesItself(origin("", "minecraft 1.21", "java", {"prismlauncher"}), std::nullopt), "Minecraft does not size itself");
    check(sizesItself(origin("", "game.exe", "game.exe", {"wineserver", "lutris-wrapper", "lutris"}), std::nullopt),
          "a Lutris game does not size itself");
    check(sizesItself(origin("", "game.exe", "game.exe", {"heroic"}), std::nullopt), "a Heroic game does not size itself");
    // Android apps.
    check(sizesItself(origin("waydroid.org.mozilla.firefox", "", "lxc-start"), std::nullopt), "an Android app does not size itself");
    // Anything else is a card unless chosen.
    const auto firefox = origin("org.mozilla.firefox", "firefox", "firefox", {"plasmashell", "systemd"});
    check(!sizesItself(firefox, std::nullopt), "Firefox sizes itself");
    check(sizesItself(firefox, true), "a chosen app does not size itself");
    check(!sizesItself(origin("", "steam_app_1", "game"), false), "a game switched off sizes itself");
    check(programName(QStringLiteral("/usr/bin/Steam.sh")) == QStringLiteral("steam"), "a program's name keeps its path");
    // Where a new window of an app that sizes itself opens. RuneLite's
    // process 30 was started by its launcher, 20, under Bolt, 10.
    const QList<qint64> client{30, 20, 10, 2};
    const auto standing = [](std::initializer_list<std::pair<qint64, const char *>> open) {
        return [open](qint64 pid) {
            for (const auto &[at, display] : open) if (at == pid) return QString::fromLatin1(display);
            return QString();
        };
    };
    check(launchDisplay(client, {}, 0, 15000, standing({{10, "DP-2"}})) == QStringLiteral("DP-2"),
          "a window does not open where its launcher stands");
    check(launchDisplay(client, {}, 0, 15000, standing({{10, "DP-2"}, {30, "eDP-1"}})) == QStringLiteral("eDP-1"),
          "a window does not open beside its own process's other window");
    const QList<ClosedSizer> closed{{{30, 20, 10, 2}, QStringLiteral("DP-2"), 1000}};
    check(launchDisplay(client, closed, 5000, 15000, standing({{10, "eDP-1"}})) == QStringLiteral("DP-2"),
          "a window built again does not open where its last one closed");
    check(launchDisplay(client, closed, 30000, 15000, standing({})).isEmpty(), "a long-closed window still decides");
    const QList<ClosedSizer> splash{{{20, 10, 2}, QStringLiteral("DP-2"), 1000}};
    check(launchDisplay(client, splash, 2000, 15000, standing({})) == QStringLiteral("DP-2"),
          "the client does not open where its launcher's window closed");
    check(launchDisplay({}, closed, 2000, 15000, standing({})).isEmpty(), "a window of no process has a display");
    return 0;
}
