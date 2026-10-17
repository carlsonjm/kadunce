#pragma once

#include "BentoLayout.h"

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
    // standing in it, and Bento's panes, take the new one at once.
    void onGutterChanged(std::function<void()> changed) { m_changed = std::move(changed); }
private:
    void reload()
    {
        const int before = m_gutter;
        const int paneGapBefore = bentoPaneGap();
        const KConfigGroup group(m_config, QStringLiteral("Effect-kadunce"));
        m_gutter = std::clamp(group.readEntry("ActiveCardGutter", 10), 4, 48);
        // Bento's panes stand the same distance apart once a gutter is set;
        // until then they keep their own 14.
        setBentoPaneGap(group.hasKey("ActiveCardGutter") ? m_gutter : DefaultBentoPaneGap);
        if ((m_gutter != before || bentoPaneGap() != paneGapBefore) && m_changed) m_changed();
    }
    KSharedConfig::Ptr m_config;
    KConfigWatcher::Ptr m_watcher;
    int m_gutter = 10;
    std::function<void()> m_changed;
};
}
