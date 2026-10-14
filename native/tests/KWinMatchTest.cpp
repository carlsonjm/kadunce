/* SPDX-License-Identifier: GPL-2.0-or-later */
// Cards play only on the KWin they were built for: a Plasma update ahead of
// the rebuild leaves them off rather than loading into a changed KWin.
#include "KWinMatch.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
} // namespace

int main()
{
    require(kwinMatches(QStringLiteral("6.7.5"), QStringLiteral("6.7.5")),
        "The KWin Cards were built for was refused");
    require(!kwinMatches(QStringLiteral("6.7.5"), QStringLiteral("6.8.0")),
        "A newer Plasma was accepted before the rebuild");
    require(!kwinMatches(QStringLiteral("6.7.5"), QStringLiteral("6.7.4")),
        "An older bugfix release was accepted");
    require(kwinMatches(QStringLiteral("6.7.5"), QString()),
        "A KWin that did not say its version turned Cards off");
    require(kwinMatches(QString(), QStringLiteral("6.8.0")),
        "A build that did not record its KWin turned Cards off");
    return 0;
}
