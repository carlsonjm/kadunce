/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ZoneContract.h"

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
    const auto all = [](const QString &) { return true; };
    const auto none = [](const QString &) { return false; };
    require(missingZoneContract(all, all).isEmpty(),
        "A tile with everything zones use was refused");
    // A KWin that renames one property or one method turns zones off.
    const QStringList renamed = missingZoneContract(
        [](const QString &name) { return name != QStringLiteral("padding"); },
        [](const QString &name) { return name != QStringLiteral("unmanage"); });
    require(renamed == QStringList({QStringLiteral("padding"), QStringLiteral("unmanage()")}),
        "A renamed property or method was not named");
    require(missingZoneContract(none, none).size() == 6,
        "A tile with nothing zones use was not refused for all of it");
    std::cout << "zone contract: ok\n";
    return 0;
}
