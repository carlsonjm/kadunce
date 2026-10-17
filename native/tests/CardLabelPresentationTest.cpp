#include "CardLabelPresentation.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce;

void require(bool condition, const char *message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

int main()
{
    require(humanApplicationName(QStringLiteral("Firefox"), QStringLiteral("firefox"),
            QStringLiteral("Navigator"), QStringLiteral("Page"))
            == QStringLiteral("Firefox"), "Desktop service name did not win");
    require(humanApplicationName({}, QStringLiteral("org.kde.kate"),
            QStringLiteral("kate"), QStringLiteral("notes.txt"))
            == QStringLiteral("org.kde.kate"), "Resource class fallback failed");
    require(humanApplicationName({}, {}, QStringLiteral("kate"),
            QStringLiteral("notes.txt")) == QStringLiteral("kate"),
            "Window class fallback failed");
    require(humanApplicationName({}, {}, {}, QStringLiteral("notes.txt"))
            == QStringLiteral("notes.txt"),
            "Caption fallback failed");
    require(pairedApplicationName(QStringLiteral("Firefox"), {QStringLiteral("Notes")})
            == QString::fromUtf8("Firefox · with Notes"),
            "A pair did not name its partner");
    require(pairedApplicationName(QStringLiteral("Firefox"),
                {QStringLiteral("Notes"), {}, QStringLiteral("Files"), QStringLiteral("Kate")})
            == QString::fromUtf8("Firefox · with Notes, Files and Kate"),
            "A layout did not name every pane beside the card");
    require(pairedApplicationName(QStringLiteral("Firefox"), {}) == QStringLiteral("Firefox"),
            "A card in no layout named a partner");
    require(stackedApplicationName(QStringLiteral("Firefox"),
                {QStringLiteral("Files"), QStringLiteral("Notes"), QStringLiteral("Kate")})
            == QString::fromUtf8("Firefox · then Files, Notes and Kate"),
            "A Stack did not name the cards behind its face");
    require(stackedApplicationName(QStringLiteral("Firefox"), {}) == QStringLiteral("Firefox"),
            "A card with nothing behind it named one");
    require(stackPositionLabel(1, 3) == QStringLiteral("2 / 3"),
            "Stack position label is incorrect");
    require(stackPositionLabel(0, 1).isEmpty(),
            "Standalone card exposed a stack position");
    std::cout << "Card label presentation checks passed\n";
}
