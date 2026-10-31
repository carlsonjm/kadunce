/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QSizeF>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <optional>

namespace Kadunce
{
// CARD-LIFECYCLE.md §11, Connected: a window opens at the size its kind of
// application wants, then fits into the room beside what stands there. The
// kind is read from the categories the application's desktop file names.
enum class AppKind { Other, Terminal, Browser, Document, Files, Communication, Media };

[[nodiscard]] inline AppKind appKindFromCategories(const QStringList &categories)
{
    const auto has = [&categories](std::initializer_list<const char *> names) {
        for (const char *name : names)
            if (categories.contains(QLatin1String(name), Qt::CaseInsensitive)) return true;
        return false;
    };
    // Most specific first: a terminal is also a System application, a browser
    // a Network one.
    if (has({"TerminalEmulator"})) return AppKind::Terminal;
    if (has({"WebBrowser"})) return AppKind::Browser;
    if (has({"FileManager"})) return AppKind::Files;
    if (has({"Email", "Chat", "InstantMessaging", "IRCClient", "Feed", "Music", "Audio"}))
        return AppKind::Communication;
    if (has({"Video", "Player", "Photography", "Viewer", "Game", "2DGraphics", "RasterGraphics"}))
        return AppKind::Media;
    if (has({"TextEditor", "WordProcessor", "Office", "Development", "IDE", "Spreadsheet",
             "Presentation"}))
        return AppKind::Document;
    return AppKind::Other;
}

// The size, in logical pixels, a window of this kind wants on a display with
// `area` free: a terminal a small square of about 80 by 24 characters, a
// browser the width most web pages are made for, a document one page wide.
// Media takes all it is given. A size the area cannot hold is the area. An
// application of no known kind wants none until a resize gives it a width,
// and stands as a layout would stand it.
[[nodiscard]] inline std::optional<QSizeF> naturalSizeFor(AppKind kind, QSizeF area)
{
    if (area.isEmpty()) return std::nullopt;
    const auto clamp = [&area](double width, double height) {
        return QSizeF(std::min(width, area.width()), std::min(height, area.height()));
    };
    switch (kind) {
    case AppKind::Terminal: return clamp(720, 560);
    case AppKind::Browser: return clamp(1280, area.height());
    case AppKind::Document: return clamp(1000, area.height());
    case AppKind::Files: return clamp(960, 640);
    case AppKind::Communication: return clamp(1100, area.height());
    case AppKind::Media: return area;
    case AppKind::Other: return std::nullopt;
    }
    return std::nullopt;
}

// A width the wheel gave an application is its natural width from then on on
// displays of that width, kept in kaduncerc [NaturalWidths] under this key.
[[nodiscard]] inline QString naturalWidthKey(const QString &app, int displayWidth)
{
    return app.isEmpty() || displayWidth <= 0 ? QString()
        : app + QLatin1Char('@') + QString::number(displayWidth);
}

// What a window wants of a layout, each a share of the work area: its natural
// size, and the least it can be given.
struct NaturalNeed {
    double width = 0.0;
    double height = 1.0;
    double leastWidth = 0.0;
    double leastHeight = 0.0;
    // A window wanting less than this share of the height stands in a corner
    // of its column, the room under it left open.
    [[nodiscard]] bool shortWindow() const { return height <= .6; }
};

[[nodiscard]] inline NaturalNeed naturalNeed(std::optional<QSizeF> natural, QSizeF minimum,
                                             QSizeF area)
{
    NaturalNeed need;
    if (area.isEmpty()) return need;
    need.leastWidth = std::clamp(minimum.width() / area.width(), 0.0, 1.0);
    need.leastHeight = std::clamp(minimum.height() / area.height(), 0.0, 1.0);
    need.width = natural ? std::clamp(natural->width() / area.width(), need.leastWidth, 1.0) : 0.0;
    need.height = natural ? std::clamp(natural->height() / area.height(), need.leastHeight, 1.0) : 1.0;
    return need;
}
} // namespace Kadunce
