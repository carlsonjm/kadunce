/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QStringList>

#include <functional>

namespace Kadunce {

// What Kadunce reads from and asks of KWin's tiles, which KWin keeps out of
// its installed headers and publishes only to scripting by name. A KWin that
// renames any of it still builds, and would leave zones quietly placing
// nothing.
inline const QStringList &zoneProperties()
{
    static const QStringList names{QStringLiteral("tiles"), QStringLiteral("relativeGeometry"),
        QStringLiteral("absoluteGeometry"), QStringLiteral("padding")};
    return names;
}

inline const QStringList &zoneMethods()
{
    static const QStringList names{QStringLiteral("manage"), QStringLiteral("unmanage")};
    return names;
}

// The names a tile lacks, empty when it has everything zones use.
[[nodiscard]] inline QStringList missingZoneContract(
    const std::function<bool(const QString &)> &hasProperty,
    const std::function<bool(const QString &)> &hasMethod)
{
    QStringList missing;
    for (const QString &name : zoneProperties())
        if (!hasProperty(name)) missing.append(name);
    for (const QString &name : zoneMethods())
        if (!hasMethod(name)) missing.append(name + QStringLiteral("()"));
    return missing;
}

} // namespace Kadunce
