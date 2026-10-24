#pragma once

#include "BentoLayout.h"
#include "CardTuning.h"

#include <KConfigGroup>
#include <KConfigWatcher>
#include <KSharedConfig>
#include <QObject>
#include <QStringList>
#include <algorithm>
#include <functional>

namespace Kadunce {
// The keys of kwinrc [Effect-kadunce] Kadunce reads, as installed in
// kadunce/settings-keys for a settings program to offer.
inline const QStringList &settingsKeys()
{
    static const QStringList keys{QStringLiteral("ActiveCardGutter"), QStringLiteral("CardCornerRadius"),
        QStringLiteral("SleepingDimmed"), QStringLiteral("StackPauseMs"), QStringLiteral("StackBehind"),
        QStringLiteral("SpreadPeek")};
    return keys;
}

class ActiveSettings : public QObject
{
public:
    explicit ActiveSettings(KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("kwinrc")))
        : m_config(std::move(config)), m_watcher(KConfigWatcher::create(m_config))
    {
        reload();
        connect(m_watcher.data(), &KConfigWatcher::configChanged, this,
            [this](const KConfigGroup &group, const QByteArrayList &keys) {
                if (group.name() != QStringLiteral("Effect-kadunce")) return;
                const bool read = keys.isEmpty() || std::any_of(keys.cbegin(), keys.cend(),
                    [](const QByteArray &key) { return settingsKeys().contains(QString::fromUtf8(key)); });
                if (read) reload();
            });
    }
    int gutter() const { return m_gutter; }
    // Called after the gutter changes while Kadunce runs, so the cards already
    // standing in it, and Bento's panes, take the new one at once.
    void onGutterChanged(std::function<void()> changed) { m_changed = std::move(changed); }
    // Called after anything else in CardTuning.h changes while Kadunce runs.
    void onTuningChanged(std::function<void()> changed) { m_tuningChanged = std::move(changed); }
private:
    void reload()
    {
        const int before = m_gutter;
        const int paneGapBefore = bentoPaneGap();
        const KConfigGroup group(m_config, QStringLiteral("Effect-kadunce"));
        m_gutter = std::clamp(group.readEntry("ActiveCardGutter", 10), 4, 48);
        // Bento's panes stand as far apart as the gutter, set or not.
        setBentoPaneGap(m_gutter);
        if ((m_gutter != before || bentoPaneGap() != paneGapBefore) && m_changed) m_changed();

        const CardTuning tuningBefore = cardTuning();
        CardTuning tuning;
        tuning.cornerRadius = std::clamp(group.readEntry("CardCornerRadius", CardTuning::DefaultCornerRadius), 2.0, 15.0);
        if (group.hasKey("SleepingDimmed")) tuning.sleepingDimmed = group.readEntry("SleepingDimmed", false);
        tuning.stackPauseMs = std::clamp(group.readEntry("StackPauseMs", CardTuning::DefaultStackPauseMs), 200, 800);
        tuning.stackBehindShown = group.readEntry("StackBehind", QStringLiteral("hairline")).trimmed().toLower()
            != QStringLiteral("none");
        tuning.spreadPeek = std::clamp(group.readEntry("SpreadPeek", CardTuning::DefaultSpreadPeek), 0.0, 1.0);
        setCardTuning(tuning);
        if (!(tuning == tuningBefore) && m_tuningChanged) m_tuningChanged();
    }
    KSharedConfig::Ptr m_config;
    KConfigWatcher::Ptr m_watcher;
    int m_gutter = 10;
    std::function<void()> m_changed;
    std::function<void()> m_tuningChanged;
};
}
