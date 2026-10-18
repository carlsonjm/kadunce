/* SPDX-License-Identifier: GPL-2.0-or-later */
// The Effect's side of Spread's guest: the program that draws in Spread's
// centre, Tettegouche's Search or a companion, over D-Bus
// (docs/TETTEGOUCHE-CONTEXT.md). The guest's place in the row is the card
// stage's; this file holds who the guest is, what it may ask, and when it goes.
#include "Effect.h"
#include "CallerTrust.h"
#include "EffectShared.h"
#include "GuestCaller.h"
#include "LaunchIdentity.h"
#include "NativeLanding.h"

#include <core/output.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusReply>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QDir>
#include <QEasingCurve>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimer>

#include <limits.h>
#include <unistd.h>

namespace Kadunce
{

int Effect::launcherGuestProtocolVersion() const
{
    return 3;
}

QString Effect::beginLauncherGuest(const QString &ownerService)
{
    const QString reply = acceptLauncherGuest(ownerService, QStringLiteral("/Launcher"),
        QStringLiteral("io.github.carlsonjm.Tettegouche"), launcherGuestProtocolVersion());
    // A refusal that names a display is one Search floats on, which only its
    // own caller is given; a stranger's refusal names none.
    const QJsonObject answer = QJsonDocument::fromJson(reply.toUtf8()).object();
    const QString owner = ownerService.trimmed();
    if (answer.value(QStringLiteral("accepted")).toBool()) {
        if (owner == m_floatingLauncherOwner) forgetFloatingLauncher();
    } else if (answer.contains(QStringLiteral("output"))) {
        floatLauncher(owner);
    }
    return reply;
}

void Effect::floatLauncher(const QString &owner)
{
    if (owner == m_floatingLauncherOwner) return;
    forgetFloatingLauncher();
    m_floatingLauncherOwner = owner;
    // Search asks to begin just before it shows, and says when it hides: a
    // launcher kept ready runs on hidden, so being on the bus is not being open.
    QDBusConnection::sessionBus().connect(owner, QStringLiteral("/Launcher"),
        QStringLiteral("io.github.carlsonjm.Tettegouche"), QStringLiteral("shown"),
        this, SLOT(floatingLauncherShown(bool)));
}

void Effect::forgetFloatingLauncher()
{
    if (m_floatingLauncherOwner.isEmpty()) return;
    QDBusConnection::sessionBus().disconnect(m_floatingLauncherOwner, QStringLiteral("/Launcher"),
        QStringLiteral("io.github.carlsonjm.Tettegouche"), QStringLiteral("shown"),
        this, SLOT(floatingLauncherShown(bool)));
    m_floatingLauncherOwner.clear();
}

void Effect::floatingLauncherShown(bool shown)
{
    if (!shown) forgetFloatingLauncher();
}

void Effect::inviteFloatingLauncher()
{
    // Spread has just opened, by any of its ways, under Search floating over
    // it. Search opening again asks to begin, and is taken in beside the card
    // in the middle as one opened in Spread is; were it left floating it would
    // cover that card. Only the caller that asked is called, on its own object.
    const QString owner = m_floatingLauncherOwner;
    if (owner.isEmpty()) return;
    forgetFloatingLauncher();
    qInfo() << "Kadunce asks Search floating over the work into Spread";
    QDBusConnection::sessionBus().send(QDBusMessage::createMethodCall(owner,
        QStringLiteral("/Launcher"), QStringLiteral("io.github.carlsonjm.Tettegouche"),
        QStringLiteral("open")));
}

int Effect::companionGuestProtocolVersion() const
{
    return 1;
}

QString Effect::beginCompanionGuest(const QString &ownerService,
    const QString &objectPath, const QString &interfaceName)
{
    static const QRegularExpression path(QStringLiteral("^(/[A-Za-z0-9_]+)+$"));
    static const QRegularExpression dbusInterface(
        QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*(\\.[A-Za-z_][A-Za-z0-9_]*)+$"));
    if (!path.match(objectPath).hasMatch() || !dbusInterface.match(interfaceName).hasMatch()) {
        return QString::fromUtf8(QJsonDocument(QJsonObject{
            {QStringLiteral("protocol"), companionGuestProtocolVersion()},
            {QStringLiteral("accepted"), false},
        }).toJson(QJsonDocument::Compact));
    }
    // A companion takes the centre of a Spread already shown. Over an Active
    // card it draws its own card and needs nothing from Kadunce.
    return acceptLauncherGuest(ownerService, objectPath, interfaceName,
        companionGuestProtocolVersion());
}

QString Effect::acceptLauncherGuest(const QString &ownerService, const QString &objectPath,
    const QString &interfaceName, int protocol)
{
    QJsonObject reply{
        {QStringLiteral("protocol"), protocol},
        {QStringLiteral("accepted"), false},
    };
    const QString owner = ownerService.trimmed();
    // The guest names its own connection, so Kadunce never calls a program
    // that did not ask to be the guest.
    const bool fromBus = calledFromDBus();
    if (!owner.startsWith(QLatin1Char(':'))
        || !guestMayBegin(fromBus, fromBus ? message().service() : QString(), owner)) {
        return QString::fromUtf8(
            QJsonDocument(reply).toJson(QJsonDocument::Compact));
    }

    // The card that has focus as the guest arrives is where KWin hands focus
    // back as the guest goes.
    if (auto *focused = KWin::effects->activeWindow(); isApplicationWindow(focused)) {
        m_lastActiveApplication = focused;
    }
    if (presentationForInput() != WorkspacePresentation::Spread) {
        // A guest joins only a Spread already shown and never calls it up:
        // Search floats the same on every display (CARD-LIFECYCLE.md §11).
        // It floats where the person is: over the card in hand, or else
        // under the pointer.
        KWin::LogicalOutput *here = m_lastActiveApplication
            ? m_lastActiveApplication->screen() : nullptr;
        if (!here) here = KWin::effects->screenAt(KWin::effects->cursorPos().toPoint());
        if (here) reply.insert(QStringLiteral("output"), here->name());
        return QString::fromUtf8(
            QJsonDocument(reply).toJson(QJsonDocument::Compact));
    }
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet || !m_cardStage->beginLauncherGuest()) {
        return QString::fromUtf8(
            QJsonDocument(reply).toJson(QJsonDocument::Compact));
    }

    // One centre: the guest standing there is told to close, rather than
    // finding later that it no longer holds the place it is drawn in.
    if (!m_launcherGuestOwner.isEmpty() && m_launcherGuestOwner != owner) {
        callLauncherGuestOwner(QStringLiteral("dismissGuest"));
    }
    if (m_launcherGuestWatcher) {
        m_launcherGuestWatcher->deleteLater();
    }
    auto *watcher = new QDBusServiceWatcher(
        owner, QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForUnregistration, this);
    m_launcherGuestWatcher = watcher;
    m_launcherGuestOwner = owner;
    m_launcherGuestPath = objectPath;
    m_launcherGuestInterface = interfaceName;
    ++m_guestGeneration;
    m_launcherGuestLaunchPending = false;
    m_launcherGuestLaunchApps.clear();
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered,
            this, [this, owner](const QString &service) {
        if (service == owner && m_launcherGuestOwner == owner) {
            closeLauncherGuest();
        }
    });

    reply.insert(QStringLiteral("accepted"), true);
    m_launcherGuestExpanded = false;
    m_guestNeighborMotion.invalidate();
    reply.insert(QStringLiteral("presentationCapability"), 1);
    reply.insert(QStringLiteral("card"),
                 geometryContext(m_cardStage->launcherGuestTarget(tablet)));
    reply.insert(QStringLiteral("active"),
                 geometryContext(launcherGuestExpandedTarget(tablet)));
    reply.insert(QStringLiteral("output"), tablet->name());
    return QString::fromUtf8(
        QJsonDocument(reply).toJson(QJsonDocument::Compact));
}

bool Effect::callerIsGuest() const
{
    const bool fromBus = calledFromDBus();
    return guestMayAct(fromBus, fromBus ? message().service() : QString(), m_launcherGuestOwner);
}

bool Effect::callerMayDrive() const
{
    if (!calledFromDBus()) return true;
    // A private compositor's scenes drive Kadunce with bus tools; KWin's own
    // screenshot interface is opened to them the same way.
    static const bool unchecked = qEnvironmentVariableIntValue("KADUNCE_NO_CALLER_CHECKS") == 1;
    if (unchecked) return true;
    const QString caller = message().service();
    auto verdict = m_callerVerdicts.constFind(caller);
    if (verdict == m_callerVerdicts.cend()) {
        QString program;
        const QDBusReply<uint> pid = connection().interface()->servicePid(caller);
        if (pid.isValid()) {
            char target[PATH_MAX];
            const QByteArray link = QByteArrayLiteral("/proc/") + QByteArray::number(pid.value())
                + QByteArrayLiteral("/exe");
            const ssize_t length = ::readlink(link.constData(), target, sizeof(target) - 1);
            if (length > 0) program = QString::fromLocal8Bit(target, length);
        }
        const bool may = programMayDrive(program, QDir::homePath());
        if (!may) {
            qInfo() << "Kadunce refuses to be driven by"
                    << (program.isEmpty() ? QStringLiteral("an unknown program") : program);
        }
        if (m_callerVerdicts.size() > 256) m_callerVerdicts.clear();
        verdict = m_callerVerdicts.insert(caller, may);
    }
    if (!verdict.value()) {
        sendErrorReply(QDBusError::AccessDenied,
            QStringLiteral("Only the suite's own programs may drive Kadunce"));
    }
    return verdict.value();
}

void Effect::updateLauncherGuest(double horizontalDelta)
{
    if (m_launcherGuestExpanded || !callerIsGuest()) return;
    m_cardStage->updateLauncherGuest(horizontalDelta);
}

bool Effect::setLauncherGuestExpanded(bool expanded)
{
    if (!calledFromDBus() || message().service() != m_launcherGuestOwner
        || !m_cardStage->launcherGuestActive() || !tabletOutput()) return false;
    if (expanded != m_launcherGuestExpanded) {
        m_guestNeighborFrom = guestNeighborOpacity();
        m_guestNeighborMotion.start();
    }
    m_launcherGuestExpanded = expanded;
    m_cardStage->updateLauncherGuest(0);
    KWin::effects->addRepaintFull();
    return true;
}

KWin::Rect Effect::launcherGuestExpandedTarget(KWin::LogicalOutput *output) const
{
    if (!output) return {};
    // Floating panels may not reserve a work-area strut. Reuse the existing
    // visible-dock boundary without changing ordinary Active card geometry.
    return KWin::RectF(dockSafeGuestRect(QRectF(activeTarget(output)),
        nativeLandingAreaForOutput(output))).toRect();
}

double Effect::guestNeighborOpacity() const
{
    const double target = m_launcherGuestExpanded ? 0.0 : 1.0;
    if (!m_guestNeighborMotion.isValid() || m_guestNeighborMotion.elapsed() >= motion(220)) return target;
    const double t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
        m_guestNeighborMotion.elapsed() / double(motion(220)));
    return m_guestNeighborFrom + (target - m_guestNeighborFrom) * t;
}

bool Effect::finishLauncherGuest(double horizontalDelta)
{
    if (m_launcherGuestExpanded || !callerIsGuest()) return false;
    const bool committed =
        m_cardStage->finishLauncherGuest(horizontalDelta);
    if (committed) {
        // The layer-shell guest relinquishes keyboard focus when it hides.
        // That restoration is not a request to expand a card.
        auto *focusReturn = KWin::effects->activeWindow();
        m_guestFocusReturn = isApplicationWindow(focusReturn)
            ? focusReturn : m_cardStage->selectedWindow();
        const auto generation = m_guestGeneration;
        QTimer::singleShot(1000, this, [this, generation]() {
            if (generation == m_guestGeneration || !m_cardStage->launcherGuestActive())
                m_guestFocusReturn.clear();
        });
        QTimer::singleShot(motion(220), this, [this, generation]() {
            if (generation == m_guestGeneration && m_cardStage->launcherGuestActive()) {
                closeLauncherGuest();
            }
        });
    }
    return committed;
}

bool Effect::prepareLauncherGuestLaunch(const QStringList &applicationIds, const QString &requestToken)
{
    QStringList identities;
    for (const auto &id : applicationIds) {
        const auto normalized = LaunchIdentity::normalized(id);
        if (!normalized.isEmpty()) identities.append(normalized);
    }
    if (!m_cardStage->launcherGuestActive()
        || m_launcherGuestOwner.isEmpty() || !callerIsGuest()
        || identities.isEmpty() || identities.size() > 8 || requestToken.isEmpty()) {
        return false;
    }
    m_launcherGuestLaunchPending = true;
    m_launcherGuestLaunchApps = identities;
    m_launcherGuestLaunchToken = requestToken;
    const auto generation = ++m_guestGeneration;
    QTimer::singleShot(10000, this, [this, generation]() {
        if (generation == m_guestGeneration) dropLauncherGuestLaunch();
    });
    return true;
}

void Effect::cancelLauncherGuestLaunch()
{
    if (!callerIsGuest()) return;
    dropLauncherGuestLaunch();
}

void Effect::dropLauncherGuestLaunch()
{
    ++m_guestGeneration;
    m_launcherGuestLaunchPending = false;
    m_launcherGuestLaunchApps.clear();
    m_launcherGuestLaunchToken.clear();
}

void Effect::endLauncherGuest()
{
    if (!callerIsGuest()) return;
    closeLauncherGuest();
}

void Effect::closeLauncherGuest()
{
    // The guest's surface gives up the keyboard as it goes, and KWin hands
    // focus back to the card that had it: Spread stays for that, as it does
    // for any way a guest closes.
    if (!m_launcherGuestOwner.isEmpty()
        && presentationForInput() == WorkspacePresentation::Spread
        && !m_guestFocusReturn && m_lastActiveApplication) {
        m_guestFocusReturn = m_lastActiveApplication;
        const auto generation = ++m_guestFocusReturnGeneration;
        QTimer::singleShot(1000, this, [this, generation]() {
            if (generation == m_guestFocusReturnGeneration) m_guestFocusReturn.clear();
        });
    }
    m_launcherGuestExpanded = false;
    m_guestNeighborMotion.invalidate();
    dropLauncherGuestLaunch();
    m_cardStage->endLauncherGuest();
    m_launcherGuestOwner.clear();
    if (m_launcherGuestWatcher) {
        m_launcherGuestWatcher->deleteLater();
        m_launcherGuestWatcher = nullptr;
    }
}

} // namespace Kadunce
