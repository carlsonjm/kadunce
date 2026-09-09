/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Effect.h"
#include "CardLineLayout.h"

#include <core/output.h>
#include <core/region.h>
#include <core/renderviewport.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <input.h>
#include <opengl/glshadermanager.h>
#include <opengl/glutils.h>
#include <window.h>
#include <workspace.h>

#include <KGlobalAccel>

#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QEasingCurve>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QPainterPath>
#include <QRegion>
#include <QTimer>

#include <algorithm>

namespace Kadunce
{

namespace
{
constexpr auto Revision = "0.1.0-kadunce-baseline";
constexpr double CardCornerRadius = 10.0;
constexpr double LauncherGuestCommitDistance = 58.0;
QString windowIdentity(const KWin::EffectWindow *window)
{
    return window
        ? window->internalId().toString(QUuid::WithoutBraces)
        : QString();
}

QString applicationIdentity(const KWin::EffectWindow *window)
{
    if (!window) {
        return {};
    }
    if (window->window()) {
        if (!window->window()->desktopFileName().isEmpty()) {
            return window->window()->desktopFileName();
        }
        // Some Electron clients publish their profile path in resourceName.
        // resourceClass is the clean application-level fallback Tettegouche
        // needs for ranking and never exposes that machine-local path.
        if (!window->window()->resourceClass().isEmpty()) {
            return window->window()->resourceClass().trimmed().toLower();
        }
        if (!window->window()->resourceName().isEmpty()) {
            return window->window()->resourceName().trimmed().toLower();
        }
    }
    return window->windowClass().section(QLatin1Char(' '), -1)
        .trimmed().toLower();
}

QJsonObject geometryContext(const KWin::Rect &geometry)
{
    return {
        {QStringLiteral("x"), geometry.x()},
        {QStringLiteral("y"), geometry.y()},
        {QStringLiteral("width"), geometry.width()},
        {QStringLiteral("height"), geometry.height()},
    };
}

QString currentPosture()
{
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtime.isEmpty()) {
        return QStringLiteral("unknown");
    }
    QFile posture(runtime + QStringLiteral("/z13-tablet-kit/posture"));
    if (!posture.open(QIODevice::ReadOnly)) {
        return QStringLiteral("unknown");
    }
    const QString value = QString::fromUtf8(posture.readLine()).trimmed();
    return value.isEmpty() ? QStringLiteral("unknown") : value;
}

bool z13TabletKitAvailable()
{
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    return !runtime.isEmpty()
        && QFileInfo::exists(runtime
            + QStringLiteral("/z13-tablet-kit/posture"));
}

constexpr auto FanApertureFragmentShader = R"GLSL(#version 140

in vec2 texcoord0;
out vec4 fragColor;

#include "colormanagement.glsl"
#include "saturation.glsl"

uniform sampler2D sampler;
uniform vec4 modulation;
uniform vec2 paintSize;
uniform vec2 apertureOrigin;
uniform vec2 apertureSize;
uniform float apertureRadius;

float roundedRectangleDistance(vec2 point, vec2 size, float radius)
{
    vec2 halfSize = size * 0.5;
    vec2 local = abs(point - halfSize)
        - max(halfSize - vec2(radius), vec2(0.0));
    return length(max(local, vec2(0.0)))
        + min(max(local.x, local.y), 0.0) - radius;
}

void main(void)
{
    vec2 point = texcoord0 * paintSize - apertureOrigin;
    vec4 tex = texture(sampler, texcoord0);

    float radius = min(
        apertureRadius, 0.5 * min(apertureSize.x, apertureSize.y));
    float distanceToEdge = roundedRectangleDistance(
        point, apertureSize, radius);

    // The quad has no pixels outside its own boundary. Fade the final
    // physical pixel inside the aperture so a rotated opaque surface has
    // the same fractional coverage as a naturally translucent decoration.
    float feather = max(fwidth(distanceToEdge), 1.0);
    float coverage = 1.0
        - smoothstep(-feather, 0.0, distanceToEdge);

    tex = sourceEncodingToNitsInDestinationColorspace(tex);
    tex = adjustSaturation(tex);
    tex *= modulation;
    tex *= coverage;
    fragColor = nitsToDestinationEncoding(tex);
}
)GLSL";

KWin::Region roundedClip(const KWin::Rect &rect, double radius)
{
    QPainterPath path;
    path.addRoundedRect(
        QRectF(rect.x(), rect.y(), rect.width(), rect.height()),
        radius, radius);
    return KWin::Region(QRegion(
        path.toFillPolygon().toPolygon(), Qt::WindingFill));
}

}


Effect::Effect()
{
    m_cardStage = std::make_unique<CardStageController>(
        static_cast<CardStageHost *>(this));
    m_desktopStage = std::make_unique<DesktopStageController>(
        static_cast<DesktopStageHost *>(this));

    if (KWin::effects->isOpenGLCompositing()
        && KWin::OffscreenEffect::supported()) {
        m_fanApertureShader =
            KWin::ShaderManager::instance()->generateCustomShader(
                KWin::ShaderTrait::MapTexture, QByteArray(),
                QByteArray(FanApertureFragmentShader));
        if (m_fanApertureShader) {
            m_fanPaintSizeLocation =
                m_fanApertureShader->uniformLocation("paintSize");
            m_fanApertureOriginLocation =
                m_fanApertureShader->uniformLocation("apertureOrigin");
            m_fanApertureSizeLocation =
                m_fanApertureShader->uniformLocation("apertureSize");
            m_fanApertureRadiusLocation =
                m_fanApertureShader->uniformLocation("apertureRadius");
        }
    }

    m_toggleAction = new QAction(tr("Toggle Kadunce Card Line"), this);
    m_toggleAction->setObjectName(QStringLiteral("Kadunce Card Line"));
    KGlobalAccel::self()->setDefaultShortcut(
        m_toggleAction, {QKeySequence(QStringLiteral("Ctrl+S"))},
        KGlobalAccel::NoAutoloading);
    KGlobalAccel::self()->setShortcut(
        m_toggleAction, {QKeySequence(QStringLiteral("Ctrl+S"))},
        KGlobalAccel::NoAutoloading);
    connect(m_toggleAction, &QAction::triggered, this, &Effect::toggle);

    m_releaseAction = new QAction(tr("Release Kadunce"), this);
    m_releaseAction->setObjectName(QStringLiteral("Kadunce Release"));
    KGlobalAccel::self()->setDefaultShortcut(
        m_releaseAction, {QKeySequence(QStringLiteral("Ctrl+Esc"))},
        KGlobalAccel::NoAutoloading);
    KGlobalAccel::self()->setShortcut(
        m_releaseAction, {QKeySequence(QStringLiteral("Ctrl+Esc"))},
        KGlobalAccel::NoAutoloading);
    connect(m_releaseAction, &QAction::triggered, this, &Effect::release);

    m_previousAction = new QAction(tr("Kadunce Previous Card"), this);
    m_previousAction->setObjectName(QStringLiteral("Kadunce Previous Card"));
    KGlobalAccel::self()->setDefaultShortcut(
        m_previousAction, {QKeySequence(QStringLiteral("Ctrl+Left"))},
        KGlobalAccel::NoAutoloading);
    KGlobalAccel::self()->setShortcut(
        m_previousAction, {}, KGlobalAccel::NoAutoloading);
    m_previousAction->setEnabled(false);
    connect(m_previousAction, &QAction::triggered, this, &Effect::pageLeft);

    m_nextAction = new QAction(tr("Kadunce Next Card"), this);
    m_nextAction->setObjectName(QStringLiteral("Kadunce Next Card"));
    KGlobalAccel::self()->setDefaultShortcut(
        m_nextAction, {QKeySequence(QStringLiteral("Ctrl+Right"))},
        KGlobalAccel::NoAutoloading);
    KGlobalAccel::self()->setShortcut(
        m_nextAction, {}, KGlobalAccel::NoAutoloading);
    m_nextAction->setEnabled(false);
    connect(m_nextAction, &QAction::triggered, this, &Effect::pageRight);

    m_stackPreviousAction = new QAction(
        tr("Kadunce Previous Stacked Card"), this);
    m_stackPreviousAction->setObjectName(
        QStringLiteral("Kadunce Previous Stacked Card"));
    KGlobalAccel::self()->setDefaultShortcut(
        m_stackPreviousAction, {QKeySequence(QStringLiteral("Ctrl+Up"))},
        KGlobalAccel::NoAutoloading);
    KGlobalAccel::self()->setShortcut(
        m_stackPreviousAction, {}, KGlobalAccel::NoAutoloading);
    m_stackPreviousAction->setEnabled(false);
    connect(m_stackPreviousAction, &QAction::triggered,
            this, &Effect::pageStackUp);

    m_stackNextAction = new QAction(
        tr("Kadunce Next Stacked Card"), this);
    m_stackNextAction->setObjectName(
        QStringLiteral("Kadunce Next Stacked Card"));
    KGlobalAccel::self()->setDefaultShortcut(
        m_stackNextAction, {QKeySequence(QStringLiteral("Ctrl+Down"))},
        KGlobalAccel::NoAutoloading);
    KGlobalAccel::self()->setShortcut(
        m_stackNextAction, {}, KGlobalAccel::NoAutoloading);
    m_stackNextAction->setEnabled(false);
    connect(m_stackNextAction, &QAction::triggered,
            this, &Effect::pageStackDown);

    m_bentoAction = new QAction(tr("Toggle Kadunce Monitor Bento"), this);
    m_bentoAction->setObjectName(
        QStringLiteral("Kadunce Monitor Bento"));
    KGlobalAccel::self()->setDefaultShortcut(
        m_bentoAction, {QKeySequence(QStringLiteral("Ctrl+B"))},
        KGlobalAccel::NoAutoloading);
    KGlobalAccel::self()->setShortcut(
        m_bentoAction, {QKeySequence(QStringLiteral("Ctrl+B"))},
        KGlobalAccel::NoAutoloading);
    connect(m_bentoAction, &QAction::triggered,
            this, &Effect::toggleBento);

    // The Z13 tablet kit selects Kadunce's richer direct four-edge router.
    // Every other touchscreen delegates top and bottom gesture recognition to
    // Plasma/KWin and only receives the resulting semantic action here. The
    // router is told which backend owns those edges so one swipe cannot be
    // consumed or toggled twice.
    m_usesDirectSystemEdges = z13TabletKitAvailable();
    if (!m_usesDirectSystemEdges) {
        m_showCardLineAction = new QAction(tr("Show Kadunce Card Line"), this);
        m_showCardLineAction->setObjectName(
            QStringLiteral("Kadunce Show Card Line"));
        connect(m_showCardLineAction, &QAction::triggered,
                this, &Effect::showCardLine);
        KWin::effects->registerTouchBorder(
            KWin::ElectricBottom, m_showCardLineAction);

        m_showActiveAction = new QAction(tr("Show Kadunce Active"), this);
        m_showActiveAction->setObjectName(
            QStringLiteral("Kadunce Show Active"));
        connect(m_showActiveAction, &QAction::triggered,
                this, &Effect::showActive);
        KWin::effects->registerTouchBorder(
            KWin::ElectricTop, m_showActiveAction);
    }

    connect(KWin::effects, &KWin::EffectsHandler::windowAdded,
            this, &Effect::handleWindowAdded);
    connect(KWin::effects, &KWin::EffectsHandler::windowClosed,
            this, &Effect::handleWindowClosed);
    connect(KWin::effects, &KWin::EffectsHandler::windowActivated,
            this, &Effect::handleWindowActivated);
    connect(KWin::effects, &KWin::EffectsHandler::sessionStateChanged,
            this, &Effect::handleSessionStateChanged);
    connect(KWin::effects, &KWin::EffectsHandler::screenRemoved,
            this, &Effect::handleScreenRemoved);

    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        connectManagedWindow(window);
    }

    if (!QDBusConnection::sessionBus().registerObject(
            QStringLiteral("/Kadunce"),
            QStringLiteral("studio.warbler.Kadunce"), this,
            QDBusConnection::ExportScriptableSlots)) {
        qWarning() << "Kadunce" << Revision
                   << "could not publish the workspace context interface";
    }

    if (KWin::input()) {
        m_inputRouter = std::make_unique<WorkspaceInputRouter>(
            static_cast<WorkspaceInputTarget *>(this),
            m_usesDirectSystemEdges);
        KWin::input()->installInputEventFilter(m_inputRouter.get());
    }

    qInfo() << "Kadunce" << Revision
            << (m_usesDirectSystemEdges
                    ? "direct Z13 system edges"
                    : "Plasma-native system edges")
            << "and output-local Bento ready; fan aperture"
            << (m_fanApertureShader ? "enabled" : "r20 fallback");
}

Effect::~Effect()
{
    if (m_showCardLineAction) {
        KWin::effects->unregisterTouchBorder(
            KWin::ElectricBottom, m_showCardLineAction);
    }
    if (m_showActiveAction) {
        KWin::effects->unregisterTouchBorder(
            KWin::ElectricTop, m_showActiveAction);
    }
    QDBusConnection::sessionBus().unregisterObject(
        QStringLiteral("/Kadunce"));
    if (m_cardStage->isActive() || hasActiveDesktopStage()) {
        release();
    }
}

void Effect::showCardLine()
{
    if (presentationForInput() != WorkspacePresentation::CardLine) {
        toggle();
    }
}

void Effect::showActive()
{
    if (presentationForInput() == WorkspacePresentation::CardLine) {
        toggle();
    }
}

bool Effect::supported()
{
    return true;
}

bool Effect::isTabletOutput(const KWin::LogicalOutput *output)
{
    if (!output) {
        return false;
    }

    const QString name = output->name().toLower();
    return name.startsWith(QStringLiteral("edp"))
        || name.startsWith(QStringLiteral("dsi"))
        || name.startsWith(QStringLiteral("lvds"));
}

bool Effect::isCardWindow(const KWin::EffectWindow *window)
{
    return isApplicationWindow(window)
        && !window->isMinimized()
        && !window->isHidden();
}

bool Effect::isApplicationWindow(const KWin::EffectWindow *window)
{
    if (!window) {
        return false;
    }
    if (window->isDeleted()) {
        return false;
    }
    // Tettegouche's layer-shell surface is interactive, and KWin can expose it
    // as a normal window. It is the guest presentation itself—not an
    // application card—and admitting it here would make its own activation
    // dismiss the lease and promote the launcher into Active.
    const QString identity = applicationIdentity(window);
    if (identity.compare(
            QStringLiteral("io.github.carlsonjm.Tettegouche"),
            Qt::CaseInsensitive) == 0
        || identity.compare(QStringLiteral("tettegouche"),
                            Qt::CaseInsensitive) == 0) {
        return false;
    }
    return window->isOnCurrentDesktop()
        && window->isOnCurrentActivity()
        && (window->isNormalWindow() || window->isDialog());
}

KWin::LogicalOutput *Effect::tabletOutput() const
{
    const QList<KWin::LogicalOutput *> outputs = KWin::effects->screens();
    const auto it = std::find_if(outputs.cbegin(), outputs.cend(),
                                 [](const KWin::LogicalOutput *output) {
                                     return isTabletOutput(output);
                                 });
    return it == outputs.cend() ? nullptr : *it;
}

bool Effect::isTabletOutputForDesktopStage(
    const KWin::LogicalOutput *output) const
{
    return isTabletOutput(output);
}

bool Effect::allowsDesktopStageOnOutput(
    const KWin::LogicalOutput *output) const
{
    if (!output) {
        return false;
    }
    if (!isTabletOutput(output)) {
        return true;
    }
    const QList<KWin::LogicalOutput *> outputs = KWin::effects->screens();
    return std::none_of(
        outputs.cbegin(), outputs.cend(),
        [output](const KWin::LogicalOutput *candidate) {
            return candidate && candidate != output
                && !isTabletOutput(candidate);
        });
}

bool Effect::isManagedWindowForDesktopStage(
    const KWin::EffectWindow *window) const
{
    return isCardWindow(window);
}

KWin::LogicalOutput *Effect::tabletOutputForDesktopStage() const
{
    return tabletOutput();
}

KWin::Rect Effect::activeTargetForDesktopStage(
    KWin::LogicalOutput *output) const
{
    return activeTarget(output);
}

void Effect::prepareOutputForDesktopStage(KWin::LogicalOutput *output)
{
    if (output && isTabletOutput(output) && m_cardStage->isActive()) {
        release();
    }
}

KWin::LogicalOutput *Effect::tabletOutputForCardStage() const
{
    return tabletOutput();
}

bool Effect::isTabletOutputForCardStage(
    const KWin::LogicalOutput *output) const
{
    return isTabletOutput(output);
}

bool Effect::isManagedWindowForCardStage(
    const KWin::EffectWindow *window) const
{
    return isCardWindow(window);
}

void Effect::setPagingShortcutsForCardStage(bool active)
{
    setPagingShortcutsActive(active);
}

void Effect::connectManagedWindowForCardStage(KWin::EffectWindow *window)
{
    connectManagedWindow(window);
}

void Effect::unredirectForCardStage(KWin::EffectWindow *window)
{
    unredirect(window);
}

bool Effect::admitCardToDesktopStage(
    KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const KWin::RectF &geometry)
{
    return m_desktopStage->admitCardWindow(window, output, geometry);
}

WorkspacePresentation Effect::presentationForInput() const
{
    if (!m_cardStage->isActive()) {
        return WorkspacePresentation::Inactive;
    }
    return m_cardStage->presentation() == CardPresentation::CardLine
        ? WorkspacePresentation::CardLine
        : WorkspacePresentation::Active;
}

WorkspaceInputGeometry Effect::geometryForInput() const
{
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet) {
        return {};
    }
    const KWin::Rect tabletRect = tablet->geometry();
    const KWin::Rect centerCard = cardTargetForSlot(tablet, 0);
    return {
        QRectF(tabletRect),
        QRectF(centerCard),
        double(tabletRect.bottom()),
        double(centerCard.right()),
    };
}

bool Effect::cardGrabActiveForInput() const
{
    return m_cardStage->cardGrabActive();
}

bool Effect::stackPreviewArmedForInput() const
{
    return m_cardStage->stackPreviewArmed();
}

int Effect::stackPreviewTargetForInput() const
{
    return m_cardStage->stackPreviewTarget();
}

bool Effect::centerCardContainsForInput(const QPointF &position) const
{
    KWin::LogicalOutput *tablet = tabletOutput();
    return tablet
        && cardTargetForSlot(tablet, 0).contains(position.toPoint());
}

bool Effect::launcherGuestActiveForInput() const
{
    return m_cardStage->launcherGuestActive();
}

bool Effect::launcherGuestContainsForInput(const QPointF &position) const
{
    KWin::LogicalOutput *tablet = tabletOutput();
    return m_cardStage->launcherGuestActive() && tablet
        && m_cardStage->launcherGuestTarget(tablet)
               .contains(position.toPoint());
}

void Effect::toggleFromInput()
{
    toggle();
}

void Effect::dismissLauncherGuestFromInput()
{
    if (!m_launcherGuestOwner.isEmpty()) {
        QDBusMessage request = QDBusMessage::createMethodCall(
            m_launcherGuestOwner,
            QStringLiteral("/Launcher"),
            QStringLiteral("io.github.carlsonjm.Tettegouche"),
            QStringLiteral("dismissGuest"));
        QDBusConnection::sessionBus().asyncCall(request);
    }
    endLauncherGuest();
}

void Effect::navigateLauncherGuestFromInput(const QPointF &position)
{
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet || !m_cardStage->launcherGuestActive()) {
        return;
    }
    const KWin::Rect left =
        m_cardStage->launcherGuestTargetForSlot(tablet, -1);
    const KWin::Rect right =
        m_cardStage->launcherGuestTargetForSlot(tablet, 1);
    const int slot = left.contains(position.toPoint()) ? -1
        : (right.contains(position.toPoint()) ? 1 : 0);
    if (slot == 0) {
        dismissLauncherGuestFromInput();
        return;
    }

    if (!m_launcherGuestOwner.isEmpty()) {
        QDBusMessage navigate = QDBusMessage::createMethodCall(
            m_launcherGuestOwner,
            QStringLiteral("/Launcher"),
            QStringLiteral("io.github.carlsonjm.Tettegouche"),
            QStringLiteral("completeGuestNavigation"));
        navigate.setArguments({slot});
        QDBusConnection::sessionBus().asyncCall(navigate);
    }
    // Moving the guest right reveals the left neighbor; moving it left reveals
    // the right neighbor. CardStage applies the matching selection only when
    // the transition completes.
    finishLauncherGuest(slot < 0
        ? LauncherGuestCommitDistance : -LauncherGuestCommitDistance);
}

void Effect::pageLeftFromInput()
{
    pageLeft();
}

void Effect::pageRightFromInput()
{
    pageRight();
}

void Effect::pageStackFromInput(int delta)
{
    pageStack(delta);
}

bool Effect::hasActiveDesktopStage() const
{
    return m_desktopStage && m_desktopStage->hasActiveSession();
}

void Effect::toggleBento()
{
    m_desktopStage->toggleUnderPointer();
}

void Effect::connectManagedWindow(KWin::EffectWindow *window)
{
    if (!window) {
        return;
    }
    connect(window, &KWin::EffectWindow::windowFrameGeometryChanged,
            this, &Effect::handleActiveGeometryChanged,
            Qt::UniqueConnection);
    connect(window, &KWin::EffectWindow::windowStartUserMovedResized,
            this, &Effect::handleWindowMoveResizeStarted,
            Qt::UniqueConnection);
    connect(window, &KWin::EffectWindow::windowStepUserMovedResized,
            this, &Effect::handleWindowMoveResizeStepped,
            Qt::UniqueConnection);
    connect(window, &KWin::EffectWindow::windowFinishUserMovedResized,
            this, &Effect::handleWindowMoveResizeFinished,
            Qt::UniqueConnection);
}

void Effect::handleWindowMoveResizeStarted(KWin::EffectWindow *window)
{
    m_desktopStage->handleWindowMoveResizeStarted(window);
}

void Effect::handleWindowMoveResizeStepped(
    KWin::EffectWindow *window, const KWin::RectF &geometry)
{
    m_desktopStage->handleWindowMoveResizeStepped(window, geometry);
}

void Effect::handleWindowMoveResizeFinished(KWin::EffectWindow *window)
{
    m_desktopStage->handleWindowMoveResizeFinished(window);
}

void Effect::admitTransferredWindowToTablet(KWin::EffectWindow *window)
{
    m_cardStage->admitTransferredWindowToTablet(window);
}

void Effect::handleScreenRemoved(KWin::LogicalOutput *output)
{
    m_desktopStage->handleScreenRemoved(output);
}

bool Effect::isTabletPoint(const QPointF &position) const
{
    KWin::LogicalOutput *tablet = tabletOutput();
    return tablet && tablet->geometry().contains(position.toPoint());
}

QStringList Effect::outputStageState() const
{
    return m_desktopStage->outputStageState();
}

QString Effect::workspaceContext() const
{
    const CardLineModel &model = m_cardStage->model();
    const QList<QPointer<KWin::EffectWindow>> &cards =
        m_cardStage->liveCards();
    KWin::EffectWindow *focused = KWin::effects->activeWindow();

    const auto cardIdentity = [&cards](int cardId) {
        const int index = cardId - 1;
        return index >= 0 && index < cards.size()
            ? windowIdentity(cards.at(index).data()) : QString();
    };

    QJsonArray applications;
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        if (!isApplicationWindow(window)) {
            continue;
        }
        const int cardIndex = m_cardStage->liveCardIndex(window);
        const int cardId = cardIndex + 1;
        QJsonObject application{
            {QStringLiteral("windowId"), windowIdentity(window)},
            {QStringLiteral("appId"), applicationIdentity(window)},
            {QStringLiteral("title"), window->caption()},
            {QStringLiteral("output"),
             window->screen() ? window->screen()->name() : QString()},
            {QStringLiteral("focused"), window == focused},
            {QStringLiteral("minimized"), window->isMinimized()},
            {QStringLiteral("hasCard"), cardIndex >= 0},
        };
        if (cardIndex >= 0) {
            const std::vector<int> members = model.stackMembersForId(cardId);
            application.insert(QStringLiteral("cardId"),
                               windowIdentity(window));
            application.insert(QStringLiteral("cardIndex"), cardIndex + 1);
            application.insert(QStringLiteral("stackId"),
                               members.empty()
                                   ? QString()
                                   : cardIdentity(members.front()));
            application.insert(QStringLiteral("stackPosition"),
                               model.stackPositionForId(cardId) + 1);
            application.insert(QStringLiteral("stackSize"),
                               model.stackSizeForId(cardId));
            application.insert(QStringLiteral("selected"),
                               model.selectedId() == cardId);
        }
        applications.append(application);
    }

    QJsonObject focus;
    if (focused && isApplicationWindow(focused)) {
        const int focusedCardIndex = m_cardStage->liveCardIndex(focused);
        focus = {
            {QStringLiteral("windowId"), windowIdentity(focused)},
            {QStringLiteral("appId"), applicationIdentity(focused)},
            {QStringLiteral("title"), focused->caption()},
            {QStringLiteral("hasCard"), focusedCardIndex >= 0},
        };
        if (focusedCardIndex >= 0) {
            focus.insert(QStringLiteral("cardId"),
                         windowIdentity(focused));
        }
    }

    QJsonArray selectedStack;
    QString selectedCardId;
    if (m_cardStage->isActive() && !cards.isEmpty()) {
        selectedCardId = cardIdentity(model.selectedId());
        for (const int member : model.stackMembersForId(model.selectedId())) {
            selectedStack.append(cardIdentity(member));
        }
    }

    const QString presentation = !m_cardStage->isActive()
        ? QStringLiteral("inactive")
        : m_cardStage->presentation() == CardPresentation::CardLine
            ? QStringLiteral("cardLine") : QStringLiteral("active");

    QJsonArray displays;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (!output) {
            continue;
        }
        const bool tablet = isTabletOutput(output);
        displays.append(QJsonObject{
            {QStringLiteral("name"), output->name()},
            {QStringLiteral("role"), tablet
                ? QStringLiteral("tablet") : QStringLiteral("external")},
            {QStringLiteral("geometry"), geometryContext(output->geometry())},
            {QStringLiteral("bentoActive"),
             m_desktopStage->hasSessionOnOutput(output->name())},
        });
    }

    QJsonObject root{
        {QStringLiteral("schema"),
         QStringLiteral("studio.warbler.kadunce.workspace-context")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("focus"), focus.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(focus)},
        {QStringLiteral("applications"), applications},
        {QStringLiteral("cardStage"), QJsonObject{
            {QStringLiteral("active"), m_cardStage->isActive()},
            {QStringLiteral("presentation"), presentation},
            {QStringLiteral("launcherGuestActive"),
             m_cardStage->launcherGuestActive()},
            {QStringLiteral("selectedCardId"), selectedCardId.isEmpty()
                ? QJsonValue(QJsonValue::Null)
                : QJsonValue(selectedCardId)},
            {QStringLiteral("selectedStack"), selectedStack},
        }},
        {QStringLiteral("desktopStage"), QJsonObject{
            {QStringLiteral("active"), hasActiveDesktopStage()},
        }},
        {QStringLiteral("displayContext"), QJsonObject{
            {QStringLiteral("posture"), currentPosture()},
            {QStringLiteral("edgeBackend"), m_usesDirectSystemEdges
                ? QStringLiteral("z13-direct")
                : QStringLiteral("plasma-native")},
            {QStringLiteral("displays"), displays},
        }},
    };
    return QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool Effect::activateApplicationWindow(const QString &windowId)
{
    const QString requested = windowId.trimmed();
    if (requested.isEmpty()) {
        return false;
    }

    const QList<KWin::EffectWindow *> windows =
        KWin::effects->stackingOrder();
    for (auto iterator = windows.crbegin(); iterator != windows.crend();
         ++iterator) {
        KWin::EffectWindow *window = *iterator;
        if (!isApplicationWindow(window)
            || windowIdentity(window) != requested
            || !window->window()) {
            continue;
        }

        if (window->isMinimized()) {
            window->unminimize();
        }
        endLauncherGuest();
        KWin::workspace()->raiseWindow(window->window());
        KWin::workspace()->activateWindow(window->window(), true);
        return true;
    }
    return false;
}

int Effect::launcherGuestProtocolVersion() const
{
    return 2;
}

QString Effect::beginLauncherGuest(const QString &ownerService)
{
    QJsonObject reply{
        {QStringLiteral("protocol"), launcherGuestProtocolVersion()},
        {QStringLiteral("accepted"), false},
    };
    const QString owner = ownerService.trimmed();
    if (owner.isEmpty()) {
        return QString::fromUtf8(
            QJsonDocument(reply).toJson(QJsonDocument::Compact));
    }

    if (presentationForInput() != WorkspacePresentation::CardLine) {
        showCardLine();
    }
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet || !m_cardStage->beginLauncherGuest()) {
        return QString::fromUtf8(
            QJsonDocument(reply).toJson(QJsonDocument::Compact));
    }

    if (m_launcherGuestWatcher) {
        m_launcherGuestWatcher->deleteLater();
    }
    auto *watcher = new QDBusServiceWatcher(
        owner, QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForUnregistration, this);
    m_launcherGuestWatcher = watcher;
    m_launcherGuestOwner = owner;
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered,
            this, [this, owner](const QString &service) {
        if (service == owner) {
            endLauncherGuest();
        }
    });

    reply.insert(QStringLiteral("accepted"), true);
    reply.insert(QStringLiteral("card"),
                 geometryContext(m_cardStage->launcherGuestTarget(tablet)));
    reply.insert(QStringLiteral("active"),
                 geometryContext(activeTarget(tablet)));
    reply.insert(QStringLiteral("output"), tablet->name());
    return QString::fromUtf8(
        QJsonDocument(reply).toJson(QJsonDocument::Compact));
}

void Effect::updateLauncherGuest(double horizontalDelta)
{
    m_cardStage->updateLauncherGuest(horizontalDelta);
}

bool Effect::finishLauncherGuest(double horizontalDelta)
{
    const bool committed =
        m_cardStage->finishLauncherGuest(horizontalDelta);
    if (committed) {
        QTimer::singleShot(220, this, [this]() {
            if (m_cardStage->launcherGuestActive()) {
                endLauncherGuest();
            }
        });
    }
    return committed;
}

bool Effect::prepareLauncherGuestLaunch()
{
    if (!m_cardStage->launcherGuestActive()
        || m_launcherGuestOwner.isEmpty()) {
        return false;
    }
    m_launcherGuestLaunchPending = true;
    return true;
}

void Effect::cancelLauncherGuestLaunch()
{
    m_launcherGuestLaunchPending = false;
}

void Effect::endLauncherGuest()
{
    m_launcherGuestLaunchPending = false;
    m_cardStage->endLauncherGuest();
    m_launcherGuestOwner.clear();
    if (m_launcherGuestWatcher) {
        m_launcherGuestWatcher->deleteLater();
        m_launcherGuestWatcher = nullptr;
    }
}

bool Effect::toggleBentoOnOutput(const QString &outputName)
{
    return m_desktopStage->toggleOnOutput(outputName);
}

bool Effect::handoffBentoLeadToOutput(
    const QString &sourceName, const QString &destinationName)
{
    return m_desktopStage->handoffLeadToOutput(
        sourceName, destinationName);
}

int Effect::activeSideForPoint(const QPointF &position) const
{
    return m_cardStage->activeSideForPoint(position);
}

bool Effect::selectedStackContains(const QPointF &position) const
{
    return m_cardStage->selectedStackContains(position);
}

void Effect::beginCardGrab()
{
    m_cardStage->beginCardGrab();
}

void Effect::updateCardGrab(double horizontalDelta)
{
    m_cardStage->updateCardGrab(horizontalDelta);
}

void Effect::updateCardGrabDestination(const QPointF &position)
{
    m_cardStage->updateCardGrabDestination(position);
}

void Effect::pageCardGrab(int direction)
{
    m_cardStage->pageCardGrab(direction);
}

void Effect::finishCardGrab(bool commit)
{
    m_cardStage->finishCardGrab(commit);
}

bool Effect::finishCardGrabOnOutput(const QPointF &position)
{
    return m_cardStage->finishCardGrabOnOutput(position);
}

int Effect::cardStackCandidate() const
{
    return m_cardStage->cardStackCandidate();
}

int Effect::cardStackBrowseTarget() const
{
    return m_cardStage->stackBrowseTarget();
}

void Effect::setCardStackPreview(int destinationId)
{
    m_cardStage->setCardStackPreview(destinationId);
}

bool Effect::pageCardStackInsertion(int direction)
{
    return m_cardStage->pageCardStackInsertion(direction);
}

double Effect::cardStackInsertionBlend() const
{
    return m_cardStage->stackInsertionBlend();
}

void Effect::clearCardStackPreview()
{
    m_cardStage->clearCardStackPreview();
}

double Effect::cardStackPreviewBlend() const
{
    return m_cardStage->stackPreviewBlend();
}

void Effect::syncSelectedElevation()
{
    m_cardStage->syncSelectedElevation();
}

void Effect::setPagingShortcutsActive(bool active)
{
    const QList<QKeySequence> previous{
        QKeySequence(QStringLiteral("Ctrl+Left"))};
    const QList<QKeySequence> next{
        QKeySequence(QStringLiteral("Ctrl+Right"))};
    const QList<QKeySequence> stackPrevious{
        QKeySequence(QStringLiteral("Ctrl+Up"))};
    const QList<QKeySequence> stackNext{
        QKeySequence(QStringLiteral("Ctrl+Down"))};

    m_previousAction->setEnabled(active);
    m_nextAction->setEnabled(active);
    m_stackPreviousAction->setEnabled(active);
    m_stackNextAction->setEnabled(active);
    if (active) {
        KGlobalAccel::self()->setShortcut(
            m_previousAction, previous, KGlobalAccel::NoAutoloading);
        KGlobalAccel::self()->setShortcut(
            m_nextAction, next, KGlobalAccel::NoAutoloading);
        KGlobalAccel::self()->setShortcut(
            m_stackPreviousAction, stackPrevious,
            KGlobalAccel::NoAutoloading);
        KGlobalAccel::self()->setShortcut(
            m_stackNextAction, stackNext,
            KGlobalAccel::NoAutoloading);
    } else {
        KGlobalAccel::self()->setShortcut(
            m_previousAction, {}, KGlobalAccel::NoAutoloading);
        KGlobalAccel::self()->setShortcut(
            m_nextAction, {}, KGlobalAccel::NoAutoloading);
        KGlobalAccel::self()->setShortcut(
            m_stackPreviousAction, {}, KGlobalAccel::NoAutoloading);
        KGlobalAccel::self()->setShortcut(
            m_stackNextAction, {}, KGlobalAccel::NoAutoloading);
    }
}

void Effect::activateSelectedFromInput()
{
    // Defer the state change so the input filter is never destroyed from
    // inside one of its own callbacks.
    QTimer::singleShot(0, this, [this]() {
        if (m_cardStage->isActive()
            && m_cardStage->presentation() == CardPresentation::CardLine) {
            toggle();
        }
    });
}

void Effect::toggle()
{
    if (m_cardStage->launcherGuestActive()) {
        dismissLauncherGuestFromInput();
    }
    if (!m_cardStage->isActive()) {
        KWin::LogicalOutput *tablet = tabletOutput();
        if (tablet && m_desktopStage->hasSessionOnOutput(tablet->name())) {
            m_desktopStage->toggleOnOutput(tablet->name());
        }
    }
    m_cardStage->toggle();
}

void Effect::release()
{
    const bool hadBento = hasActiveDesktopStage();
    if (hadBento) {
        m_desktopStage->stopPendingSettle();
        m_desktopStage->restoreAllSessions();
    }
    const bool hadCards = m_cardStage->isActive();
    m_cardStage->release();
    m_paintingOutput = nullptr;
    if (!hadCards && hadBento) {
        qInfo() << "Kadunce" << Revision
                << "released output-local Bento";
    }
}

void Effect::pageLeft()
{
    pageHorizontal(-1);
}

void Effect::pageRight()
{
    pageHorizontal(1);
}

void Effect::pageHorizontal(int delta)
{
    if (m_cardStage->launcherGuestActive()) {
        dismissLauncherGuestFromInput();
    }
    m_cardStage->pageHorizontal(delta);
}

void Effect::pageStackUp()
{
    pageStack(-1);
}

void Effect::pageStackDown()
{
    pageStack(1);
}

void Effect::pageStack(int delta)
{
    if (m_cardStage->launcherGuestActive()) {
        dismissLauncherGuestFromInput();
    }
    m_cardStage->pageStack(delta);
}

void Effect::prePaintScreen(KWin::ScreenPrePaintData &data)
{
    if (m_cardStage->isActive() && isTabletOutput(data.screen)) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        if (m_cardStage->animationsRunning()) {
            KWin::effects->addRepaintFull();
        }
    }
    KWin::effects->prePaintScreen(data);
}

void Effect::prePaintWindow(KWin::RenderView *view,
                            KWin::EffectWindow *window,
                            KWin::WindowPrePaintData &data)
{
    if (m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::CardLine
        && visibleSlot(window) != 99) {
        data.setTransformed();
        // A rotated opaque client needs compositor blending for the
        // fractional coverage emitted by the r21 fan aperture.
        data.setTranslucent();
    }
    KWin::OffscreenEffect::prePaintWindow(view, window, data);
}

void Effect::paintScreen(const KWin::RenderTarget &renderTarget,
                         const KWin::RenderViewport &viewport,
                         int mask,
                         const KWin::Region &deviceRegion,
                         KWin::LogicalOutput *screen)
{
    m_paintingOutput = screen;
    KWin::effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
    m_paintingOutput = nullptr;
}

KWin::EffectWindow *Effect::selectedWindow() const
{
    return m_cardStage->selectedWindow();
}

void Effect::handleWindowAdded(KWin::EffectWindow *window)
{
    connectManagedWindow(window);
    const QPointer<KWin::EffectWindow> candidate(window);
    QTimer::singleShot(0, this, [this, candidate]() {
        if (!candidate || !isCardWindow(candidate)) {
            return;
        }
        if (m_desktopStage->handleWindowAdded(candidate)) {
            return;
        }
        (void)m_cardStage->handleWindowAdded(candidate);
    });
}

void Effect::handleWindowClosed(KWin::EffectWindow *window)
{
    m_desktopStage->handleWindowClosed(window);
    m_cardStage->handleWindowClosed(window);
}

void Effect::handleWindowActivated(KWin::EffectWindow *window)
{
    if (m_cardStage->launcherGuestActive()
        && isApplicationWindow(window)) {
        if (m_launcherGuestLaunchPending) {
            m_launcherGuestLaunchPending = false;
            const QPointer<KWin::EffectWindow> launched(window);
            if (!m_launcherGuestOwner.isEmpty()) {
                QDBusMessage ready = QDBusMessage::createMethodCall(
                    m_launcherGuestOwner,
                    QStringLiteral("/Launcher"),
                    QStringLiteral("io.github.carlsonjm.Tettegouche"),
                    QStringLiteral("completeGuestLaunch"));
                QDBusConnection::sessionBus().asyncCall(ready);
            }
            QTimer::singleShot(220, this, [this, launched]() {
                endLauncherGuest();
                if (launched && !launched->isDeleted()) {
                    m_cardStage->handleWindowActivated(launched);
                }
            });
            return;
        }
        dismissLauncherGuestFromInput();
    }
    m_cardStage->handleWindowActivated(window);
}

void Effect::handleActiveGeometryChanged(KWin::EffectWindow *window,
                                         const KWin::RectF &)
{
    m_cardStage->handleActiveGeometryChanged(window);
}

void Effect::handleSessionStateChanged()
{
    if ((m_cardStage->isActive() || hasActiveDesktopStage())
        && KWin::effects->sessionState() != KWin::SessionState::Normal) {
        qInfo() << "Kadunce" << Revision
                << "restoring managed windows before session save or shutdown";
        release();
    }
}

int Effect::liveCardIndex(const KWin::EffectWindow *window) const
{
    return m_cardStage->liveCardIndex(window);
}

int Effect::visibleSlot(const KWin::EffectWindow *window) const
{
    return m_cardStage->visibleSlot(window);
}

KWin::Rect Effect::cardTargetForSlot(KWin::LogicalOutput *output,
                                    int slot) const
{
    return m_cardStage->cardTargetForSlot(output, slot);
}

KWin::Rect Effect::activeTarget(KWin::LogicalOutput *output) const
{
    return m_cardStage->activeTarget(output);
}

void Effect::drawWindow(const KWin::RenderTarget &renderTarget,
                        const KWin::RenderViewport &viewport,
                        KWin::EffectWindow *window,
                        int mask,
                        const KWin::Region &deviceRegion,
                        KWin::WindowPaintData &data)
{
    const bool useFanAperture = window == m_fanApertureWindow
        && m_fanApertureShader
        && !m_fanPaintSize.isEmpty()
        && !m_fanApertureSize.isEmpty()
        && m_fanPaintSizeLocation >= 0
        && m_fanApertureOriginLocation >= 0
        && m_fanApertureSizeLocation >= 0
        && m_fanApertureRadiusLocation >= 0;

    if (!useFanAperture) {
        // Redirection is intentionally ephemeral. Active, Release, ordinary
        // Card Line faces, hidden deck members, and shader failure all return
        // to KWin's exact r20 direct path.
        unredirect(window);
        KWin::OffscreenEffect::drawWindow(
            renderTarget, viewport, window, mask, deviceRegion, data);
        return;
    }

    redirect(window);
    setShader(window, m_fanApertureShader.get());
    KWin::ShaderManager::instance()->pushShader(
        m_fanApertureShader.get());
    m_fanApertureShader->setUniform(
        m_fanPaintSizeLocation,
        QVector2D(static_cast<float>(m_fanPaintSize.width()),
                  static_cast<float>(m_fanPaintSize.height())));
    m_fanApertureShader->setUniform(
        m_fanApertureOriginLocation,
        QVector2D(static_cast<float>(m_fanApertureOrigin.x()),
                  static_cast<float>(m_fanApertureOrigin.y())));
    m_fanApertureShader->setUniform(
        m_fanApertureSizeLocation,
        QVector2D(static_cast<float>(m_fanApertureSize.width()),
                  static_cast<float>(m_fanApertureSize.height())));
    m_fanApertureShader->setUniform(
        m_fanApertureRadiusLocation, m_fanApertureRadius);
    glActiveTexture(GL_TEXTURE0);

    KWin::OffscreenEffect::drawWindow(
        renderTarget, viewport, window, mask, deviceRegion, data);
    KWin::ShaderManager::instance()->popShader();
}

void Effect::paintWindow(const KWin::RenderTarget &renderTarget,
                         const KWin::RenderViewport &viewport,
                         KWin::EffectWindow *window,
                         int mask,
                         const KWin::Region &deviceRegion,
                         KWin::WindowPaintData &data)
{
    if (!m_cardStage->isActive() || !m_paintingOutput
        || !isCardWindow(window)) {
        KWin::effects->paintWindow(
            renderTarget, viewport, window, mask, deviceRegion, data);
        return;
    }

    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet) {
        KWin::effects->paintWindow(
            renderTarget, viewport, window, mask, deviceRegion, data);
        return;
    }

    if (m_paintingOutput != tablet) {
        // A tablet-owned surface never paints into an external output while
        // the gate is active. Every external-owned surface follows KWin's
        // ordinary paint chain with no replacement scene or input surface.
        if (window->screen() != tablet) {
            KWin::effects->paintWindow(
                renderTarget, viewport, window, mask, deviceRegion, data);
        }
        return;
    }

    const int slot = visibleSlot(window);
    if (window->screen() != tablet || slot == 99) {
        return;
    }

    if (m_cardStage->presentation() == CardPresentation::Active) {
        KWin::effects->paintWindow(
            renderTarget, viewport, window, mask, deviceRegion, data);
        return;
    }

    KWin::Rect logicalRegion = window->expandedGeometry().toRect();
    KWin::Rect target = cardTargetForSlot(tablet, slot);
    double launcherGuestRotation = 0.0;
    if (m_cardStage->launcherGuestActive()) {
        target = m_cardStage->launcherGuestTargetForSlot(tablet, slot);
        const double offset = m_cardStage->launcherGuestOffset();
        const double progress =
            m_cardStage->launcherGuestTransitionProgress();
        const bool incoming = (offset < 0.0 && slot == 1)
            || (offset > 0.0 && slot == -1);
        if (incoming) {
            const KWin::Rect center = cardTargetForSlot(tablet, 0);
            const auto blend = [progress](int from, int to) {
                return qRound(from + (to - from) * progress);
            };
            target = KWin::Rect(blend(target.x(), center.x()),
                                blend(target.y(), center.y()),
                                blend(target.width(), center.width()),
                                blend(target.height(), center.height()));

            // Anticipate the handoff without changing either endpoint: the
            // incoming shoulder rises and leans during the user's drag, then
            // settles flat as it falls into the canonical center rectangle.
            constexpr double PreviewLimit = 0.18;
            const double settleSpan = 1.0 - PreviewLimit;
            const double liftBlend = progress <= PreviewLimit
                ? progress / PreviewLimit
                : (1.0 - progress) / settleSpan;
            target.translate(0, qRound(-16.0 * liftBlend));
            launcherGuestRotation = (slot < 0 ? -0.8 : 0.8)
                * liftBlend;
        }
    }
    const CardLineModel &cardLine = m_cardStage->model();
    const int cardId = liveCardIndex(window) + 1;
    const bool grabbedWindow = m_cardStage->cardGrabActive()
        && cardId == cardLine.selectedId();
    const double previewBlend = cardStackPreviewBlend();
    CardStackPose paintPose{0.0, 0.0, 0.0, true};
    if (grabbedWindow) {
        CardStackPose previewPose{0.0, 0.0, 0.0, true};
        if (m_cardStage->stackPreviewTarget() != 0) {
            const int destinationSize =
                cardLine.stackSizeForId(m_cardStage->stackPreviewTarget());
            previewPose = makeInsertionStackPose(
                m_cardStage->stackInsertionIndex(), destinationSize + 1,
                m_cardStage->stackInsertionIndex(), target.width());
        }
        const double heldScaleInset = 0.05 * (1.0 - previewBlend);
        const int insetX = qRound(target.width() * heldScaleInset);
        const int insetY = qRound(target.height() * heldScaleInset);
        target.adjust(insetX, insetY, -insetX, -insetY);
        const double x = m_cardStage->cardGrabOffset()
                * (1.0 - previewBlend)
            + previewPose.x * previewBlend;
        const double y = -24.0 * (1.0 - previewBlend)
            + previewPose.y * previewBlend;
        target.translate(qRound(x), qRound(y));
        paintPose.rotation = previewPose.rotation * previewBlend;
    } else {
        const int memberCount = cardLine.stackSizeForId(cardId);
        const int memberIndex = cardLine.stackPositionForId(cardId);
        const int activeIndex = cardLine.stackActivePositionForId(cardId);
        CardStackPose pose{0.0, 0.0, 0.0, true};
        const bool previewDestination = m_cardStage->cardGrabActive()
            && m_cardStage->stackPreviewTarget() != 0
            && cardLine.sameStack(
                cardId, m_cardStage->stackPreviewTarget());
        const int browseTarget = cardStackBrowseTarget();
        const bool browseDestination = m_cardStage->cardGrabActive()
            && browseTarget != 0
            && cardLine.sameStack(cardId, browseTarget);
        if (memberCount > 1 || previewDestination || browseDestination) {
            const CardStackPose closed = makeClosedStackPose(
                memberIndex, memberCount,
                tablet->geometry().width());
            if (previewDestination) {
                const int insertion = std::clamp(
                    m_cardStage->stackInsertionIndex(), 0, memberCount);
                const int previousInsertion = std::clamp(
                    m_cardStage->previousStackInsertionIndex(),
                    0, memberCount);
                const int previewMemberIndex = memberIndex >= insertion
                    ? memberIndex + 1 : memberIndex;
                const int previousMemberIndex =
                    memberIndex >= previousInsertion
                    ? memberIndex + 1 : memberIndex;
                const CardStackPose previous = makeInsertionStackPose(
                    previousMemberIndex, memberCount + 1,
                    previousInsertion, target.width());
                const CardStackPose next = makeInsertionStackPose(
                    previewMemberIndex, memberCount + 1,
                    insertion, target.width());
                const double insertionBlend = cardStackInsertionBlend();
                const CardStackPose opened{
                    previous.x + (next.x - previous.x) * insertionBlend,
                    previous.y + (next.y - previous.y) * insertionBlend,
                    previous.rotation
                        + (next.rotation - previous.rotation)
                            * insertionBlend,
                    previous.visible || next.visible,
                };
                pose = {
                    closed.x + (opened.x - closed.x) * previewBlend,
                    closed.y + (opened.y - closed.y) * previewBlend,
                    closed.rotation
                        + (opened.rotation - closed.rotation) * previewBlend,
                    opened.visible,
                };
            } else if (browseDestination) {
                pose = makeOpenStackPose(
                    memberIndex, memberCount,
                    activeIndex, target.width());
            } else if (!m_cardStage->cardGrabActive()
                       && cardLine.sameStack(
                           cardId, cardLine.selectedId())) {
                pose = makeOpenStackPose(
                    memberIndex, memberCount,
                    activeIndex, target.width());
            } else {
                pose = closed;
            }
        }
        target.translate(qRound(pose.x), qRound(pose.y));
        paintPose = pose;
    }
    paintPose.rotation += launcherGuestRotation;
    if (!paintPose.visible) {
        return;
    }
    const bool rotatedFanCard = !qFuzzyIsNull(paintPose.rotation);
    // Every card uses the same proportional cover transform. Rotated fan
    // members are cropped by the GPU aperture below instead of being stretched
    // into the slot, so Discord, Spotify, and every other aspect ratio retain
    // their native proportions without escaping the fixed card envelope.
    KWin::Effect::setPositionTransformations(
        data, logicalRegion, window, target,
        Qt::KeepAspectRatioByExpanding);
    if (rotatedFanCard) {
        // Keep the already-approved reference layout pose anchored to the card's bottom
        // right, not the larger proportional cover rectangle's bottom right.
        const double originX =
            (target.x() + target.width() - logicalRegion.x())
            / data.xScale();
        const double originY =
            (target.y() + target.height() - logicalRegion.y())
            / data.yScale();
        data.setRotationAngle(paintPose.rotation);
        data.setRotationOrigin(QVector3D(originX, originY, 0.0));
    }

    // OffscreenEffect snapshots expandedGeometry(), including decoration
    // shadows, while setPositionTransformations() maps the window frame. Map
    // that expanded texture through the same proportional scale so the shader
    // aperture origin is measured against the pixels it actually samples.
    const KWin::RectF expanded = window->expandedGeometry();
    const KWin::RectF frame = window->frameGeometry();
    const KWin::RectF paintRegion(
        logicalRegion.x() + (expanded.x() - frame.x()) * data.xScale(),
        logicalRegion.y() + (expanded.y() - frame.y()) * data.yScale(),
        expanded.width() * data.xScale(),
        expanded.height() * data.yScale());

    // deviceRegion is KWin's actual renderer clip. Intersecting it with the
    // fixed slot produces a native per-card aperture: every live surface
    // covers the same rectangle, while excess pixels remain compositor-only
    // and can never alter the Card Line pitch or cross an output boundary.
    const KWin::Rect deviceTarget =
        viewport.mapToDeviceCoordinatesAligned(target);
    const KWin::Rect devicePaint =
        viewport.mapToDeviceCoordinatesAligned(paintRegion);
    // QRegion remains only the hard output fence. A rotated fan member uses
    // the r21 offscreen aperture for one physical pixel of fractional edge
    // coverage; this is independent of client alpha and therefore treats a
    // solid Spotify surface exactly like a translucent decoration.
    KWin::Rect fanBaseline =
        viewport.mapToDeviceCoordinatesAligned(tablet->geometry());
    fanBaseline.setBottom(std::min(fanBaseline.bottom(), deviceTarget.bottom()));
    const KWin::Region cardClip = !rotatedFanCard
        ? deviceRegion & roundedClip(
              deviceTarget, CardCornerRadius * viewport.scale())
        : deviceRegion & KWin::Region(fanBaseline);
    const bool useFanAperture = rotatedFanCard
        && m_fanApertureShader
        && !devicePaint.isEmpty()
        && !deviceTarget.isEmpty();
    m_fanApertureWindow = useFanAperture ? window : nullptr;
    m_fanPaintSize = useFanAperture
        ? QSizeF(devicePaint.size()) : QSizeF();
    m_fanApertureOrigin = useFanAperture
        ? QPointF(deviceTarget.x() - devicePaint.x(),
                  deviceTarget.y() - devicePaint.y())
        : QPointF();
    m_fanApertureSize = useFanAperture
        ? QSizeF(deviceTarget.size()) : QSizeF();
    m_fanApertureRadius = useFanAperture
        ? static_cast<float>(CardCornerRadius * viewport.scale()) : 0.0F;

    KWin::effects->paintWindow(
        renderTarget, viewport, window, mask | PAINT_WINDOW_TRANSFORMED,
        cardClip, data);

    m_fanApertureWindow = nullptr;
    m_fanPaintSize = {};
    m_fanApertureOrigin = {};
    m_fanApertureSize = {};
    m_fanApertureRadius = 0.0F;
}

} // namespace Kadunce
