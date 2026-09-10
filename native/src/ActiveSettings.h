#pragma once

#include <KConfigGroup>
#include <KConfigWatcher>
#include <KSharedConfig>
#include <QObject>
#include <algorithm>

namespace Kadunce {
class ActiveSettings : public QObject
{
public:
    explicit ActiveSettings(KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("kwinrc")))
        : m_config(std::move(config)), m_watcher(KConfigWatcher::create(m_config))
    {
        reload();
        connect(m_watcher.data(), &KConfigWatcher::configChanged, this,
            [this](const KConfigGroup &group, const QByteArrayList &keys) {
                if (group.name() == QStringLiteral("Effect-kadunce")
                    && (keys.isEmpty() || keys.contains("ActiveCardGutter"))) reload();
            });
    }
    int gutter() const { return m_gutter; }
private:
    void reload()
    {
        m_gutter = std::clamp(KConfigGroup(m_config, QStringLiteral("Effect-kadunce"))
            .readEntry("ActiveCardGutter", 10), 6, 48);
    }
    KSharedConfig::Ptr m_config;
    KConfigWatcher::Ptr m_watcher;
    int m_gutter = 10;
};
}
