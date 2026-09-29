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
    require(bentoApplicationNames({QStringLiteral("Kate"), QStringLiteral("Firefox"),
                                   QStringLiteral("Kate")})
            == QString::fromUtf8("Kate · Firefox · Kate"),
            "Bento pane order or duplicate labels were lost");
    require(bentoApplicationNames({QStringLiteral("Kate"), {}, QStringLiteral("Firefox")})
            == QString::fromUtf8("Kate · Firefox"),
            "Empty pane label was not ignored");
    require(stackPositionLabel(1, 3, false) == QStringLiteral("2 / 3"),
            "Stack position label is incorrect");
    require(stackPositionLabel(0, 3, true).isEmpty(),
            "Non-pageable Bento group exposed a stack position");
    require(stackPositionLabel(0, 1, false).isEmpty(),
            "Standalone card exposed a stack position");
    std::cout << "Card label presentation checks passed\n";
}
