#pragma once

#include <KConfigGroup>
#include <KConfigWatcher>
#include <KSharedConfig>
#include <QObject>
#include <algorithm>
#include <functional>

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
    // Called after the gutter changes while Kadunce runs, so the cards already
    // standing in it take the new one at once.
    void onGutterChanged(std::function<void()> changed) { m_changed = std::move(changed); }
private:
    void reload()
    {
        const int before = m_gutter;
        m_gutter = std::clamp(KConfigGroup(m_config, QStringLiteral("Effect-kadunce"))
            .readEntry("ActiveCardGutter", 10), 6, 48);
        if (m_gutter != before && m_changed) m_changed();
    }
    KSharedConfig::Ptr m_config;
    KConfigWatcher::Ptr m_watcher;
    int m_gutter = 10;
    std::function<void()> m_changed;
};
}
