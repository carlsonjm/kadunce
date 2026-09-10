#pragma once

#include <QString>
#include <QRegularExpression>

namespace Compatibility {
inline QString pluginVersion(const QString &iid)
{
    const auto match = QRegularExpression(
        QStringLiteral("^org\\.kde\\.kwin\\.EffectPluginFactory(\\d+\\.\\d+\\.\\d+)$")).match(iid);
    return match.hasMatch() ? match.captured(1) : QString();
}

inline QString kwinVersion(const QString &output)
{
    const auto match = QRegularExpression(
        QStringLiteral("(?:^|\\n)kwin (\\d+\\.\\d+\\.\\d+)(?:\\s|$)")).match(output);
    return match.hasMatch() ? match.captured(1) : QString();
}

inline bool mismatch(const QString &plugin, const QString &installed)
{
    // An unreadable version is unknown, never proof of incompatibility.
    return !plugin.isEmpty() && !installed.isEmpty() && plugin != installed;
}
}
