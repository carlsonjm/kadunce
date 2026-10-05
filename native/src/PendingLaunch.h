/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "LaunchIdentity.h"
#include <QStringList>
#include <optional>

namespace Kadunce {
// An application someone has asked to open, waiting for its first window. It
// matches one window once, by application identity, and carries the token its
// requester names it by.
struct PendingLaunch {
    QStringList identities;
    QString token;

    static constexpr int MaximumIdentities = 8;

    static std::optional<PendingLaunch> make(const QStringList &applicationIds, const QString &token)
    {
        PendingLaunch launch;
        for (const auto &id : applicationIds) {
            const auto identity = LaunchIdentity::normalized(id);
            if (!identity.isEmpty()) launch.identities.append(identity);
        }
        if (launch.identities.isEmpty() || launch.identities.size() > MaximumIdentities
            || token.isEmpty()) return std::nullopt;
        launch.token = token;
        return launch;
    }

    [[nodiscard]] bool matches(const QString &applicationIdentity) const
    {
        for (const auto &identity : identities)
            if (LaunchIdentity::matches(identity, applicationIdentity)) return true;
        return false;
    }
};
} // namespace Kadunce
