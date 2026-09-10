#pragma once
#include <QString>

namespace LaunchIdentity {
inline QString normalized(QString value)
{
    value = value.trimmed().toLower();
    if (value.startsWith(QStringLiteral("applications:"))) value.remove(0, 13);
    if (value.startsWith(QStringLiteral("services_"))) value.remove(0, 9);
    value = value.section(QLatin1Char('/'), -1);
    if (value.endsWith(QStringLiteral(".desktop"))) value.chop(8);
    return value;
}
inline bool matches(const QString &requested, const QString &actual)
{
    const auto left = normalized(requested);
    const auto right = normalized(actual);
    return !left.isEmpty() && left == right;
}
}
