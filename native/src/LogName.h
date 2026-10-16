/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QPointer>
#include <QString>
#include <QUuid>

namespace Kadunce
{
// What the journal calls a window: its application and the start of its id,
// enough to follow one window through a log. Never its title, which names the
// document or the page open in it.
[[nodiscard]] inline QString logName(const QString &windowClass, const QUuid &id)
{
    // A window class reads "resourceName resourceClass"; the class is last.
    const QString application = windowClass.section(QLatin1Char(' '), -1).trimmed();
    return QStringLiteral("%1 %2").arg(application.isEmpty() ? QStringLiteral("window") : application,
        id.toString(QUuid::WithoutBraces).left(8));
}

// For KWin's EffectWindow, which names its class windowClass(), and its
// Window, which names it resourceClass().
template<typename W>
[[nodiscard]] QString logNameOf(const W *window)
{
    if (!window) return QStringLiteral("nothing");
    if constexpr (requires { window->windowClass(); })
        return logName(window->windowClass(), window->internalId());
    else
        return logName(window->resourceClass(), window->internalId());
}

template<typename W>
[[nodiscard]] QString logNameOf(const QPointer<W> &window)
{
    return logNameOf(window.data());
}
} // namespace Kadunce
