// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QList>
#include <QRectF>
#include <QString>

#include <algorithm>
#include <functional>

namespace Kadunce
{

// Where Table's rows stand on its display: pills of workspaces under the top
// edge, the chosen one's cards hanging below it on a stem, the depth line
// between them, past which the cards are chosen, and the lift line under the
// cards, past which a card is moved. Both lines sit 5 mm from the row above them, for a finger.
// The effect measures text; this places everything, so hit tests and drawing
// agree.
struct TableTabContent {
    // Empty for a workspace nothing has named yet: its tab is its number.
    QString name;
    bool current = false;
    int colours = 0;
    // The desktop's place in KDE's order, from 1, which KDE's own
    // Switch to Desktop shortcuts use.
    int number = 0;
    // Being renamed: the name is what has been typed, shown whole however
    // crowded the row, with room for the cursor when nothing has.
    bool editing = false;
};

// What a crowded row gives up, in this order, so every workspace stays in
// reach: the names shorten, the application colours go,
// the names shorten further, and at their smallest the tabs are circles
// holding only their numbers.
enum class TableTabParts { All, NoColours, NamesOnly, Numbers };

struct TableCardContent {
    QString application;
    QString title;
    // A stack's cards behind its face, whose edges show to its left.
    int stacked = 0;
};

struct TableLayout {
    double row = 44.0;
    double tabsTop = 0.0;
    double trayTop = 0.0;
    double stemHeight = 26.0;
    double depth = 0.0;
    double lift = 0.0;
    double cardHeight = 0.0;
    QList<QRectF> tabs;
    QList<double> nameWidths;
    TableTabParts parts = TableTabParts::All;
    // Empty when no more workspaces can be made.
    QRectF plus;
    // Beside a tab whose name has been cleared: removes the workspace.
    QRectF remove;
    // Each workspace's cards, laid out as they hang under its own tab.
    QList<QList<QRectF>> cards;
    QList<QList<double>> textWidths;
    // Where a workspace with no cards says so, under its tab.
    QList<QRectF> empty;
};

struct TableMetrics {
    // Returns the width text takes at a pixel size, medium weight or regular.
    std::function<double(const QString &, double, bool)> measure;
};

// The piece under the finger lifts a little, and never into a neighbour's
// gap: Itasca keeps 4 between pills, so the tabs stand 8 apart and the cards
// 12, and a lift grows each side by at most 4. The number sits at the centre
// of the pill's rounded end.
namespace TableSizes {
inline constexpr double TabGap = 8.0;
inline constexpr double RowSide = 12.0;
inline constexpr double TabPadLeft = 13.0;
inline constexpr double TabPadRight = 16.0;
inline constexpr double TabInnerGap = 8.0;
// The number's place.
inline constexpr double Number = 20.0;
inline constexpr double Colour = 10.0;
inline constexpr double ColourGap = 4.0;
inline constexpr double NameSize = 14.0;
inline constexpr double NameMinimum = 36.0;
// About nine letters: what the colours keep for a name.
inline constexpr double NameComfortable = 72.0;
inline constexpr double PlusPad = 14.0;
inline constexpr double PlusIcon = 18.0;
inline constexpr double PlusIconGap = 6.0;
inline constexpr double StemHeight = 38.0;
// About 5 mm on the tablet: 25 logical pixels at its scale of 1.75.
inline constexpr double LineGap = 25.0;
inline constexpr double CardGap = 12.0;
inline constexpr double CardPadLeft = 16.0;
inline constexpr double CardPadRight = 16.0;
inline constexpr double CardIcon = 24.0;
inline constexpr double CardInnerGap = 8.0;
inline constexpr double CardAppSize = 13.0;
inline constexpr double CardTitleSize = 12.0;
inline constexpr double CardTitleMaximum = 150.0;
// Spread's closed stack: at most three edges behind the face, a step apart.
inline constexpr double SliverStep = 7.0;
inline constexpr int SliversShown = 3;
inline constexpr double TrayEdge = 12.0;
}

inline double tableRowHeight(double height)
{
    return std::clamp(height * 0.085, 34.0, 46.0);
}

// A tab's width apart from its name.
inline double tableTabFrame(const TableTabContent &tab, TableTabParts parts)
{
    using namespace TableSizes;
    double width = TabPadLeft + Number + TabPadRight;
    if (parts == TableTabParts::All && tab.colours > 0 && !tab.editing)
        width += TabInnerGap + tab.colours * Colour + (tab.colours - 1) * ColourGap;
    if (!tab.name.isEmpty() || tab.editing) width += TabInnerGap;
    return width;
}

// The + tab is a circle, and says New beside its + while a card is carried to
// it. An empty label shows the + alone; without a + no more workspaces can be
// made. A remove label puts its pill after the tab being renamed.
inline TableLayout layoutTable(const QSizeF &display, const QList<TableTabContent> &tabs,
    const QList<QList<TableCardContent>> &cards, const QString &plusLabel,
    const QString &emptyLabel, const TableMetrics &metrics, bool withPlus = true,
    const QString &removeLabel = QString())
{
    using namespace TableSizes;
    TableLayout layout;
    layout.row = tableRowHeight(display.height());
    layout.tabsTop = display.height() * 0.04;
    layout.stemHeight = StemHeight;
    layout.trayTop = layout.tabsTop + layout.row + StemHeight;
    layout.depth = layout.tabsTop + layout.row + LineGap;

    // Each tab at its natural width. When the row is wider than the display
    // the names give way evenly, never below a readable minimum; past that
    // the tabs give up their parts in TableTabParts' order.
    const double plusWidth = !withPlus ? 0.0
        : plusLabel.isEmpty() ? layout.row
        : std::max(layout.row, PlusPad + PlusIcon + PlusIconGap + metrics.measure(plusLabel, NameSize, true) + PlusPad);
    int editing = -1;
    for (int i = 0; i < tabs.size(); ++i)
        if (tabs[i].editing) editing = i;
    const double removeWidth = editing < 0 || removeLabel.isEmpty() ? 0.0
        : PlusPad + metrics.measure(removeLabel, NameSize, true) + PlusPad;
    const double available = display.width() - 2 * RowSide - plusWidth - TabGap * (tabs.size() - (withPlus ? 0 : 1))
        - (removeWidth > 0 ? removeWidth + TabGap : 0.0);
    QList<double> natural;
    for (const auto &tab : tabs) {
        const double measured = tab.name.isEmpty() ? 0.0 : metrics.measure(tab.name, NameSize, true);
        natural.append(tab.editing ? std::max(measured + 2.0, NameMinimum) : measured);
    }
    // A tab is never narrower than the circle it becomes at its smallest.
    const auto frameOf = [&](int i, TableTabParts parts) {
        const double frame = tableTabFrame(tabs[i], parts);
        return tabs[i].name.isEmpty() && !tabs[i].editing ? std::max(layout.row, frame) : frame;
    };
    // The tab being renamed keeps its whole name at every tier.
    const auto kept = [&](int i, double floor) { return tabs[i].editing ? natural[i] : std::min(natural[i], floor); };
    const auto fits = [&](TableTabParts parts) {
        const double floor = parts == TableTabParts::NamesOnly ? NameMinimum : NameComfortable;
        double width = 0.0;
        for (int i = 0; i < tabs.size(); ++i) width += frameOf(i, parts) + kept(i, floor);
        return width <= available;
    };
    layout.parts = TableTabParts::Numbers;
    for (TableTabParts parts : {TableTabParts::All, TableTabParts::NoColours, TableTabParts::NamesOnly}) {
        if (fits(parts)) {
            layout.parts = parts;
            break;
        }
    }
    QList<double> names;
    QList<double> frames;
    for (int i = 0; i < tabs.size(); ++i) {
        const bool numbers = layout.parts == TableTabParts::Numbers && !tabs[i].editing;
        names.append(numbers ? 0.0 : natural[i]);
        frames.append(numbers ? layout.row : frameOf(i, layout.parts == TableTabParts::Numbers ? TableTabParts::NamesOnly : layout.parts));
    }
    double total = 0.0;
    for (int i = 0; i < tabs.size(); ++i) total += names[i] + frames[i];
    if (total > available && layout.parts != TableTabParts::Numbers && !tabs.isEmpty()) {
        double frameTotal = 0.0;
        for (int i = 0; i < tabs.size(); ++i) frameTotal += frames[i] + (tabs[i].editing ? names[i] : 0.0);
        double overflow = total - frameTotal - std::max(0.0, available - frameTotal);
        // Shorten the longest names first, down to what they share evenly;
        // the one being typed is left whole.
        const double typed = editing >= 0 ? names[editing] : 0.0;
        if (editing >= 0) names[editing] = 0.0;
        QList<double> sorted = names;
        std::sort(sorted.begin(), sorted.end(), std::greater<>());
        double cap = sorted.first();
        for (int i = 0; i < sorted.size() && overflow > 0; ++i) {
            const double next = i + 1 < sorted.size() ? std::max(sorted[i + 1], NameMinimum) : NameMinimum;
            const double cut = std::min(overflow, std::max(0.0, cap - next) * (i + 1));
            cap -= cut / (i + 1);
            overflow -= cut;
        }
        for (double &name : names) name = std::min(name, std::max(cap, std::min(name, NameMinimum)));
        if (editing >= 0) names[editing] = typed;
    }
    double rowWidth = plusWidth + TabGap * (tabs.size() - (withPlus ? 0 : 1)) + (removeWidth > 0 ? removeWidth + TabGap : 0.0);
    for (int i = 0; i < tabs.size(); ++i) rowWidth += names[i] + frames[i];
    double x = std::max(RowSide, (display.width() - rowWidth) / 2.0);
    for (int i = 0; i < tabs.size(); ++i) {
        const QRectF rect(x, layout.tabsTop, names[i] + frames[i], layout.row);
        layout.tabs.append(rect);
        layout.nameWidths.append(names[i]);
        x = rect.right() + TabGap;
        if (i == editing && removeWidth > 0) {
            layout.remove = QRectF(x, layout.tabsTop, removeWidth, layout.row);
            x = layout.remove.right() + TabGap;
        }
    }
    if (withPlus) layout.plus = QRectF(x, layout.tabsTop, plusWidth, layout.row);

    const double cardHeight = layout.row + 14.0;
    layout.cardHeight = cardHeight;
    layout.lift = layout.trayTop + cardHeight + LineGap;
    for (int w = 0; w < tabs.size(); ++w) {
        QList<double> widths;
        QList<double> texts;
        double trayWidth = -CardGap;
        const auto &own = w < cards.size() ? cards[w] : QList<TableCardContent>{};
        for (const auto &card : own) {
            const double text = std::max(metrics.measure(card.application, CardAppSize, true),
                std::min(metrics.measure(card.title, CardTitleSize, false), CardTitleMaximum));
            const double width = CardPadLeft + CardIcon + CardInnerGap + text + CardPadRight
                + std::min(card.stacked, SliversShown) * SliverStep;
            widths.append(width);
            texts.append(text);
            trayWidth += width + CardGap;
        }
        const double centre = layout.tabs[w].center().x();
        QList<QRectF> rects;
        if (!own.isEmpty()) {
            double left = std::clamp(centre - trayWidth / 2.0, TrayEdge,
                std::max(TrayEdge, display.width() - trayWidth - TrayEdge));
            for (double width : widths) {
                rects.append(QRectF(left, layout.trayTop, width, cardHeight));
                left += width + CardGap;
            }
        }
        layout.cards.append(rects);
        layout.textWidths.append(texts);
        const double emptyWidth = metrics.measure(emptyLabel, CardTitleSize, false) + 28.0;
        layout.empty.append(QRectF(std::clamp(centre - emptyWidth / 2.0, TrayEdge,
            std::max(TrayEdge, display.width() - emptyWidth - TrayEdge)), layout.trayTop, emptyWidth, layout.row));
    }
    return layout;
}

} // namespace Kadunce
