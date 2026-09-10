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
#include <QPluginLoader>
#include <QJsonObject>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QLabel>
#include <QPointer>

#include "Compatibility.h"

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
        m_notifier.setIsMenu(false);
        QObject::connect(&m_notifier, &KStatusNotifierItem::activateRequested,
                         [this](bool, const QPoint &) {
            if (m_busy) return;
            if (effectEnabled()) disable(); else enable();
        });

        m_toggle->setCheckable(true);
        m_health = m_menu->addAction(QStringLiteral("Checking KWin compatibility…"));
        m_health->setEnabled(false);
        m_repairAction = m_menu->addAction(QStringLiteral("Repair for current KWin…"));
        QObject::connect(m_repairAction, &QAction::triggered, [this]() { repair(); });
        QObject::connect(&m_probe, &QProcess::finished, [this](int code, QProcess::ExitStatus status) {
            m_kwinVersion = code == 0 && status == QProcess::NormalExit
                ? Compatibility::kwinVersion(QString::fromUtf8(m_probe.readAllStandardOutput())) : QString();
            refresh();
        });
        QObject::connect(&m_probe, &QProcess::errorOccurred, [this](QProcess::ProcessError) {
            m_kwinVersion.clear();
            refresh();
        });
        QObject::connect(&m_repair, &QProcess::readyRead, [this]() {
            m_repairOutput += QString::fromUtf8(m_repair.readAll());
            // Keep diagnostics bounded even for a verbose compiler failure.
            m_repairOutput = m_repairOutput.right(24000);
        });
        QObject::connect(&m_repair, &QProcess::finished, [this](int code, QProcess::ExitStatus status) {
            m_repairing = false;
            m_repairOutput += QString::fromUtf8(m_repair.readAll());
            auto *message = new QMessageBox(code == 0 && status == QProcess::NormalExit
                    ? QMessageBox::Information : QMessageBox::Warning,
                QStringLiteral("Kadunce repair"),
                code == 0 && status == QProcess::NormalExit
                    ? QStringLiteral("Rebuilt and installed for the current KWin. Your enabled/disabled choice was preserved. "
                                     "No session restart was performed. If KWin still rejects it, save your work and log out and back in.")
                    : QStringLiteral("Repair did not complete. No effect toggle or desktop restart was requested. See details for the failing step."),
                QMessageBox::Ok);
            message->setDetailedText(m_repairOutput.right(24000));
            message->setWindowModality(Qt::NonModal);
            message->setAttribute(Qt::WA_DeleteOnClose);
            message->show();
            probe();
            refresh();
        });
        QObject::connect(&m_repair, &QProcess::errorOccurred, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                m_repairing = false;
                m_notifier.showMessage(QStringLiteral("Kadunce repair could not start"), m_repair.errorString(), QStringLiteral("dialog-error"));
                refresh();
            }
        });
        m_menu->addSeparator();
        QAction *settings = m_menu->addAction(QStringLiteral("Settings…"));
        QObject::connect(settings, &QAction::triggered, [this]() { showSettings(); });
        m_notifier.setContextMenu(m_menu);

        QObject::connect(m_menu, &QMenu::aboutToShow,
                         [this]() { probe(); refresh(); });
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
        probe();
        refresh();
    }

private:
    QString repairDirectory() const
    {
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + QStringLiteral("/kadunce/repair");
    }

    void probe()
    {
        if (m_probe.state() != QProcess::NotRunning) {
            return;
        }
        m_probe.start(QStringLiteral("kwin_wayland"), {QStringLiteral("--version")});
        const auto generation = ++m_probeGeneration;
        QTimer::singleShot(3000, &m_probe, [this, generation]() {
            if (generation == m_probeGeneration && m_probe.state() != QProcess::NotRunning) m_probe.kill();
        });
    }

    void repair()
    {
        if (m_repairing) return;
        auto *question = new QMessageBox(QMessageBox::Question, QStringLiteral("Repair Kadunce?"),
            QStringLiteral("Rebuild the source saved during installation for your current KWin? "
                           "No downloads or development-folder changes are used. Checks run before an administrator prompt installs the plugin. "
                           "Your workspace switch stays available; nothing will restart automatically."),
            QMessageBox::Yes | QMessageBox::Cancel);
        question->setDefaultButton(QMessageBox::Cancel);
        question->setWindowModality(Qt::NonModal);
        question->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(question, &QMessageBox::finished, [this](int answer) {
            if (answer != QMessageBox::Yes || m_repairing) return;
            m_repairing = true;
            m_repairOutput.clear();
            m_repair.setProcessChannelMode(QProcess::MergedChannels);
            m_repair.start(QStringLiteral("bash"), {repairDirectory() + QStringLiteral("/repair.sh")});
            refresh();
        });
        question->show();
    }

    void showSettings()
    {
        if (m_settings) { m_settings->raise(); m_settings->activateWindow(); return; }
        auto *dialog = new QDialog;
        m_settings = dialog;
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(QStringLiteral("Kadunce settings"));
        auto *layout = new QFormLayout(dialog);
        auto *gutter = new QSpinBox(dialog);
        gutter->setRange(6, 48);
        gutter->setSuffix(QStringLiteral(" px"));
        auto config = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
        config->reparseConfiguration();
        gutter->setValue(KConfigGroup(config, QStringLiteral("Effect-kadunce"))
            .readEntry("ActiveCardGutter", 10));
        layout->addRow(QStringLiteral("Active card gutter"), gutter);
        auto *hint = new QLabel(QStringLiteral("Default: 10 px. Applies when a card becomes Active.\n"
            "6–48 px. The gutter preserves room for edge swipes."), dialog);
        hint->setWordWrap(true);
        layout->addRow(hint);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, dialog);
        layout->addRow(buttons);
        QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
        QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, [config, gutter, dialog]() {
            KConfigGroup(config, QStringLiteral("Effect-kadunce")).writeEntry("ActiveCardGutter", gutter->value(), KConfig::Notify);
            config->sync();
            dialog->accept();
        });
        dialog->show();
    }

    QPointer<QDialog> m_settings;

    void refresh()
    {
        const bool enabled = effectEnabled();
        m_toggle->setText(QStringLiteral("Kadunce enabled"));
        m_toggle->setChecked(enabled);
        m_toggle->setEnabled(!m_busy);
        const QPluginLoader plugin(QStringLiteral("/usr/lib/qt6/plugins/kwin/effects/plugins/kwin4_effect_kadunce.so"));
        const QString built = Compatibility::pluginVersion(plugin.metaData().value(QStringLiteral("IID")).toString());
        m_mismatch = Compatibility::mismatch(built, m_kwinVersion);
        m_health->setText(m_mismatch
            ? QStringLiteral("KWin %1 → %2: rebuild needed").arg(built, m_kwinVersion)
            : built.isEmpty() || m_kwinVersion.isEmpty()
                ? QStringLiteral("KWin compatibility could not be determined")
                : QStringLiteral("Built for installed KWin %1").arg(built));
        m_repairAction->setText(m_repairing ? QStringLiteral("Repair in progress…") : QStringLiteral("Repair for current KWin…"));
        m_repairAction->setEnabled(!m_repairing && QFileInfo::exists(repairDirectory() + QStringLiteral("/source.tar")));
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
        if (m_mismatch) {
            m_notifier.showMessage(QStringLiteral("Kadunce needs a rebuild"),
                QStringLiteral("KWin was updated. Choose ‘Repair for current KWin’ from this tray menu. If you have not restarted since the system update, finish it with a normal logout/login first."),
                QStringLiteral("dialog-warning"));
            refresh();
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
    QAction *m_health;
    QAction *m_repairAction;
    QProcess m_probe;
    QProcess m_repair;
    QString m_kwinVersion;
    QString m_repairOutput;
    bool m_mismatch = false;
    bool m_repairing = false;
    unsigned m_probeGeneration = 0;
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
