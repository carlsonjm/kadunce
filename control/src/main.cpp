/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KConfigGroup>
#include <KSharedConfig>
#include <KStatusNotifierItem>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QLockFile>
#include <QMenu>
#include <QProcess>
#include <QTimer>

namespace
{
constexpr auto NativeEffectId = "kwin4_effect_kadunce";

struct CommandResult {
    bool succeeded = false;
    QByteArray output;
};

CommandResult runCommand(const QString &program, const QStringList &arguments,
                         int timeout = 8000)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);
    if (!process.waitForStarted(2000)) {
        return {};
    }
    if (!process.waitForFinished(timeout)) {
        process.kill();
        process.waitForFinished(1000);
        return {};
    }
    return {
        process.exitStatus() == QProcess::NormalExit
            && process.exitCode() == 0,
        process.readAll(),
    };
}

bool writeEffectEnabled(bool enabled)
{
    const KSharedConfig::Ptr config = KSharedConfig::openConfig(
        QStringLiteral("kwinrc"));
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    plugins.writeEntry(
        QStringLiteral("kwin4_effect_kadunceEnabled"), enabled);
    config->sync();
    return plugins.readEntry(
        QStringLiteral("kwin4_effect_kadunceEnabled"), !enabled)
        == enabled;
}

bool effectEnabled()
{
    const KSharedConfig::Ptr config = KSharedConfig::openConfig(
        QStringLiteral("kwinrc"));
    config->reparseConfiguration();
    const KConfigGroup plugins(config, QStringLiteral("Plugins"));
    return plugins.readEntry(
        QStringLiteral("kwin4_effect_kadunceEnabled"), false);
}

CommandResult callKWinEffects(const QString &method, const QString &effectId)
{
    return runCommand(QStringLiteral("qdbus6"), {
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Effects"),
        QStringLiteral("org.kde.kwin.Effects.%1").arg(method),
        effectId,
    });
}

void reconfigureKWin()
{
    (void)runCommand(QStringLiteral("qdbus6"), {
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/KWin"),
        QStringLiteral("org.kde.KWin.reconfigure"),
    });
}

QString disabledMarkerPath()
{
    const QString stateHome = qEnvironmentVariableIsEmpty("XDG_STATE_HOME")
        ? QDir::homePath() + QStringLiteral("/.local/state")
        : QString::fromLocal8Bit(qgetenv("XDG_STATE_HOME"));
    return stateHome + QStringLiteral("/kadunce/disabled");
}

void setDisabledMarker(bool disabled)
{
    const QString path = disabledMarkerPath();
    if (!disabled) {
        QFile::remove(path);
        return;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile marker(path);
    if (marker.open(QIODevice::WriteOnly)) {
        marker.close();
    }
}

bool prepareWaylandEnvironment()
{
    const bool useWayland = qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")
        || qgetenv("QT_QPA_PLATFORM") == QByteArrayLiteral("wayland");
    const QByteArray runtime = qgetenv("XDG_RUNTIME_DIR");
    if (useWayland && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        const QList<QByteArray> candidates{QByteArrayLiteral("wayland-0"),
                                           QByteArrayLiteral("wayland-1")};
        for (const QByteArray &candidate : candidates) {
            if (QFileInfo::exists(QString::fromLocal8Bit(runtime)
                                  + QLatin1Char('/')
                                  + QString::fromLocal8Bit(candidate))) {
                qputenv("WAYLAND_DISPLAY", candidate);
                break;
            }
        }
    }
    if (useWayland
        && (runtime.isEmpty()
            || qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))) {
        return false;
    }
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("wayland"));
    }
    return true;
}

class ControlMenu final
{
public:
    ControlMenu()
        : m_notifier(QStringLiteral("kadunce-control"))
        , m_menu(new QMenu)
        , m_toggle(m_menu->addAction(QStringLiteral("Kadunce enabled")))
    {
        m_notifier.setTitle(QStringLiteral("Kadunce"));
        m_notifier.setCategory(KStatusNotifierItem::SystemServices);
        // This is the user's emergency workspace switch, so it remains
        // discoverable in the live tray instead of drifting into overflow.
        m_notifier.setStatus(KStatusNotifierItem::Active);
        m_notifier.setStandardActionsEnabled(false);
        m_notifier.setIsMenu(true);

        m_toggle->setCheckable(true);
        m_menu->addSeparator();
        QAction *settings = m_menu->addAction(QStringLiteral("Settings…"));
        settings->setEnabled(false);
        m_notifier.setContextMenu(m_menu);

        QObject::connect(m_menu, &QMenu::aboutToShow,
                         [this]() { refresh(); });
        QObject::connect(m_toggle, &QAction::triggered,
                         [this](bool enabled) {
            if (enabled) {
                enable();
            } else {
                disable();
            }
        });

        m_refresh.setInterval(5000);
        QObject::connect(&m_refresh, &QTimer::timeout,
                         [this]() { refresh(); });
        m_refresh.start();
        refresh();
    }

private:
    void refresh()
    {
        const bool enabled = effectEnabled();
        m_toggle->setText(QStringLiteral("Kadunce enabled"));
        m_toggle->setChecked(enabled);
        m_toggle->setEnabled(!m_busy);
        const QString icon = enabled
            ? QStringLiteral(":/icons/assets/kadunce-enabled.svg")
            : QStringLiteral(":/icons/assets/kadunce-disabled.svg");
        m_notifier.setIconByPixmap(QIcon(icon));
        m_notifier.setToolTip(
            QIcon(icon), QStringLiteral("Kadunce"),
            enabled ? QStringLiteral("Workspace enabled")
                    : QStringLiteral("Workspace disabled"));
    }

    void setBusy(bool busy)
    {
        m_busy = busy;
        if (busy) {
            m_toggle->setText(QStringLiteral("Changing Kadunce…"));
            m_toggle->setEnabled(false);
            QApplication::processEvents();
        } else {
            refresh();
        }
    }

    void enable()
    {
        if (m_busy || effectEnabled()) {
            return;
        }
        setBusy(true);
        bool loaded = writeEffectEnabled(true);
        if (loaded) {
            reconfigureKWin();
            const CommandResult result = callKWinEffects(
                QStringLiteral("loadEffect"),
                QString::fromLatin1(NativeEffectId));
            loaded = result.succeeded
                && result.output.trimmed() != QByteArrayLiteral("false");
        }
        if (!loaded) {
            writeEffectEnabled(false);
            reconfigureKWin();
            setBusy(false);
            m_notifier.showMessage(
                QStringLiteral("Kadunce was not enabled"),
                QStringLiteral("KWin did not accept the workspace effect. "
                               "The previous disabled state was preserved."),
                QStringLiteral("dialog-error"));
            return;
        }
        setDisabledMarker(false);
        setBusy(false);
        m_notifier.showMessage(
            QStringLiteral("Kadunce enabled"),
            QStringLiteral("Card Line and Bento are ready."),
            QStringLiteral("dialog-information"));
    }

    void disable()
    {
        if (m_busy || !effectEnabled()) {
            return;
        }
        setBusy(true);
        if (!writeEffectEnabled(false)) {
            setBusy(false);
            return;
        }
        const CommandResult unloaded = callKWinEffects(
            QStringLiteral("unloadEffect"),
            QString::fromLatin1(NativeEffectId));
        const bool safelyUnloaded = unloaded.succeeded
            && unloaded.output.trimmed() != QByteArrayLiteral("false");
        if (!safelyUnloaded) {
            writeEffectEnabled(true);
            reconfigureKWin();
            setBusy(false);
            m_notifier.showMessage(
                QStringLiteral("Kadunce remains enabled"),
                QStringLiteral("KWin could not confirm a safe release, so "
                               "the enabled state was restored."),
                QStringLiteral("dialog-warning"));
            return;
        }
        setDisabledMarker(true);
        reconfigureKWin();
        setBusy(false);
        m_notifier.showMessage(
            QStringLiteral("Kadunce disabled"),
            QStringLiteral("Managed windows were safely released."),
            QStringLiteral("dialog-information"));
    }

    KStatusNotifierItem m_notifier;
    QMenu *m_menu;
    QAction *m_toggle;
    QTimer m_refresh;
    bool m_busy = false;
};

} // namespace

int main(int argc, char **argv)
{
    if (!prepareWaylandEnvironment()) {
        return 75;
    }

    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("kadunce-control"));
    application.setOrganizationName(QStringLiteral("Jared Carlson"));
    application.setOrganizationDomain(QStringLiteral("studio.warbler"));
    application.setDesktopFileName(
        QStringLiteral("studio.warbler.Kadunce.Control"));
    application.setQuitOnLastWindowClosed(false);

    const QString runtime = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"));
    QLockFile instanceLock(runtime
        + QStringLiteral("/kadunce-control.lock"));
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock()) {
        return 0;
    }

    ControlMenu control;
    return application.exec();
}
