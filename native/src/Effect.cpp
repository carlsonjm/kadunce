/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Effect.h"
#include <QScopeGuard>
#include <fstream>
#include <string>
#include "LaunchIdentity.h"
#include "SpreadLayout.h"
#include "BentoCompositeGeometry.h"
#include "DisplayHandoffPolicy.h"
#include "CarryPaintPlan.h"
#include "NativeLanding.h"
#include "MonitorDropIntent.h"
#include "DeliberateEdgeEntry.h"

#include <core/output.h>
#include <core/region.h>
#include <core/renderviewport.h>
#include <core/rendertarget.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <input.h>
#include <inputmethod.h>
#include <inputpanelv1window.h>
#include <main.h>
#include <opengl/glshadermanager.h>
#include <opengl/glutils.h>
#include <opengl/glvertexbuffer.h>
#include <window.h>
#include <workspace.h>
#include <wayland_server.h>
#include <wayland/seat.h>

#include <KGlobalAccel>
#include <KService>

#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusServiceWatcher>
#include <QFileSystemWatcher>
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
#include <cmath>
#include <vector>

namespace Kadunce
{

namespace
{
constexpr auto Revision = "0.1.0-kadunce-baseline";
constexpr double CardCornerRadius = 10.0;
constexpr float BentoWorkspaceTintOpacity = 0.22F;
constexpr double LauncherGuestCommitDistance = 58.0;
constexpr auto DestinationVertex = R"GLSL(#version 140
in vec4 position;
uniform mat4 modelViewProjectionMatrix;
out vec2 point;
void main() {
    point = position.xy;
    gl_Position = modelViewProjectionMatrix * position;
}
)GLSL";
constexpr auto DestinationFragment = R"GLSL(#version 140
in vec2 point;
out vec4 fragColor;
uniform vec4 destinationBox;
uniform float outlineRadius;
uniform vec4 surfaceFill;
uniform float outlineOpacity;
#include "colormanagement.glsl"
void main() {
    vec2 halfSize = destinationBox.zw * 0.5;
    float radius = min(outlineRadius, min(halfSize.x, halfSize.y));
    vec2 q = abs(point - destinationBox.xy - halfSize) - (halfSize - vec2(radius));
    float d = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
    float feather = max(fwidth(d), 0.5);
    float inside = 1.0 - smoothstep(-feather, 0.0, d);
    float ring = smoothstep(-2.0 - feather, -2.0, d) * inside;
    float edgeAlpha = ring * outlineOpacity;
    vec4 fill = vec4(surfaceFill.rgb * surfaceFill.a, surfaceFill.a) * inside;
    vec4 color = vec4(vec3(0.88) * edgeAlpha, edgeAlpha) + fill * (1.0 - edgeAlpha);
    fragColor = nitsToDestinationEncoding(sourceEncodingToNitsInDestinationColorspace(color));
}
)GLSL";
// Card backing and vacant seam share one rigid transform and neutral material.
void paintCardSurface(KWin::GLShader *shader, const KWin::RenderTarget &renderTarget,
    const KWin::RenderViewport &viewport, const KWin::Region &clip,
    const QRectF &box, double angle, float opacity, float outline,
    const QVector3D &fillColor = QVector3D(0.075F, 0.075F, 0.075F),
    float fillOpacityScale = 1.0F)
{
    if (!shader || box.isEmpty()) return;
    QList<QVector2D> vertices;
    vertices << QVector2D(box.topLeft()) << QVector2D(box.topRight()) << QVector2D(box.bottomLeft())
             << QVector2D(box.bottomLeft()) << QVector2D(box.topRight()) << QVector2D(box.bottomRight());
    KWin::ShaderBinder binder(shader);
    auto matrix = viewport.projectionMatrix();
    matrix.scale(viewport.scale(), viewport.scale());
    matrix.translate(box.right(), box.bottom());
    matrix.rotate(float(angle), 0, 0, 1);
    matrix.translate(-box.right(), -box.bottom());
    shader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
    shader->setUniform("destinationBox", QVector4D(box.x(), box.y(), box.width(), box.height()));
    shader->setUniform("outlineRadius", float(CardCornerRadius));
    shader->setUniform("surfaceFill", QVector4D(fillColor,
        opacity * fillOpacityScale));
    shader->setUniform("outlineOpacity", outline);
    shader->setColorspaceUniforms(KWin::ColorDescription::sRGB,
        renderTarget.colorDescription(), KWin::RenderingIntent::Perceptual);
    const bool blended = glIsEnabled(GL_BLEND);
    const bool scissored = glIsEnabled(GL_SCISSOR_TEST);
    GLint previousScissor[4];
    glGetIntegerv(GL_SCISSOR_BOX, previousScissor);
    GLint sr, dr, sa, da;
    glGetIntegerv(GL_BLEND_SRC_RGB, &sr); glGetIntegerv(GL_BLEND_DST_RGB, &dr);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &sa); glGetIntegerv(GL_BLEND_DST_ALPHA, &da);
    glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    auto *buffer = KWin::GLVertexBuffer::streamingBuffer();
    glEnable(GL_SCISSOR_TEST);
    buffer->reset(); buffer->setVertices(vertices); buffer->render(clip, GL_TRIANGLES, true);
    glScissor(previousScissor[0], previousScissor[1], previousScissor[2], previousScissor[3]);
    if (!scissored) glDisable(GL_SCISSOR_TEST);
    glBlendFuncSeparate(sr, dr, sa, da);
    if (!blended) glDisable(GL_BLEND);
}
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

constexpr auto FanApertureVertexShader = R"GLSL(#version 140
in vec4 position;
in vec4 texcoord;
uniform mat4 modelViewProjectionMatrix;
uniform vec2 paintSize;
uniform vec2 apertureOrigin;
out vec2 texcoord0;
out vec2 cardPoint;
void main(void)
{
    gl_Position = modelViewProjectionMatrix * position;
    texcoord0 = texcoord.st;
    // Geometry coordinates are independent of texture flips and shadow UVs.
    cardPoint = position.xy * paintSize - apertureOrigin;
}
)GLSL";

constexpr auto FanApertureFragmentShader = R"GLSL(#version 140

in vec2 texcoord0;
in vec2 cardPoint;
out vec4 fragColor;

#include "colormanagement.glsl"
#include "saturation.glsl"

uniform sampler2D sampler;
uniform vec4 modulation;
uniform vec2 apertureSize;
uniform float apertureRadius;
uniform float tiltedSampling;

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
    vec2 point = cardPoint;
    vec4 tex = texture(sampler, texcoord0);
    if (tiltedSampling > 0.5) {
        // Integrate four points inside the destination pixel footprint. A
        // single bilinear lookup aliases thin client lines under fan rotation.
        // Keep ordinary cards on their exact existing sampling path.
        vec2 dx = dFdx(texcoord0) * 0.25;
        vec2 dy = dFdy(texcoord0) * 0.25;
        tex = (texture(sampler, texcoord0 - dx - dy)
             + texture(sampler, texcoord0 + dx - dy)
             + texture(sampler, texcoord0 - dx + dy)
             + texture(sampler, texcoord0 + dx + dy)) * 0.25;
    }

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
    connect(KWin::effects, &KWin::EffectsHandler::windowDeleted, this,
        [this](KWin::EffectWindow *window) { m_previewSourceBounds.remove(window); });
    m_cardStage = std::make_unique<CardStageController>(
        static_cast<CardStageHost *>(this));
    m_desktopStage = std::make_unique<DesktopStageController>(
        static_cast<DesktopStageHost *>(this));
    // Assertion-only §14 observation. Both stage controllers exist now, so this
    // is the first point that can see all three owners at once. It reports and
    // never repairs: a violation it finds is a pre-existing defect.
    // Window lifecycle alone is not enough: workspaceContextChanged is emitted
    // on add, close and activate, never on a Bento or Spread transition, so the
    // observer is also called directly from each transition below.
    connect(this, &Effect::workspaceContextChanged, this,
            &Effect::observeCardOwnership);

    if (KWin::effects->isOpenGLCompositing()
        && KWin::OffscreenEffect::supported()) {
        m_destinationShader = KWin::ShaderManager::instance()->generateCustomShader(
            KWin::ShaderTrait::UniformColor, QByteArray(DestinationVertex), QByteArray(DestinationFragment));
        m_fanApertureShader =
            KWin::ShaderManager::instance()->generateCustomShader(
                KWin::ShaderTrait::MapTexture, QByteArray(FanApertureVertexShader),
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

    m_toggleAction = new QAction(tr("Toggle Kadunce Spread"), this);
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
        m_showSpreadAction = new QAction(tr("Show Kadunce Spread"), this);
        m_showSpreadAction->setObjectName(
            QStringLiteral("Kadunce Show Spread"));
        connect(m_showSpreadAction, &QAction::triggered,
                this, &Effect::showCardLine);
        KWin::effects->registerTouchBorder(
            KWin::ElectricBottom, m_showSpreadAction);

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
    connect(KWin::effects, &KWin::EffectsHandler::screenAdded, this,
            [this](KWin::LogicalOutput *output) { m_desktopStage->handleScreenAdded(output); });
    // The keyboard overlays the desktop: KeyboardOverlayPolicy stops KWin
    // lifting the focused window for it, and nothing here resizes a card. What
    // the card stage answers is a covered text cursor, so it hears every
    // change that can cover or uncover one: the keyboard showing, hiding or
    // changing height, text focus moving, and the cursor itself moving.
    const auto watchInputPanel = [this]() {
        disconnect(m_inputPanelGeometry);
        // KWin announces a panel before the panel has a window an effect can
        // see, so a keyboard changing height is heard from the panel itself.
        KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
        if (KWin::Window *panel = method ? method->panel() : nullptr) {
            m_inputPanelGeometry = connect(panel,
                &KWin::Window::frameGeometryChanged, this,
                [this]() { m_cardStage->refreshKeyboardReveal(); });
        }
        m_cardStage->refreshKeyboardReveal();
    };
    connect(KWin::effects, &KWin::EffectsHandler::inputPanelChanged, this,
            watchInputPanel);
    watchInputPanel();
    if (KWin::InputMethod *method = KWin::kwinApp()->inputMethod()) {
        // Before the reveal hears it: a keyboard put back down reveals nothing.
        connect(method, &KWin::InputMethod::visibleChanged, this,
                &Effect::quietKeyboardIfSummoned);
        for (const auto signal : {&KWin::InputMethod::visibleChanged,
                                  &KWin::InputMethod::activeWindowChanged,
                                  &KWin::InputMethod::cursorRectangleChanged}) {
            connect(method, signal, this,
                    [this]() { m_cardStage->refreshKeyboardReveal(); });
        }
    }

    m_carryRuntime = std::make_unique<NativeCarryRuntime>();
    m_carryRuntime->observed = [this](KWin::Window *w, const char *event) {
        traceNativeMove(w ? w->effectWindow() : m_carriedWindow.data(), event);
    };
    m_carryRuntime->adopted = [this](KWin::Window *w) {
        const auto owner = m_carryRuntime->route.owner();
        if (m_inputRouter) {
            if (owner.kind == CarryDevice::Touch) m_inputRouter->retireNativeTouch(owner.contact);
            else m_inputRouter->retireNativePointer(Qt::LeftButton);
        }
        m_carryPickup = QRectF(m_carryRuntime->handoff.carry().snapshot().geometry);
        if (m_nativeCarry == w->effectWindow()) {
            m_nativeCarry = nullptr; m_nativeCarrySource.clear(); m_nativeCarryFromBento = false;
        }
        m_carriedWindow = w->effectWindow();
        traceNativeMove(m_carriedWindow, "adopted");
        KWin::effects->setElevatedWindow(m_carriedWindow, true);
        KWin::effects->addRepaintFull();
    };
    m_carryRuntime->moved = [this](QPointF p) { updateNativeCarryDestination(p); };
    m_carryRuntime->deferred = [this](KWin::Window *w) {
        // Native-drag visibility exception only: no controller start, which
        // would invalidate the saved reservation or take geometry ownership.
        m_nativeCarry = w->effectWindow();
        m_nativeCarrySource = w->output()->name();
        m_nativeCarryFromBento = false;
        KWin::effects->setElevatedWindow(m_nativeCarry, true);
    };
    m_carryRuntime->entryRequested = [this](const PreparedCarrySource &source, QPointF p) {
        if (isPanelPoint(p)) return false;
        for (auto *output : KWin::effects->screens()) {
            if (!QRectF(output->geometry()).contains(p)) continue;
            if (monitorCarryEdge(QRectF(output->geometry()), p)) return true;
            return output->name() != source.origin().output
                && (isTabletOutput(output) || m_desktopStage->hasSessionOnOutput(output->name()));
        }
        return false;
    };
    m_carryRuntime->ended = [this] { endNativeCarryPresentation(); };
    m_carryRuntime->nativeReleased = [this](KWin::Window *window, QPointF contact) {
        const QPointer<KWin::Window> guarded = window;
        // Let KWin finish its native release first. Cancel/Escape/extra contact
        // never schedules this callback; destruction cancels the one-shot.
        QTimer::singleShot(0, this, [this, guarded, contact] {
            if (!guarded || guarded->isDeleted() || guarded->isInteractiveMove()
                || guarded->isInteractiveResize() || guarded->isMinimized()
                || !guarded->output()
                || m_desktopStage->managesWindow(guarded->effectWindow())) return;
            const QRectF output(guarded->output()->geometry());
            const QRectF area = nativeLandingAreaForOutput(guarded->output());
            if (!inNativeDockReleaseZone(contact, output, area)) return;
            const QRectF frame(guarded->moveResizeGeometry());
            const auto landing = safeNativeLanding(frame, area);
            if (landing != frame) guarded->moveResize(KWin::RectF(landing));
        });
    };
    m_carryRuntime->interrupted = [this] { clearDropSettle(); clearBentoMotions(); };
    connect(KWin::effects, &KWin::EffectsHandler::screenAboutToLock, this,
            [this] { if (m_carryRuntime) m_carryRuntime->cancel(); });
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        connectManagedWindow(window);
    }

    if (!QDBusConnection::sessionBus().registerObject(
            QStringLiteral("/Kadunce"),
            QStringLiteral("studio.warbler.Kadunce"), this,
            QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals)) {
        qWarning() << "Kadunce" << Revision
                   << "could not publish the workspace context interface";
    }

    if (KWin::input()) {
        m_inputRouter = std::make_unique<WorkspaceInputRouter>(
            static_cast<WorkspaceInputTarget *>(this),
            m_usesDirectSystemEdges);
        KWin::input()->installInputEventFilter(m_inputRouter.get());
    }

    if (!m_usesDirectSystemEdges) watchForTabletKit();

    m_nativeEdgePolicy = std::make_unique<NativeEdgePolicy<KWin::Options>>(KWin::options);
    m_keyboardOverlayPolicy =
        std::make_unique<KeyboardOverlayPolicy<KWin::Options>>(KWin::options);
    connect(KWin::options, &KWin::Options::configChanged, this, [this] {
        if (m_nativeEdgePolicy) m_nativeEdgePolicy->refresh();
        if (m_keyboardOverlayPolicy) m_keyboardOverlayPolicy->refresh();
    });

    qInfo() << "Kadunce" << Revision
            << (m_usesDirectSystemEdges
                    ? "direct Z13 system edges"
                    : "Plasma-native system edges")
            << "and output-local Bento ready; fan aperture"
            << (m_fanApertureShader ? "enabled" : "r20 fallback");
}

Effect::~Effect()
{
    if (m_nativeCarry) KWin::effects->setElevatedWindow(m_nativeCarry, false);
    m_desktopStage->cancelRestoredMinimizations();
    // Roll back input while its target/controllers are alive, then unregister
    // the filter before restoration can reenter KWin or destroy those owners.
    cancelInputForCardStage();
    m_carryRuntime.reset();
    m_inputRouter.reset();
    Q_EMIT bridgeUnavailable();
    endLauncherGuest();
    if (m_showSpreadAction) {
        KWin::effects->unregisterTouchBorder(
            KWin::ElectricBottom, m_showSpreadAction);
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
    m_nativeEdgePolicy.reset();
}

void Effect::showCardLine()
{
    if (presentationForInput() != WorkspacePresentation::Spread) {
        toggle();
    }
}

void Effect::showActive()
{
    if (presentationForInput() == WorkspacePresentation::Spread) {
        toggle();
    }
}

bool Effect::supported()
{
    return true;
}

void Effect::watchForTabletKit()
{
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtime.isEmpty()) return;
    // The posture manager is a separate user service and has been observed
    // reaching active state after KWin. Testing once in the constructor then
    // makes the Plasma edge fallback permanent for the session, so watch the
    // runtime directory for the kit and adopt the direct router when it lands.
    m_tabletKitWatcher = std::make_unique<QFileSystemWatcher>();
    const QString kit = runtime + QStringLiteral("/z13-tablet-kit");
    m_tabletKitWatcher->addPath(runtime);
    if (QFileInfo::exists(kit)) m_tabletKitWatcher->addPath(kit);
    connect(m_tabletKitWatcher.get(), &QFileSystemWatcher::directoryChanged,
            this, [this, kit] {
                // The directory can appear before the posture file inside it.
                if (m_tabletKitWatcher && QFileInfo::exists(kit)
                    && !m_tabletKitWatcher->directories().contains(kit)) {
                    m_tabletKitWatcher->addPath(kit);
                }
                adoptDirectSystemEdges();
            });
}

void Effect::adoptDirectSystemEdges()
{
    if (m_usesDirectSystemEdges || !z13TabletKitAvailable()) return;
    m_usesDirectSystemEdges = true;
    m_tabletKitWatcher.reset();
    // Hand each edge back to Plasma before the router claims it, so one swipe
    // cannot reach both backends. Adoption is one-way: a kit that later goes
    // away leaves Kadunce's own recognition in place rather than churning the
    // backend mid-session.
    const auto release = [](KWin::ElectricBorder border, QAction *&action) {
        if (!action) return;
        KWin::effects->unregisterTouchBorder(border, action);
        action->deleteLater();
        action = nullptr;
    };
    release(KWin::ElectricBottom, m_showSpreadAction);
    release(KWin::ElectricTop, m_showActiveAction);
    if (m_inputRouter) m_inputRouter->setOwnsSystemEdges(true);
    qInfo() << "Kadunce" << Revision
            << "adopted direct Z13 system edges after the tablet kit appeared";
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
    // KWin types a layer surface by the scope it asks for, and any scope it
    // does not recognise becomes a normal window. A layer surface is placed by
    // its own anchors, so it can be neither moved nor resized as a card, and
    // treating it as one hides it: it has no card slot to be painted in. The
    // class is not exported, so it is recognised by name.
    if (!window->window()
        || window->window()->inherits("KWin::LayerShellV1Window")) {
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
    return output && KWin::effects->screens().contains(const_cast<KWin::LogicalOutput *>(output));
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

bool Effect::outputCanOwnCards(const KWin::LogicalOutput *output) const
{
    return m_cardStage->canOwnCards(output);
}

KWin::Rect Effect::activeTargetForDesktopStage(
    KWin::LogicalOutput *output) const
{
    return activeTarget(output);
}

void Effect::retireOutputFromDesktopStage(KWin::LogicalOutput *output)
{
    if (output && isTabletOutput(output)) m_cardStage->leaveBentoPresentation();
}

void Effect::prepareOutputForDesktopStage(KWin::LogicalOutput *output)
{
    if (output && isTabletOutput(output) && m_cardStage->isActive()) {
        m_cardStage->release(); // Do not release Bento sessions on other displays.
    }
}

std::optional<NativeMoveSnapshot> Effect::activeRestoreForDesktopStage(KWin::EffectWindow *window) const
{
    return m_cardStage->managedRestore(window);
}

bool Effect::admitDisplacedPaneToTablet(KWin::EffectWindow *window,
    const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore)
{
    // §5: while the display presents its layout, the pane that yielded becomes
    // a nonselected card behind it. Anywhere else there is no layout in front
    // of it, and an ordinary transfer is the right arrival.
    if (m_cardStage->admitDisplacedPaneAsHiddenCard(window, commitSource, restore))
        return true;
    return admitTransferredWindowToTablet(window, commitSource, restore);
}

bool Effect::admitSleepingPaneToTablet(KWin::EffectWindow *window,
    const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore)
{
    // Unlike the awake door, this one cannot find the record for itself: it
    // would prepare a carry, and a carry refuses a minimized window. The caller
    // still holds the session that has it, so it supplies it, and without it
    // the state §13 restores the window to would be the one the user just asked
    // for -- release would re-minimize a window it had woken.
    if (!restore) return false;
    // §7: a minimized pane is a sleeping individual card, not an arrival. Both
    // other doors present what they take, so neither can hold one.
    return m_cardStage->admitSleepingPaneAsCard(window, commitSource, restore);
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

std::optional<double> Effect::inputPanelTopForCardStage(
    KWin::LogicalOutput *output) const
{
    // KWin hands every effect the input panel, so the keyboard is readable
    // without a downstream surface and without the Keyboard telling anyone.
    // What a placement must never do is infer the keyboard from the panel that
    // yielded to it: that reads an occupied region as a free one.
    KWin::EffectWindow *panel = KWin::effects->inputPanel();
    if (!output || !panel || panel->isDeleted() || !panel->isVisible()) {
        return std::nullopt;
    }
    const KWin::RectF covered =
        KWin::RectF(panel->frameGeometry()).intersected(
            KWin::RectF(output->geometry()));
    if (covered.isEmpty()) {
        return std::nullopt;
    }
    return covered.top();
}

std::optional<KWin::RectF> Effect::textCursorForCardStage(
    const KWin::EffectWindow *window) const
{
    // Only the window KWin sends text to has a cursor worth revealing, and a
    // cursor outside that window's own frame is not one it could be showing.
    // An empty rectangle is a client that never said where its cursor is.
    KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
    if (!window || !method || !method->activeWindow()
        || method->activeWindow() != window->window()) {
        return std::nullopt;
    }
    const KWin::RectF cursor = method->cursorRectangle();
    if (cursor.height() <= 0.0
        || !window->frameGeometry().contains(cursor.center())) {
        return std::nullopt;
    }
    return cursor;
}

void Effect::cardActivatedForCardStage(KWin::EffectWindow *window)
{
    m_keyboardQuietWindow = window;
    m_keyboardQuietSince.start();
}

void Effect::quietKeyboardIfSummoned()
{
    // Focusing a card is not asking to type into it. Some clients have the
    // compositor raise the keyboard as they gain focus; for a card this stage
    // focused itself, that keyboard goes back down. The raise follows the
    // focus within a moment, so the quiet lasts only that moment.
    constexpr qint64 QuietMs = 1000;
    KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
    if (m_keyboardQuietWindow && m_keyboardQuietSince.elapsed() > QuietMs) {
        m_keyboardQuietWindow.clear();
    }
    if (!method || !method->isVisible() || !m_keyboardQuietWindow
        || !method->activeWindow()
        || method->activeWindow() != m_keyboardQuietWindow->window()) {
        return;
    }
    method->hide();
    qInfo() << "Kadunce kept the keyboard down for" << m_keyboardQuietWindow->caption();
}

bool Effect::mayHoldWindowForCardStage(
    const KWin::EffectWindow *window) const
{
    // §7 keeps a minimized window owned as an individual card, so what card
    // ownership may hold cannot exclude the one state the section is about.
    // `isCardWindow` answers what this effect can present, and excludes it.
    return isApplicationWindow(window) && !window->isHidden();
}

void Effect::setPagingShortcutsForCardStage(bool active)
{
    setPagingShortcutsActive(active);
}

void Effect::cancelInputForCardStage()
{
    clearBentoMotions();
    clearDropSettle();
    m_lineDestination.reset(); m_lineDestinationWindow.clear();
    if (m_carryRuntime) m_carryRuntime->cancel();
    m_inputActivationGuard.invalidate();
    if (m_inputRouter) m_inputRouter->cancelWorkspaceInteraction();
}

void Effect::connectManagedWindowForCardStage(KWin::EffectWindow *window)
{
    connectManagedWindow(window);
}

void Effect::unredirectForCardStage(KWin::EffectWindow *window)
{
    unredirect(window);
    m_previewSourceBounds.remove(window);
}

void Effect::retireBentoProjectionForCardStage(
    const QList<QPointer<KWin::EffectWindow>> &windows)
{
    // Exact resume can synchronously return these surfaces to Desktop Stage.
    // Drop every Spread paint latch before that native scene becomes visible.
    m_fanApertureWindow = nullptr;
    m_fanPaintSize = {};
    m_fanApertureOrigin = {};
    m_fanApertureSize = {};
    m_fanApertureRadius = 0.0F;
    m_preparationNeighbors.removeIf([&](const auto &window) {
        return windows.contains(window);
    });
    m_neighborPreparedThisFrame = false;
    m_neighborPreparationFrames = m_preparationNeighbors.size();
    for (const auto &window : windows) {
        if (!window || window->isDeleted()) continue;
        unredirect(window);
        m_previewSourceBounds.remove(window);
    }
    KWin::effects->addRepaintFull();
}

bool Effect::admitCardToDesktopStage(
    KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const KWin::RectF &geometry, const std::function<bool()> &commitSource,
    const std::function<void()> &releaseSource)
{
    if (m_carryDestination && window == m_carriedWindow)
        return m_desktopStage->transferPreparedCard(*m_carryDestination, commitSource, releaseSource);
    if (m_cardStage->cardGrabActive()) {
        if (!m_lineDestination || m_lineDestinationWindow != window) return false;
        const auto reserved = *m_lineDestination;
        const auto target = m_linePreview;
        const auto from = QRectF(m_cardStage->cardGrabTarget()).translated(m_cardStage->cardGrabOffset());
        QPointer<KWin::EffectWindow> arrival = window;
        QPointer<KWin::LogicalOutput> destination = output;
        const bool committed = m_desktopStage->transferPreparedCard(reserved, commitSource, releaseSource);
        if (committed && target) startDropSettle(arrival, destination, from, QRectF(*target));
        return committed;
    }
    return m_desktopStage->transferCardWindow(window, output, geometry, commitSource, releaseSource);
}

bool Effect::resumeBentoProjectionForCardStage(
    const BentoProjectionSession &projection,
    const std::function<bool()> &commitSource,
    const std::function<void()> &releaseSource)
{
    return m_desktopStage->resumeProjectedSession(
        projection, commitSource, releaseSource);
}

WorkspacePresentation Effect::presentationForInput() const
{
    // Bento is presented by the desktop stage's real panes. Card Stage still
    // owns its hidden individual cards, but none of its gestures apply, so the
    // router sees the same thing it saw before those cards could coexist.
    if (!m_cardStage->isActive()
        || m_cardStage->presentation() == CardPresentation::Bento) {
        return WorkspacePresentation::Inactive;
    }
    return m_cardStage->presentation() == CardPresentation::Spread
        ? WorkspacePresentation::Spread
        : WorkspacePresentation::Active;
}

WorkspaceInputGeometry Effect::geometryForInput() const
{
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet) {
        return {};
    }
    const KWin::Rect tabletRect = tablet->geometry();
    const auto *selected = m_cardStage->selectedWindow();
    const KWin::Rect centerCard = selected && !m_cardStage->cardGrabActive()
        && !m_cardStage->launcherGuestActive()
        ? m_cardStage->posedTargetForWindow(tablet, selected)
        : cardTargetForSlot(tablet, 0);
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

bool Effect::nativeWindowInteractionForInput() const
{
    return KWin::workspace()->moveResizeWindow() != nullptr;
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
    if (!tablet) return false;
    const auto *selected = m_cardStage->selectedWindow();
    const auto target = selected && !m_cardStage->launcherGuestActive()
        ? m_cardStage->posedTargetForWindow(tablet, selected)
        : cardTargetForSlot(tablet, 0);
    return target.contains(position.toPoint());
}

bool Effect::launcherGuestActiveForInput() const
{
    return m_cardStage->launcherGuestActive();
}

bool Effect::launcherGuestContainsForInput(const QPointF &position) const
{
    KWin::LogicalOutput *tablet = tabletOutput();
    return m_cardStage->launcherGuestActive() && tablet
        && (m_launcherGuestExpanded ? launcherGuestExpandedTarget(tablet) : m_cardStage->launcherGuestTarget(tablet))
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
    if (m_launcherGuestExpanded) return;
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

KWin::LogicalOutput *Effect::externalDesktopOutput() const
{
    // The largest output that is not the tablet. PRODUCT-CONTRACT.md makes the
    // external display the desktop stage, so a Bento action belongs to it while
    // one is attached.
    KWin::LogicalOutput *largest = nullptr;
    double largestArea = 0.0;
    for (auto *output : KWin::effects->screens()) {
        if (!output || isTabletOutput(output)) continue;
        const auto geometry = output->geometry();
        const double area = double(geometry.width()) * double(geometry.height());
        if (area > largestArea) { largestArea = area; largest = output; }
    }
    return largest;
}

bool Effect::pairActiveCardIntoBento(KWin::LogicalOutput *output)
{
    auto *active = m_cardStage->activeCardIdentity();
    if (!output || !active) return false;
    // CARD-LIFECYCLE.md §3 places by the gesture, and this gesture contacts no
    // edge, so the side is stated rather than read: the Active card keeps the
    // left pane and its partner is named by walking right from it.
    const BentoSidePlacement side{false, true};
    auto *partner = m_cardStage->partnerForSideSnap(active, false);
    if (!partner) return false;
    const auto reserved = m_desktopStage->prepareCardDrop(active, output,
        KWin::RectF(active->frameGeometry()),
        DesktopStageController::CardDropIntent::ActivateBento, side, partner);
    if (!reserved) return false;
    QPointer<KWin::EffectWindow> carried = active;
    QPointer<KWin::EffectWindow> named = reserved->namedPartner();
    return m_desktopStage->activatePreparedTabletDrop(*reserved, nullptr,
        [this, carried, named] { return m_cardStage->releasePairToBento(carried, named); });
}

void Effect::toggleBento()
{
    if (m_carryRuntime) m_carryRuntime->cancel();
    // PRODUCT-CONTRACT.md: external Bento while docked, tablet Bento while
    // undocked. This read the pointer before, which on a touch tablet is
    // wherever the pointer was last left rather than where the user is working.
    KWin::LogicalOutput *target = externalDesktopOutput();
    if (!target) target = tabletOutput();
    if (!target) return;
    // §3 names a pair on a display that can own cards, so a Bento action there
    // composes two named windows rather than sweeping the display. A display
    // that cannot own cards reaches neither pairing rule and composes across
    // itself, which is what an external desktop stage is for.
    if (!m_desktopStage->hasSessionOnOutput(target->name())
        && m_cardStage->canOwnCards(target)) {
        (void)pairActiveCardIntoBento(target);
        observeCardOwnership();
        return;
    }
    (void)m_desktopStage->toggleOnOutput(target->name());
    observeCardOwnership();
}

void Effect::connectManagedWindow(KWin::EffectWindow *window)
{
    if (!window) {
        return;
    }
    if (window->window()) {
        connect(window->window(), &KWin::Window::minimizedChanged, this,
            [this, guarded = QPointer<KWin::EffectWindow>(window)] {
                if (guarded) m_desktopStage->handleWindowMinimizedChanged(guarded);
            });
        connect(window->window(), &KWin::Window::readyForPaintingChanged,
                this, &Effect::handleLaunchWindowChanged, Qt::UniqueConnection);
        connect(window->window(), &KWin::Window::desktopFileNameChanged,
                this, &Effect::handleLaunchWindowChanged, Qt::UniqueConnection);
        connect(window->window(), &KWin::Window::windowClassChanged,
                this, &Effect::handleLaunchWindowChanged, Qt::UniqueConnection);
        connect(window->window(), &KWin::Window::captionChanged,
                this, &Effect::handleLaunchWindowChanged, Qt::UniqueConnection);
        connect(window->window(), &KWin::Window::fullScreenChanged,
                this, &Effect::handleManagedStateChanged, Qt::UniqueConnection);
        connect(window->window(), &KWin::Window::maximizedChanged,
                this, &Effect::handleManagedStateChanged, Qt::UniqueConnection);
        connect(window->window(), &KWin::Window::quickTileModeChanged,
                this, &Effect::handleManagedStateChanged, Qt::UniqueConnection);
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
    if (m_carryRuntime && window->window()) m_carryRuntime->observer.watch(window->window());
}

QString Effect::applicationDisplayName(KWin::EffectWindow *window)
{
    if (!window) return {};
    const auto cached = m_applicationDisplayNames.constFind(window);
    if (cached != m_applicationDisplayNames.cend()) return *cached;
    QString serviceName;
    QString resourceClass;
    if (window->window()) {
        const QString desktopFileName =
            window->window()->desktopFileName().trimmed();
        if (!desktopFileName.isEmpty()) {
            QString desktopName = desktopFileName;
            if (desktopName.endsWith(QStringLiteral(".desktop")))
                desktopName.chop(8);
            auto service = KService::serviceByDesktopName(desktopName);
            if (!service) service = KService::serviceByStorageId(desktopFileName);
            if (service) serviceName = service->name();
        }
        resourceClass = window->window()->resourceClass();
    }
    const QString name = humanApplicationName(serviceName, resourceClass,
        window->windowClass().section(QLatin1Char(' '), -1),
        window->caption());
    m_applicationDisplayNames.insert(window, name);
    return name;
}

void Effect::handleWindowMoveResizeStarted(KWin::EffectWindow *window)
{
    // Active-sized cards do not become ordinary windows through a resize grip.
    // Defer cancellation until KWin has finished publishing native-start; never
    // tear down its transaction recursively inside that signal.
    if (window && window->window() && window->isUserResize()
        && m_cardStage->managedRestore(window)) {
        QPointer<KWin::EffectWindow> guarded = window;
        QTimer::singleShot(0, this, [this, guarded] {
            if (!guarded || !guarded->window() || !guarded->isUserResize()
                || !m_cardStage->managedRestore(guarded)) return;
            guarded->window()->cancelInteractiveMoveResize();
        });
        return;
    }
    traceNativeMove(window, "native-start");
    if (window == m_settlingWindow) clearDropSettle();
    if (m_carryRuntime && m_carryRuntime->route.busy()) m_carryRuntime->cancel();
    if (m_carryRuntime && window->window() && window->isUserMove()
        && !m_carryRuntime->route.busy()) {
        auto source = m_cardStage->prepareNativeCarrySource(window);
        const bool fromBento = !source;
        if (!source) source = m_desktopStage->prepareNativeCarrySource(window);
        if (source) {
            m_carryPickup = QRectF(window->frameGeometry());
            QPointer<KWin::EffectWindow> guarded = window;
            if (m_carryRuntime->handoff.stage(window->window(), *source,
                [this, saved = *source, fromBento] {
                    return fromBento ? m_desktopStage->nativeCarrySourceValid(saved)
                                     : m_cardStage->nativeCarrySourceValid(saved);
                }, [this, guarded] { if (guarded) beginLegacyNativeMove(guarded); })) {
                traceNativeMove(window, source->isDesktopWindow() ? "staged-ordinary" : "staged-card");
                return;
            }
        }
    }
    beginLegacyNativeMove(window);
}

void Effect::beginLegacyNativeMove(KWin::EffectWindow *window)
{
    traceNativeMove(window, "native-fallback");
    if (isApplicationWindow(window) && window->isUserMove()) {
        m_nativeCarry = window;
        m_nativeCarrySource = window->screen() ? window->screen()->name() : QString();
        m_nativeCarryFromBento = m_desktopStage->managesWindow(window);
    }
    m_cardStage->handleManualWindowChange(window);
    m_desktopStage->handleWindowMoveResizeStarted(window);
    if (m_nativeCarry == window) {
        KWin::effects->setElevatedWindow(window, true);
        KWin::effects->addRepaintFull();
    }
}

void Effect::endNativeCarryPresentation()
{
    if (m_carriedWindow) traceNativeMove(m_carriedWindow, "presentation-ended");
    m_lastCarryDestinationTrace.clear();
    if (m_carriedWindow) {
        KWin::effects->setElevatedWindow(m_carriedWindow, false);
        unredirect(m_carriedWindow);
    }
    m_carriedWindow.clear(); m_carryDestination.reset(); m_carryPreview.reset();
    m_carryCardEntryOutput.clear();
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
}

void Effect::traceNativeMove(KWin::EffectWindow *window, const char *event)
{
    // No titles, query text, coordinates or persistent journal output. Keep only
    // the latest 48 lifecycle transitions; this read-only evidence dies on unload.
    const auto *client = window ? window->window() : nullptr;
    const QJsonObject entry{
        {QStringLiteral("event"), QString::fromLatin1(event)},
        {QStringLiteral("app"), client ? client->resourceClass() : QString()},
        {QStringLiteral("window"), client ? client->internalId().toString() : QString()},
        {QStringLiteral("type"), client ? QString::fromLatin1(client->metaObject()->className()) : QString()},
        {QStringLiteral("decorated"), client && bool(client->decoration())},
        {QStringLiteral("output"), window && window->screen() ? window->screen()->name() : QString()},
        {QStringLiteral("bentoMember"), window && m_desktopStage->managesWindow(window)},
        {QStringLiteral("carrying"), bool(m_carriedWindow)},
        {QStringLiteral("rendererActive"), isActive()}
    };
    m_nativeMoveTrace.append(QString::fromUtf8(QJsonDocument(entry).toJson(QJsonDocument::Compact)));
    while (m_nativeMoveTrace.size() > 48) m_nativeMoveTrace.removeFirst();
}

QString Effect::nativeCarryState() const
{
    QJsonArray bentoMotion;
    for (const auto &m : m_bentoMotions) {
        const auto pose = bentoMotionRect(m.window);
        if (pose) bentoMotion.append(QJsonObject{
            {QStringLiteral("window"), windowIdentity(m.window)},
            {QStringLiteral("rect"), geometryContext(KWin::RectF(*pose).toRect())},
            {QStringLiteral("target"), geometryContext(KWin::RectF(m.to).toRect())}});
    }
    const auto settling = dropSettleRect();
    const auto &reservation = m_carriedWindow ? m_carryDestination : m_lineDestination;
    const auto &preview = m_carriedWindow ? m_carryPreview : m_linePreview;
    // A Card Stage entry has no Bento reservation to validate; its destination
    // is the Active card target, and it shows the same placement outline.
    const bool cardEntry = m_carriedWindow ? bool(m_carryCardEntryOutput)
                                           : bool(m_lineCardEntryOutput);
    const bool previewValid = (m_carriedWindow || m_cardStage->cardGrabActive()) && preview
        && (cardEntry || (reservation && m_desktopStage->cardDropValid(*reservation)));
    auto *tablet = tabletOutput();
    auto *selected = m_cardStage->selectedWindow();
    auto lineRect = tablet && selected
        ? m_cardStage->previewTargetForWindow(tablet, selected) : KWin::Rect();
    auto linePose = m_cardStage->stackPoseForWindow(selected, lineRect.width());
    if (m_cardStage->cardGrabActive()) {
        lineRect = m_cardStage->cardGrabTarget();
        const auto offset = m_cardStage->cardGrabOffset();
        lineRect.translate(qRound(offset.x()), qRound(offset.y()));
    } else {
        lineRect.translate(qRound(linePose.x), qRound(linePose.y));
        (void)m_cardStage->applyPoseTransition(selected, lineRect, linePose);
    }
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("dropSettling"), bool(settling)},
        {QStringLiteral("guestNeighborOpacity"), guestNeighborOpacity()},
        {QStringLiteral("bentoMotion"), bentoMotion},
        {QStringLiteral("dropRect"), settling ? geometryContext(KWin::RectF(*settling).toRect()) : QJsonObject{}},
        {QStringLiteral("destinationPreview"), previewValid},
        {QStringLiteral("placementOutline"),
            previewValid && (cardEntry || reservation->showsPlacementOutline())},
        {QStringLiteral("detachPreview"),
            previewValid && !cardEntry && reservation->detachesToDesktop()},
        {QStringLiteral("rendererActive"), isActive()},
        {QStringLiteral("destinationRect"), previewValid ? geometryContext(preview->toRect()) : QJsonObject{}},
        {QStringLiteral("lineAnimating"), m_cardStage->animationsRunning()},
        {QStringLiteral("lineRotation"), linePose.rotation},
        {QStringLiteral("lineRect"), QJsonObject{{QStringLiteral("x"), lineRect.x()},
            {QStringLiteral("y"), lineRect.y()}, {QStringLiteral("width"), lineRect.width()},
            {QStringLiteral("height"), lineRect.height()}}},
        {QStringLiteral("carrying"), bool(m_carriedWindow)},
        {QStringLiteral("inputBusy"), m_carryRuntime && m_carryRuntime->route.busy()},
        {QStringLiteral("destination"), bool(m_carryDestination) || bool(m_carryCardEntryOutput)},
        {QStringLiteral("lineCarrying"), m_cardStage->cardGrabActive()},
        {QStringLiteral("lineDestination"),
            bool(m_lineDestination) || bool(m_lineCardEntryOutput)},
        {QStringLiteral("stackArmed"), m_cardStage->stackPreviewArmed()},
        {QStringLiteral("stackInsertion"), m_cardStage->stackInsertionIndex()}
    }).toJson(QJsonDocument::Compact));
}

void Effect::updateNativeCarryDestination(QPointF contact)
{
    const auto traceDestination = qScopeGuard([this] {
        const QString state = m_carryCardEntryOutput
            ? QStringLiteral("destination-card:%1").arg(m_carryCardEntryOutput->name())
            : !m_carryDestination ? QStringLiteral("destination-none")
            : QStringLiteral("destination-%1:%2")
                .arg(m_carryDestination->detachesToDesktop() ? "exit" : "placement",
                     m_carryDestination->destinationOutput()->name());
        if (state != m_lastCarryDestinationTrace) {
            m_lastCarryDestinationTrace = state;
            traceNativeMove(m_carriedWindow, qPrintable(state));
        }
    });
    const auto previousDestination = m_carryDestination;
    m_carryDestination.reset();
    m_carryCardEntryOutput.clear();
    m_carryPreview.reset();
    if (!m_carriedWindow || !m_carryRuntime->handoff.source()) return;
    auto &handoff = m_carryRuntime->handoff;
    handoff.withdrawDrop(); // Leaving an exit/edge must retire its commit callback too.
    KWin::LogicalOutput *target = nullptr;
    for (auto *o : KWin::effects->screens())
        if (QRectF(o->geometry()).contains(contact)) { target = o; break; }
    KWin::effects->addRepaintFull();
    if (!target) return;
    const bool bento = m_desktopStage->nativeCarrySourceValid(*handoff.source());
    const bool desktopWindow = handoff.source()->isDesktopWindow();
    const bool local = target->name() == handoff.source()->origin().output;
    const bool tablet = isTabletOutput(target);
    const auto edge = isPanelPoint(contact) ? std::nullopt
        : monitorCarryEdge(QRectF(target->geometry()), contact);
    std::optional<BentoSidePlacement> side;
    if (edge && (*edge == CarryEdge::Left || *edge == CarryEdge::Right))
        side = bentoSideChoice(*edge == CarryEdge::Right, contact.y(),
            QRectF(target->geometry()).center().y(), previousDestination
                && previousDestination->destinationOutput() == target
                ? previousDestination->sidePlacement() : std::nullopt);
    // Only an already-owned Bento carry may traverse the dock to the physical
    // bottom edge. This does not change panel hit testing for ordinary input.
    const QRectF area = nativeLandingAreaForOutput(target);
    // Landing clearance is separate from the physical-edge gesture target.
    const double exitBottom = area.bottom() - 10.0;
    if (local && bento && !desktopWindow
        && contact.y() >= double(target->geometry().bottom()) - 24.0) {
        QSizeF size = QRectF(handoff.source()->restoreSnapshot().floatingGeometry).size();
        if (!size.isValid()) size = m_carryPickup.size();
        size.setWidth(std::clamp(size.width(), 200.0, std::max(200.0, area.width() * .8)));
        size.setHeight(std::clamp(size.height(), 150.0, std::max(150.0, area.height() * .8)));
        QRectF box(contact.x() - size.width() / 2, exitBottom - size.height(), size.width(), size.height());
        box.moveLeft(std::clamp(box.left(), area.left(), area.right() - box.width()));
        const auto reserved = m_desktopStage->prepareCardDrop(m_carriedWindow, target,
            KWin::RectF(box), DesktopStageController::CardDropIntent::NativeDesktop);
        if (!reserved) return;
        m_carryDestination = reserved;
        m_carryPreview = m_desktopStage->cardDropPreview(*reserved);
        handoff.previewDrop({CarryDestinationKind::NativeDesktop, target->name(), target->name(), 0, 0},
            [this, reserved] { return m_desktopStage->cardDropValid(*reserved); },
            [this, reserved](const PreparedCarrySource &source) {
                return m_desktopStage->transferNativeCarryToDesktop(source, *reserved);
            });
        return;
    }
    // CARD-LIFECYCLE.md §3 and §10 decide what an edge action means before any
    // layout is reserved. The Spread grab asks the same question, so a side snap
    // cannot mean one thing carried from the desktop and another from Spread.
    const bool liveLayout = m_desktopStage->hasSessionOnOutput(target->name());
    // §5: a pane carried to the top edge leaves the layout it is in, which is
    // the one edge action a live layout answers by giving a window up. Every
    // other snap into one is §5's displacement and is reserved below.
    const bool extractingPane = liveLayout && edge && *edge == CarryEdge::Top
        && m_desktopStage->managesWindow(m_carriedWindow)
        && m_cardStage->canOwnCards(target);
    if (local && edge && (!liveLayout || extractingPane)) {
        const bool leftEdge = *edge == CarryEdge::Left;
        const bool sideEdge = leftEdge || *edge == CarryEdge::Right;
        // §3 names the partner from Spread order. Naming is read-only: the
        // prepared carry embeds the workspace revision, so it must not move
        // selection or the pair side.
        auto *partner = sideEdge
            ? m_cardStage->partnerForSideSnap(m_carriedWindow, leftEdge) : nullptr;
        const auto outcome = planEdgeEntry({
            .edge = *edge,
            // §3: a display with a live layout is owned, whether or not this
            // stage also holds individual cards on it.
            .ownsDisplay = m_cardStage->ownsDisplay(target) || liveLayout,
            .canOwnCards = m_cardStage->canOwnCards(target),
            .hasBentoLayout = liveLayout,
            .carriedEligible = isCardWindow(m_carriedWindow),
            .partnerNamed = partner != nullptr,
            .carriedIsActive = m_cardStage->activeCardIdentity() == m_carriedWindow,
        });
        QPointer<KWin::EffectWindow> carried = m_carriedWindow;
        QPointer<KWin::LogicalOutput> output = target;
        const auto destination = monitorDropIntent(target->name(), 0, std::nullopt, edge);
        switch (outcome) {
        case EdgeEntryOutcome::Refuse:
        case EdgeEntryOutcome::Unchanged:
            return;
        case EdgeEntryOutcome::ComposeDisplayBento:
            break; // A display that cannot own cards reserves below as before.
        case EdgeEntryOutcome::PairIntoBento: {
            if (!side || !destination) return;
            const auto reserved = m_desktopStage->prepareCardDrop(m_carriedWindow, target,
                KWin::RectF(QRectF(handoff.carry().position(), m_carryPickup.size())),
                DesktopStageController::CardDropIntent::ActivateBento, side, partner);
            if (!reserved) return;
            m_carryPreview = m_desktopStage->cardDropPreview(*reserved);
            if (!m_carryPreview) return;
            m_carryDestination = reserved;
            handoff.previewDrop(*destination,
                [this, reserved] { return m_desktopStage->cardDropValid(*reserved); },
                [this, reserved, carried, bento](const PreparedCarrySource &source) {
                    // Commit against the identity the reservation named, never
                    // a partner re-read after preparation.
                    QPointer<KWin::EffectWindow> named = reserved->namedPartner();
                    return (bento ? m_desktopStage->nativeCarrySourceValid(source)
                                  : m_cardStage->nativeCarrySourceValid(source))
                        && m_desktopStage->activatePreparedTabletDrop(*reserved,
                            &source.restoreSnapshot(),
                            [this, carried, named] {
                                return m_cardStage->releasePairToBento(carried, named);
                            });
                });
            return;
        }
        case EdgeEntryOutcome::AdoptDisplay:
        case EdgeEntryOutcome::MakeActive: {
            // §10: a card that is already owned and not Active cannot be carried
            // natively, so only adoption and a native arrival reach here.
            if (!destination) return;
            const bool adopt = outcome == EdgeEntryOutcome::AdoptDisplay;
            if (!adopt && m_cardStage->liveCardIndex(carried) >= 0) return;
            // The destination is Card Stage: the carried window lands as the
            // Active card, and adoption makes every other eligible window a
            // nonselected individual card.
            m_carryCardEntryOutput = target;
            m_carryPreview = KWin::RectF(m_cardStage->activeTarget(target));
            handoff.previewDrop(*destination,
                [this, carried, output, adopt] {
                    const bool valid = carried && !carried->isDeleted() && output
                        && KWin::effects->screens().contains(output.data())
                        && isCardWindow(carried) && carried->screen() == output
                        && (m_cardStage->ownsDisplay(output)
                            || m_desktopStage->hasSessionOnOutput(output->name())) != adopt;
                    if (!valid) {
                        qInfo() << "Kadunce card entry drop invalid:"
                                << "window" << bool(carried) << "output" << bool(output)
                                << "card" << (carried && isCardWindow(carried))
                                << "sameOutput" << (carried && output && carried->screen() == output)
                                << "ownsDisplay" << (output && m_cardStage->ownsDisplay(output))
                                << "bento" << (output && m_desktopStage->hasSessionOnOutput(output->name()))
                                << "adopt" << adopt;
                    }
                    return valid;
                },
                [this, carried, adopt, bento, extractingPane](const PreparedCarrySource &source) {
                    const auto sourceValid = [this, &source, bento] {
                        return bento ? m_desktopStage->nativeCarrySourceValid(source)
                                     : m_cardStage->nativeCarrySourceValid(source);
                    };
                    if (adopt) return m_cardStage->adoptDisplayWithActive(carried, sourceValid);
                    // §5: a pane gives up Bento ownership in the same published
                    // step that makes it a card, so the layout it left is never
                    // observed still naming it. Everything else arrives without
                    // a layout to leave.
                    if (extractingPane
                        ? !m_desktopStage->extractPaneToCards(carried, sourceValid)
                        : !admitTransferredWindowToTablet(carried, sourceValid)) return false;
                    (void)m_cardStage->promoteToActive(carried);
                    return true;
                });
            return;
        }
        }
    }
    const bool stayNative = desktopWindow && !edge
        && (local || (!tablet && !m_desktopStage->hasSessionOnOutput(target->name())));
    if (local && (!bento || (!stayNative && (isPanelPoint(contact)
        || contact.y() >= target->geometry().bottom() - 48)))) return;
    if (tablet && !bento) return;
    QRectF landing(handoff.carry().position(), m_carryPickup.size());
    if (stayNative && inNativeDockReleaseZone(contact, QRectF(target->geometry()), area))
        landing = safeNativeLanding(landing, area);
    const KWin::RectF geometry(landing);
    const auto reserved = local && !desktopWindow && !side
        ? m_desktopStage->prepareLocalCardDrop(m_carriedWindow, target, geometry, contact)
        : m_desktopStage->prepareCardDrop(m_carriedWindow, target, geometry,
        stayNative ? DesktopStageController::CardDropIntent::NativeDesktop
        : edge && !m_cardStage->canOwnCards(target)
             ? DesktopStageController::CardDropIntent::ActivateBento
             : DesktopStageController::CardDropIntent::OpenSpace, side);
    if (!reserved) return;
    m_carryDestination = reserved;
    m_carryPreview = m_desktopStage->cardDropPreview(*reserved);
    const bool existing = m_desktopStage->hasSessionOnOutput(target->name());
    if (side && !m_carryPreview) return;
    QPointer<KWin::LogicalOutput> output = target;
    const auto destination = stayNative
        ? monitorDropIntent(target->name(), 0, std::nullopt)
        : tablet && !existing
        ? std::optional<CarryDestination>{{CarryDestinationKind::LineGap, target->name(), target->name(), 0, 0}}
        : monitorDropIntent(target->name(), 0,
            existing ? std::optional<MonitorLayoutTarget>{{target->name(), 0, 0}} : std::nullopt, edge);
    if (!destination) return;
    handoff.previewDrop(*destination,
        [this, reserved] { return m_desktopStage->cardDropValid(*reserved); },
        [this, reserved, bento, output, geometry, targetRect = m_carryPreview,
         arrival = m_carriedWindow](const PreparedCarrySource &source) {
            const bool committed = bento
                ? m_desktopStage->transferNativeCarryToDesktop(source, *reserved)
                : m_cardStage->transferNativeCarryToDesktop(source, output, geometry);
            if (committed && targetRect) startDropSettle(arrival, output, QRectF(geometry), QRectF(*targetRect));
            return committed;
        });
}

void Effect::clearDropSettle()
{
    const bool repaint = bool(m_settlingWindow);
    if (m_settlingWindow && !m_settlingWindow->isDeleted()) unredirect(m_settlingWindow);
    m_settlingWindow.clear(); m_settlingOutput.clear();
    m_dropSettleTimer.invalidate();
    if (repaint) KWin::effects->addRepaintFull();
}

std::optional<QRectF> Effect::bentoMotionRect(KWin::EffectWindow *window) const
{
    for (const auto &m : m_bentoMotions) {
        if (m.window != window) continue;
        if (!window || window->isDeleted() || !window->window() || !m.output
            || !m.timer.isValid() || m.timer.elapsed() >= 220
            || window->isUserMove() || window->isUserResize() || window->isMinimized()
            || QRectF(m.output->geometry()) != m.outputGeometry
            || window->window()->moveResizeOutput() != m.output
            || QRectF(window->window()->moveResizeGeometry()) != m.to) return std::nullopt;
        const double t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(m.timer.elapsed()/220.0);
        return QRectF(m.from.topLeft()+(m.to.topLeft()-m.from.topLeft())*t,
            m.from.size()+(m.to.size()-m.from.size())*t);
    }
    return std::nullopt;
}

QRectF Effect::bentoPresentationRect(KWin::EffectWindow *window) const
{
    if (!window || window->isDeleted() || window->isMinimized() || window == m_carriedWindow)
        return {};
    if (const auto pose = bentoMotionRect(window)) return *pose;
    return QRectF(window->frameGeometry());
}

void Effect::clearBentoMotions()
{
    if (m_bentoMotions.isEmpty()) return;
    for (const auto &m : std::as_const(m_bentoMotions))
        if (m.window && !m.window->isDeleted()) unredirect(m.window);
    m_bentoMotions.clear();
    KWin::effects->addRepaintFull();
}

void Effect::animateBentoLayout(KWin::LogicalOutput *output,
    const QList<QPointer<KWin::EffectWindow>> &windows,
    const QList<QRectF> &from, const QList<QRectF> &to)
{
    // Geometry is already committed. This is bounded presentation only;
    // each output's changed panes share one clock and retain interrupted poses.
    for (auto it = m_bentoMotions.begin(); it != m_bentoMotions.end();) {
        if (it->output != output) { ++it; continue; }
        if (it->window && !it->window->isDeleted()) unredirect(it->window);
        it = m_bentoMotions.erase(it);
    }
    if (!output) return;
    QElapsedTimer clock; clock.start();
    for (int i = 0; i < windows.size() && i < from.size() && i < to.size(); ++i) {
        auto *w = windows[i].data();
        if (!w || w->isDeleted() || !w->window() || w->isMinimized()
            || w == m_carriedWindow || w == m_settlingWindow || !from[i].isValid()
            || from[i] == to[i] || w->window()->moveResizeOutput() != output
            || QRectF(w->window()->moveResizeGeometry()) != to[i]) continue;
        m_bentoMotions.append({w,output,from[i],to[i],QRectF(output->geometry()),clock});
    }
    if (!m_bentoMotions.isEmpty()) KWin::effects->addRepaintFull();
}

void Effect::startDropSettle(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                            const QRectF &from, const QRectF &to)
{
    clearDropSettle();
    // Logical commitment is not proof of native placement. Require KWin's
    // accepted target, not a late client buffer/frame acknowledgement. Painting
    // maps whichever buffer is current; never hold input or retry geometry.
    if (!window || window->isDeleted() || !output || isTabletOutput(output)
        || !from.isValid() || !to.isValid() || from == to
        || !window->window() || window->window()->moveResizeOutput() != output
        || QRectF(window->window()->moveResizeGeometry()) != to
        || window->isUserMove() || window->isUserResize() || window->isMinimized()) return;
    m_settlingWindow = window; m_settlingOutput = output;
    m_settleFrom = from; m_settleTo = to;
    m_settleOutputGeometry = QRectF(output->geometry());
    m_dropSettleTimer.start();
    KWin::effects->addRepaintFull();
}

std::optional<QRectF> Effect::dropSettleRect() const
{
    constexpr double Duration = 220.0;
    if (!m_settlingWindow || m_settlingWindow->isDeleted() || !m_settlingOutput
        || !m_dropSettleTimer.isValid() || m_dropSettleTimer.elapsed() >= Duration
        || !m_settlingWindow->window()
        || m_settlingWindow->window()->moveResizeOutput() != m_settlingOutput
        || QRectF(m_settlingOutput->geometry()) != m_settleOutputGeometry
        || QRectF(m_settlingWindow->window()->moveResizeGeometry()) != m_settleTo
        || m_settlingWindow->isUserMove() || m_settlingWindow->isUserResize()
        || m_settlingWindow->isMinimized()) return std::nullopt;
    const auto t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(m_dropSettleTimer.elapsed() / Duration);
    return QRectF(m_settleFrom.topLeft() + (m_settleTo.topLeft() - m_settleFrom.topLeft()) * t,
                  m_settleFrom.size() + (m_settleTo.size() - m_settleFrom.size()) * t);
}

void Effect::handleManagedStateChanged()
{
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        if (window->window() == sender()) {
            if (window == m_settlingWindow) clearDropSettle();
            m_cardStage->handleActiveGeometryChanged(window);
            return;
        }
    }
}

void Effect::handleWindowMoveResizeStepped(
    KWin::EffectWindow *window, const KWin::RectF &geometry)
{
    m_desktopStage->handleWindowMoveResizeStepped(window, geometry);
}

void Effect::handleWindowMoveResizeFinished(KWin::EffectWindow *window)
{
    if (m_carryRuntime && m_carryRuntime->handoff.ownsNativeFinish(window->window())) return;
    if (m_carryRuntime && m_carryRuntime->deferredFor(window->window())) m_carryRuntime->clearDeferred();
    traceNativeMove(window, "native-finished");
    const bool carried = m_nativeCarry == window;
    const QString source = m_nativeCarrySource;
    const bool fromBento = m_nativeCarryFromBento;
    if (carried) {
        m_nativeCarry = nullptr;
        m_nativeCarrySource.clear();
        m_nativeCarryFromBento = false;
        KWin::effects->setElevatedWindow(window, false);
    }
    m_desktopStage->handleWindowMoveResizeFinished(window);
    // Use KWin's completed output assignment, not the cursor: Escape restores
    // the source output even if the pointer remains over the destination.
    if (carried && !fromBento && window->screen()
        && window->screen()->name() != source) {
        if (isTabletOutput(window->screen())) {
            admitTransferredWindowToTablet(window);
        } else {
            (void)m_desktopStage->admitCardWindow(window, window->screen(), window->frameGeometry());
        }
    }
    if (carried) {
        syncSelectedElevation();
        KWin::effects->addRepaintFull();
    }
}

bool Effect::admitTransferredWindowToTablet(KWin::EffectWindow *window,
    const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore)
{
    // Copy before guest/source cleanup can retire NativeCarryRuntime's pose.
    const QRectF carriedOrigin = window == m_carriedWindow && m_carryRuntime
        ? QRectF(m_carryRuntime->handoff.carry().position(), m_carryPickup.size())
        : QRectF();
    if (m_cardStage->launcherGuestActive()) endLauncherGuest();
    // A caller that still held the window's session record supplies it. Asking
    // the desktop stage afterwards would read the pane rectangle instead, since
    // by then the session has published a plan that no longer names the window.
    const auto source = restore ? std::nullopt : m_desktopStage->prepareNativeCarrySource(window);
    const auto derived = source ? std::optional<NativeMoveSnapshot>(source->restoreSnapshot())
        : std::nullopt;
    if (!restore && derived) restore = &*derived;
    return m_cardStage->admitTransferredWindowToTablet(window, commitSource, carriedOrigin,
        restore);
}

void Effect::handleScreenRemoved(KWin::LogicalOutput *output)
{
    cancelInputForCardStage();
    m_desktopStage->handleScreenRemoved(output);
}

bool Effect::cancelForwardedTouchForInput()
{
    auto *server = KWin::waylandServer();
    if (!server || !server->seat() || !server->seat()->isTouchSequence()) return false;
    // Cancel only the client delivery, not the physical input stream that
    // our router now owns until release. This prevents a swipe becoming a tap.
    server->seat()->notifyTouchCancel();
    return true;
}

QRectF Effect::nativeLandingAreaForOutput(KWin::LogicalOutput *output) const
{
    QList<QRectF> docks;
    for (auto *window : KWin::effects->stackingOrder()) {
        if (window && !window->isDeleted() && window->isDock()
            && window->isOnCurrentDesktop() && window->isOnCurrentActivity()
            && window->window() && window->window()->isShown())
            docks.append(QRectF(window->frameGeometry()));
    }
    return nativeLandingArea(QRectF(output->geometry()),
        QRectF(KWin::effects->clientArea(KWin::MaximizeArea, output)), docks);
}

bool Effect::isPanelPoint(const QPointF &position) const
{
    // Use Plasma's actual input surface, not a fixed bottom strip: floating,
    // vertical and hidden panels must keep their own geometry and visibility.
    // The tray opens a separate AppletPopup surface. Its controls (especially
    // Disable) must remain reachable while the other device owns a card hold.
    // Do not exempt arbitrary app dialogs or the desktop behind the cards.
    for (auto *window : KWin::effects->stackingOrder()) {
        if (window && !window->isDeleted()
            && (window->isDock() || window->isAppletPopup())
            && window->isOnCurrentDesktop() && window->isOnCurrentActivity()
            && window->window() && window->window()->isShown()
            && window->window()->hitTest(position)) return true;
    }
    return false;
}

bool Effect::surfaceOwnsTouchAt(const QPointF &position) const
{
    // Asks for the window KWin itself would deliver the touch to, so a layer
    // surface's input region is honoured: a touch beside a masked strip lands
    // on whatever is beneath and stays a bottom swipe candidate. Panels are
    // excluded because a swipe from the dock is the bottom swipe.
    const KWin::Window *window = KWin::input()->findToplevel(position);
    return window && window->inherits("KWin::LayerShellV1Window")
        && !window->isDock() && !window->isAppletPopup();
}

bool Effect::inputPanelContainsForInput(const QPointF &position) const
{
    // The panel's own input region, not its frame: a keyboard drawn over a
    // wider transparent surface takes only the touches on its keys.
    KWin::EffectWindow *panel = KWin::effects->inputPanel();
    return panel && !panel->isDeleted() && panel->isVisible() && panel->window()
        && panel->window()->hitTest(position);
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

QString Effect::loadedPluginProvenance() const
{
    // Replacing the plugin file gives it a new inode. A KWin that kept the
    // previous image mapped across an unload re-instantiates the earlier build
    // under the same effect id, and the only place that is visible is this
    // process's own map of itself. An unlinked mapping is reported rather than
    // hidden: it is the clearest form of the same answer.
    // Read with a plain stream: procfs reports a size of zero, so QFile's
    // end-of-file and bytes-available answers are derived from a length that
    // does not exist and a QFile loop reads nothing at all.
    std::ifstream maps("/proc/self/maps");
    if (!maps.is_open()) {
        return {};
    }
    std::string raw;
    while (std::getline(maps, raw)) {
        const QString line = QString::fromStdString(raw).trimmed();
        const qsizetype pathStart = line.indexOf(QLatin1Char('/'));
        if (pathStart < 0) {
            continue;
        }
        QString path = line.mid(pathStart);
        const bool deleted = path.endsWith(QLatin1String(" (deleted)"));
        if (deleted) {
            path.chop(QLatin1String(" (deleted)").size());
        }
        if (!path.endsWith(QLatin1String("/kwin4_effect_kadunce.so"))) {
            continue;
        }
        const QStringList fields = line.left(pathStart).split(QLatin1Char(' '),
                                                              Qt::SkipEmptyParts);
        if (fields.size() < 5) {
            continue;
        }
        return QStringLiteral("%1 %2").arg(fields.at(4),
            deleted ? QStringLiteral("deleted") : QStringLiteral("present"));
    }
    return {};
}

QString Effect::workspaceContext() const
{
    const auto workspace = m_cardStage->workspaceSnapshot();
    KWin::EffectWindow *focused = KWin::effects->activeWindow();

    QJsonArray applications;
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        if (!isApplicationWindow(window)) {
            continue;
        }
        const auto *card = workspace.find(windowIdentity(window));
        QJsonObject application{
            {QStringLiteral("windowId"), windowIdentity(window)},
            {QStringLiteral("appId"), applicationIdentity(window)},
            {QStringLiteral("title"), window->caption()},
            {QStringLiteral("output"),
             window->screen() ? window->screen()->name() : QString()},
            {QStringLiteral("focused"), window == focused},
            {QStringLiteral("lastActivated"), static_cast<qint64>(m_activationOrder.value(windowIdentity(window)))},
            {QStringLiteral("minimized"), window->isMinimized()},
            {QStringLiteral("hasCard"), card != nullptr},
        };
        if (card) {
            application.insert(QStringLiteral("cardId"),
                               windowIdentity(window));
            application.insert(QStringLiteral("cardIndex"), card->cardIndex);
            application.insert(QStringLiteral("stackId"),
                               card->stackId);
            application.insert(QStringLiteral("stackPosition"),
                               card->stackPosition);
            application.insert(QStringLiteral("stackSize"),
                               card->stackSize);
            application.insert(QStringLiteral("selected"),
                               card->selected);
        }
        applications.append(application);
    }

    QJsonObject focus;
    if (focused && isApplicationWindow(focused)) {
        const bool focusedHasCard = workspace.find(windowIdentity(focused)) != nullptr;
        focus = {
            {QStringLiteral("windowId"), windowIdentity(focused)},
            {QStringLiteral("appId"), applicationIdentity(focused)},
            {QStringLiteral("title"), focused->caption()},
            {QStringLiteral("hasCard"), focusedHasCard},
        };
        if (focusedHasCard) {
            focus.insert(QStringLiteral("cardId"),
                         windowIdentity(focused));
        }
    }

    QJsonArray selectedStack;
    const QString selectedCardId = workspace.selectedCardId;
    for (const auto &member : workspace.selectedStack) selectedStack.append(member);

    // §2 has three presentations and this reported two, so the one where the
    // display shows its panes and Card Stage holds its cards hidden read as
    // Active -- the state physical review could not tell from a card actually
    // drawn over the layout. `cardLine` is a frozen interface identity; adding
    // a value beside it is not a rename.
    const QString presentation = !m_cardStage->isActive()
        ? QStringLiteral("inactive")
        : m_cardStage->presentation() == CardPresentation::Spread
            ? QStringLiteral("cardLine")
            : m_cardStage->presentation() == CardPresentation::Bento
                ? QStringLiteral("bento") : QStringLiteral("active");

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
    return 3;
}

QString Effect::beginLauncherGuest(const QString &ownerService)
{
    QJsonObject reply{
        {QStringLiteral("protocol"), launcherGuestProtocolVersion()},
        {QStringLiteral("accepted"), false},
    };
    const QString owner = ownerService.trimmed();
    if (!owner.startsWith(QLatin1Char(':'))) {
        return QString::fromUtf8(
            QJsonDocument(reply).toJson(QJsonDocument::Compact));
    }

    if (presentationForInput() != WorkspacePresentation::Spread) {
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
    ++m_guestGeneration;
    m_launcherGuestLaunchPending = false;
    m_launcherGuestLaunchApps.clear();
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered,
            this, [this, owner](const QString &service) {
        if (service == owner && m_launcherGuestOwner == owner) {
            endLauncherGuest();
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

void Effect::updateLauncherGuest(double horizontalDelta)
{
    if (m_launcherGuestExpanded) return;
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
    if (!m_guestNeighborMotion.isValid() || m_guestNeighborMotion.elapsed() >= 220) return target;
    const double t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
        m_guestNeighborMotion.elapsed() / 220.0);
    return m_guestNeighborFrom + (target - m_guestNeighborFrom) * t;
}

bool Effect::finishLauncherGuest(double horizontalDelta)
{
    if (m_launcherGuestExpanded) return false;
    const bool committed =
        m_cardStage->finishLauncherGuest(horizontalDelta);
    if (committed) {
        // The layer-shell guest relinquishes keyboard focus when it hides.
        // That restoration is not a request to expand a card.
        auto *focusReturn = KWin::effects->activeWindow();
        m_guestSwipeFocusReturn = isApplicationWindow(focusReturn)
            ? focusReturn : m_cardStage->selectedWindow();
        const auto generation = m_guestGeneration;
        QTimer::singleShot(1000, this, [this, generation]() {
            if (generation == m_guestGeneration || !m_cardStage->launcherGuestActive())
                m_guestSwipeFocusReturn.clear();
        });
        QTimer::singleShot(220, this, [this, generation]() {
            if (generation == m_guestGeneration && m_cardStage->launcherGuestActive()) {
                endLauncherGuest();
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
        || m_launcherGuestOwner.isEmpty()
        || identities.isEmpty() || identities.size() > 8 || requestToken.isEmpty()) {
        return false;
    }
    m_launcherGuestLaunchPending = true;
    m_launcherGuestLaunchApps = identities;
    m_launcherGuestLaunchToken = requestToken;
    const auto generation = ++m_guestGeneration;
    QTimer::singleShot(10000, this, [this, generation]() {
        if (generation == m_guestGeneration) cancelLauncherGuestLaunch();
    });
    return true;
}

void Effect::cancelLauncherGuestLaunch()
{
    ++m_guestGeneration;
    m_launcherGuestLaunchPending = false;
    m_launcherGuestLaunchApps.clear();
    m_launcherGuestLaunchToken.clear();
}

void Effect::endLauncherGuest()
{
    m_launcherGuestExpanded = false;
    m_guestNeighborMotion.invalidate();
    cancelLauncherGuestLaunch();
    m_cardStage->endLauncherGuest();
    m_launcherGuestOwner.clear();
    if (m_launcherGuestWatcher) {
        m_launcherGuestWatcher->deleteLater();
        m_launcherGuestWatcher = nullptr;
    }
}

bool Effect::toggleBentoOnOutput(const QString &outputName)
{
    const bool toggled = m_desktopStage->toggleOnOutput(outputName);
    observeCardOwnership();
    return toggled;
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

void Effect::beginCardGrab(const QPointF &position)
{
    m_lineDestination.reset(); m_lineDestinationWindow.clear();
    m_lineCardEntryOutput.clear();
    m_cardStage->beginCardGrab(position);
}

void Effect::updateCardGrab(const QPointF &position)
{
    const auto previousDestination = m_lineDestination;
    m_linePreview.reset();
    m_lineDestination.reset(); m_lineDestinationWindow.clear();
    m_lineCardEntryOutput.clear();
    m_cardStage->updateCardGrab(position);
    if (!m_cardStage->cardGrabActive()) return;
    for (auto *output : KWin::effects->screens()) {
        if (!QRectF(output->geometry()).contains(position)) continue;
        const auto edge = isPanelPoint(position) ? std::nullopt
            : monitorCarryEdge(QRectF(output->geometry()), position);
        std::optional<BentoSidePlacement> side;
        if (edge && (*edge == CarryEdge::Left || *edge == CarryEdge::Right))
            side = bentoSideChoice(*edge == CarryEdge::Right, position.y(),
                QRectF(output->geometry()).center().y(), previousDestination
                    && previousDestination->destinationOutput() == output
                    ? previousDestination->sidePlacement() : std::nullopt);
        if (m_cardStage->canOwnCards(output) && !edge) continue;
        const KWin::RectF geometry(QRectF(m_cardStage->cardGrabTarget())
            .translated(m_cardStage->cardGrabOffset()));
        if (m_cardStage->canOwnCards(output)) {
            // CARD-LIFECYCLE.md §3 and §10, asked exactly as the native carry
            // seam asks it. Spread selection chooses what is carried; §3 names
            // who it pairs with.
            auto *grabbed = selectedWindow();
            const bool leftEdge = *edge == CarryEdge::Left;
            const bool sideEdge = leftEdge || *edge == CarryEdge::Right;
            auto *partner = sideEdge
                ? m_cardStage->partnerForSideSnap(grabbed, leftEdge) : nullptr;
            const bool liveLayout = m_desktopStage->hasSessionOnOutput(output->name());
            const auto outcome = planEdgeEntry({
                .edge = *edge,
                // §3: a display with a live layout is owned, whether or not
                // this stage also holds individual cards on it.
                .ownsDisplay = m_cardStage->ownsDisplay(output) || liveLayout,
                .canOwnCards = true,
                .hasBentoLayout = liveLayout,
                .carriedEligible = isCardWindow(grabbed),
                .partnerNamed = partner != nullptr,
                .carriedIsActive = m_cardStage->activeCardIdentity() == grabbed,
            });
            if (outcome == EdgeEntryOutcome::MakeActive) {
                m_lineCardEntryOutput = output;
                m_linePreview = KWin::RectF(m_cardStage->activeTarget(output));
                m_lineDestinationWindow = grabbed;
                m_lineDestinationContact = position;
                break;
            }
            if (outcome != EdgeEntryOutcome::PairIntoBento || !side) break;
            m_lineDestination = m_desktopStage->prepareCardDrop(grabbed, output, geometry,
                DesktopStageController::CardDropIntent::ActivateBento, side, partner);
            if (m_lineDestination) {
                m_linePreview = m_desktopStage->cardDropPreview(*m_lineDestination);
                if (!m_linePreview) { m_lineDestination.reset(); break; }
                m_lineDestinationWindow = grabbed;
                m_lineDestinationContact = position;
            }
            break;
        }
        m_lineDestination = m_desktopStage->prepareCardDrop(selectedWindow(), output, geometry,
            edge ? DesktopStageController::CardDropIntent::ActivateBento
                 : DesktopStageController::CardDropIntent::OpenSpace, side);
        if (m_lineDestination) {
            m_linePreview = m_desktopStage->cardDropPreview(*m_lineDestination);
            if (side && !m_linePreview) { m_lineDestination.reset(); continue; }
            m_lineDestinationWindow = selectedWindow();
            m_lineDestinationContact = position;
        }
        break;
    }
}

void Effect::pageCardGrab(int direction)
{
    m_cardStage->pageCardGrab(direction);
}

void Effect::finishCardGrab(bool commit)
{
    m_cardStage->finishCardGrab(commit);
    m_lineDestination.reset(); m_lineDestinationWindow.clear();
    m_lineCardEntryOutput.clear();
}

bool Effect::finishCardGrabOnOutput(const QPointF &position)
{
    // A release cannot invent a destination that was never evaluated in motion.
    if (position != m_lineDestinationContact) {
        m_lineDestination.reset();
        m_lineCardEntryOutput.clear();
    }
    if (m_lineCardEntryOutput) {
        // §10: nothing distinct to pair with, so the carried card becomes the
        // Active card. Its grab is cancelled first, which returns it to the
        // membership it was lifted from.
        QPointer<KWin::EffectWindow> grabbed = m_lineDestinationWindow;
        m_lineCardEntryOutput.clear(); m_lineDestinationWindow.clear();
        m_cardStage->finishCardGrab(false);
        (void)m_cardStage->promoteToActive(grabbed);
        return true;
    }
    if (m_lineDestination
        && m_cardStage->canOwnCards(m_lineDestination->destinationOutput())) {
        const auto reserved = *m_lineDestination;
        QPointer<KWin::EffectWindow> carried = m_lineDestinationWindow;
        // The partner the reservation was prepared and validated against, never
        // one re-read at release: a selection between the two can otherwise
        // surrender one window's ownership while the layout publishes another.
        QPointer<KWin::EffectWindow> partner = reserved.namedPartner();
        if (!m_desktopStage->activatePreparedTabletDrop(reserved, nullptr,
                [this, carried, partner] {
                    return m_cardStage->releasePairToBento(carried, partner);
                })) {
            m_cardStage->finishCardGrab(false);
        }
        m_lineDestination.reset(); m_lineDestinationWindow.clear();
        return true;
    }
    // §10: an edge action commits only when released inside its valid edge
    // zone, and a release inside one is an edge action whether or not it was
    // admitted. Neither branch above claimed this release, so the entry rule
    // refused it: §3's Active card with no eligible partner, a pair that could
    // not be prepared, or a side snap §5 gives to a live layout. Handing it to
    // the ordinary Spread drop instead would move the card one place in Spread
    // order and take a named member out of its stack, which §14 forbids a
    // refused gesture from doing. Cancelling the grab restores the membership
    // the card was lifted from.
    if (auto *edgeOutput = KWin::effects->screenAt(position.toPoint());
        edgeOutput && QRectF(edgeOutput->geometry()).contains(position)
        && m_cardStage->canOwnCards(edgeOutput) && !isPanelPoint(position)
        && monitorCarryEdge(QRectF(edgeOutput->geometry()), position)) {
        m_cardStage->finishCardGrab(false);
        m_lineDestinationWindow.clear();
        return true;
    }
    const bool result = m_cardStage->finishCardGrabOnOutput(position);
    m_lineDestination.reset(); m_lineDestinationWindow.clear();
    return result;
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
    const auto ticket = m_inputActivationGuard.issue();
    const QPointer<KWin::EffectWindow> requested = selectedWindow();
    QTimer::singleShot(0, this, [this, ticket, requested]() {
        if (m_inputActivationGuard.accepts(ticket)
            && requested && !requested->isDeleted() && selectedWindow() == requested
            && !nativeWindowInteractionForInput() && !m_cardStage->cardGrabActive()
            && !m_cardStage->launcherGuestActive() && m_cardStage->isActive()
            && m_cardStage->presentation() == CardPresentation::Spread) {
            // CARD-LIFECYCLE.md section 6: selecting the Bento group resumes
            // the group. A refused resume is not an invitation to try the
            // individual-card path on it, which cannot admit a pane and leaves
            // the selection doing nothing at all.
            if (m_cardStage->selectedIsBentoGroup()) {
                if (!m_cardStage->resumeSelectedBentoProjection()) {
                    qWarning() << "Kadunce" << Revision
                               << "declined to resume the selected Bento group";
                }
            } else {
                toggle();
            }
        }
    });
}

void Effect::toggle()
{
    if (m_cardStage->launcherGuestActive()) {
        dismissLauncherGuestFromInput();
    }
    {
        // §2: the live session becomes the Bento group entry in Spread, whether
        // or not this stage already owns individual cards beside it.
        KWin::LogicalOutput *tablet = tabletOutput();
        if (tablet && m_desktopStage->hasSessionOnOutput(tablet->name())) {
            m_desktopStage->transferTabletSessionToSpread(tablet,
                [this](const auto &projection, const auto &commit) {
                    return m_cardStage->admitBentoStack(projection, commit);
                });
            // Rejection retains Bento; never fall through to rediscovery.
            observeCardOwnership();
            return;
        }
    }
    if (m_cardStage->selectedIsBentoGroup()) {
        if (!m_cardStage->resumeSelectedBentoProjection()) {
            qWarning() << "Kadunce" << Revision
                       << "declined to resume the selected Bento group";
        }
        observeCardOwnership();
        return;
    }
    m_cardStage->toggle();
    observeCardOwnership();
}

void Effect::release()
{
    if (m_carryRuntime) m_carryRuntime->cancel();
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
    observeCardOwnership();
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
    const auto neighbors = m_cardStage->preparationNeighbors();
    for (const auto &window : std::as_const(m_preparationNeighbors)) {
        if (window && !neighbors.contains(window)
            && (!m_cardStage->isActive()
                || m_cardStage->presentation() != CardPresentation::Spread
                || m_cardStage->paintSlot(window) == 99)) unredirect(window);
    }
    if (neighbors != m_preparationNeighbors) m_neighborPreparationFrames = neighbors.size();
    m_preparationNeighbors = neighbors;
    if (isTabletOutput(data.screen)) m_neighborPreparedThisFrame = false;
    m_continueRepaint = false;
    if (m_cardStage->launcherGuestActive() && m_guestNeighborMotion.isValid()
        && m_guestNeighborMotion.elapsed() < 220) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        m_continueRepaint = true;
    }
    for (auto it = m_bentoMotions.begin(); it != m_bentoMotions.end();) {
        if (!bentoMotionRect(it->window)) {
            if (it->window && !it->window->isDeleted()) unredirect(it->window);
            it = m_bentoMotions.erase(it);
        } else {
            data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
            m_continueRepaint = true;
            ++it;
        }
    }
    if (m_settlingWindow) {
        if (!dropSettleRect()) clearDropSettle();
        else { data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS; m_continueRepaint = true; }
    }
    if (m_carriedWindow) data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    if (m_cardStage->isActive()
        && (isTabletOutput(data.screen) || m_cardStage->cardGrabActive())) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        if (m_cardStage->animationsRunning()) {
            m_continueRepaint = true;
        }
    }
    KWin::effects->prePaintScreen(data);
}

void Effect::postPaintScreen()
{
    // KWin consumes current layer damage after prePaintScreen. Request the
    // NEXT frame only after paint. Latch pre-paint activity so an animation
    // expiring during this frame still gets one exact final-state frame.
    const bool continueRepaint = m_continueRepaint;
    m_continueRepaint = false;
    KWin::effects->postPaintScreen();
    if (continueRepaint) {
        KWin::effects->addRepaintFull();
    }
}

void Effect::prePaintWindow(KWin::RenderView *view,
                            KWin::EffectWindow *window,
                            KWin::WindowPrePaintData &data)
{
    if (window == m_carriedWindow || (window == m_settlingWindow && dropSettleRect()) || bentoMotionRect(window)) {
        data.setTransformed(); data.setTranslucent();
    }
    if (m_cardStage->isActive()
        && window != m_nativeCarry
        && m_cardStage->presentation() == CardPresentation::Spread
        && m_cardStage->paintSlot(window) != 99) {
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
    if (screen && screen == tabletOutput()) m_cardLabelTargets.clear();
    KWin::effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
    if (m_destinationShader && screen && (screen != tabletOutput() || !m_cardStage->isActive())
        && !m_carriedWindow && !m_cardStage->cardGrabActive()) {
        QList<QRectF> pills;
        for (const auto &rail : m_desktopStage->grabRails())
            if (rail.output == screen->name()) pills.append(rail.pill);
        const auto previews = m_desktopStage->railPreview(screen->name());
        const auto drawRailShape = [&](const QRectF &box, bool pill) {
            QList<QVector2D> vertices;
            for (const auto &dirty : deviceRegion.rects()) {
                const auto part = QRectF(viewport.mapFromDeviceCoordinates(KWin::RectF(dirty)))
                    .intersected(QRectF(screen->geometry())).intersected(box);
                if (part.isEmpty()) continue;
                vertices << QVector2D(part.topLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomLeft())
                    << QVector2D(part.bottomLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomRight());
            }
            if (vertices.isEmpty()) return;
            KWin::ShaderBinder binder(m_destinationShader.get());
            auto matrix = viewport.projectionMatrix();
            matrix.scale(viewport.scale(), viewport.scale());
            m_destinationShader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
            m_destinationShader->setUniform("destinationBox", QVector4D(box.x(),box.y(),box.width(),box.height()));
            m_destinationShader->setUniform("outlineRadius", pill ? 2.f : 10.f);
            m_destinationShader->setUniform("surfaceFill", QVector4D(.88f,.88f,.88f,pill ? .85f : .035f));
            m_destinationShader->setUniform("outlineOpacity", pill ? 0.f : .8f);
            m_destinationShader->setColorspaceUniforms(KWin::ColorDescription::sRGB,
                renderTarget.colorDescription(), KWin::RenderingIntent::Perceptual);
            const bool blended = glIsEnabled(GL_BLEND);
            GLint sr,dr,sa,da;
            glGetIntegerv(GL_BLEND_SRC_RGB,&sr); glGetIntegerv(GL_BLEND_DST_RGB,&dr);
            glGetIntegerv(GL_BLEND_SRC_ALPHA,&sa); glGetIntegerv(GL_BLEND_DST_ALPHA,&da);
            glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
            auto *buffer = KWin::GLVertexBuffer::streamingBuffer();
            buffer->reset(); buffer->setVertices(vertices); buffer->render(GL_TRIANGLES);
            glBlendFuncSeparate(sr,dr,sa,da);
            if (!blended) glDisable(GL_BLEND);
        };
        for (const auto &box : previews) drawRailShape(box,false);
        if (!previews.isEmpty()) for (const auto &pill : pills) drawRailShape(pill,true);
    }
    // Draw only from a still-owned reservation. No layout solve, timers, native
    // outline window, or input grab belongs in the paint pass.
    const auto &reservation = m_carriedWindow ? m_carryDestination : m_lineDestination;
    const auto &preview = m_carriedWindow ? m_carryPreview : m_linePreview;
    const auto cardEntry = m_carriedWindow ? m_carryCardEntryOutput : m_lineCardEntryOutput;
    if (m_destinationShader && screen && preview
        && (m_carriedWindow || m_cardStage->cardGrabActive())
        && (cardEntry == screen
            || (reservation && !cardEntry && reservation->showsPlacementOutline()
                && reservation->destinationOutput() == screen
                && m_desktopStage->cardDropValid(*reservation)))) {
        const QRectF box(*preview);
        QRectF clip = QRectF(KWin::effects->clientArea(KWin::MaximizeArea, screen))
            .intersected(QRectF(screen->geometry()));
        clip.setBottom(std::min(clip.bottom(), double(screen->geometry().bottom()) - 48.0));
        QList<QVector2D> vertices;
        for (const auto &dirty : deviceRegion.rects()) {
            const auto part = QRectF(viewport.mapFromDeviceCoordinates(KWin::RectF(dirty)))
                .intersected(clip).intersected(box);
            if (part.isEmpty()) continue;
            vertices << QVector2D(part.topLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomLeft())
                     << QVector2D(part.bottomLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomRight());
        }
        if (!vertices.isEmpty()) {
            KWin::ShaderBinder binder(m_destinationShader.get());
            auto matrix = viewport.projectionMatrix();
            matrix.scale(viewport.scale(), viewport.scale());
            m_destinationShader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
            m_destinationShader->setUniform("destinationBox", QVector4D(box.x(), box.y(), box.width(), box.height()));
            m_destinationShader->setUniform("outlineRadius", 10.0f);
            m_destinationShader->setUniform("surfaceFill", QVector4D(0.88f, 0.88f, 0.88f, 0.035f));
            m_destinationShader->setUniform("outlineOpacity", 0.65f);
            m_destinationShader->setColorspaceUniforms(KWin::ColorDescription::sRGB,
                renderTarget.colorDescription(), KWin::RenderingIntent::Perceptual);
            const bool blended = glIsEnabled(GL_BLEND);
            GLint srcRgb, dstRgb, srcAlpha, dstAlpha;
            glGetIntegerv(GL_BLEND_SRC_RGB, &srcRgb); glGetIntegerv(GL_BLEND_DST_RGB, &dstRgb);
            glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            auto *buffer = KWin::GLVertexBuffer::streamingBuffer();
            buffer->reset(); buffer->setVertices(vertices); buffer->render(GL_TRIANGLES);
            glBlendFuncSeparate(srcRgb, dstRgb, srcAlpha, dstAlpha);
            if (!blended) glDisable(GL_BLEND);
        }
        // The outline above is drawn for a Bento reservation or for a Card
        // Stage entry, and only the first has a reservation to ask. §10 gives
        // the bottom edge the only detaching release, and an entry is never
        // one, so a card entry simply has no label to draw here.
        if (reservation && !cardEntry && reservation->detachesToDesktop())
            m_detachLabel.render(renderTarget, viewport, box);
    }
    // Text-only feedback uses the same validated slot as release. It owns no
    // input or geometry and vanishes as soon as insertion is no longer valid.
    if (screen && screen == tabletOutput()
        && m_cardStage->stackInsertionPreviewValid()) {
        const QRectF work(KWin::effects->clientArea(KWin::MaximizeArea, screen));
        m_stackSlotLabel.render(renderTarget, viewport, work,
            tr("place in slot %1 of %2")
                .arg(m_cardStage->stackInsertionIndex() + 1)
                .arg(m_cardStage->model().stackSizeForId(m_cardStage->stackPreviewTarget()) + 1));
    }
    if (screen && screen == tabletOutput() && m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Spread) {
        const auto &model = m_cardStage->model();
        const auto &windows = m_cardStage->liveCards();
        for (int index = 0; index < windows.size(); ++index) {
            auto *window = windows.at(index).data();
            if (!window || window->isDeleted() || m_cardStage->paintSlot(window) == 99)
                continue;
            const int cardId = index + 1;
            if (model.stackPositionForId(cardId)
                != model.stackActivePositionForId(cardId)) continue;
            const bool bentoGroup = m_cardStage->usesBentoProjectionAperture(window);
            QString applicationName;
            if (bentoGroup) {
                QStringList paneNames;
                for (const auto &pane : m_cardStage->bentoProjectionPanes()) {
                    if (pane && !pane->isDeleted())
                        paneNames.append(applicationDisplayName(pane));
                }
                applicationName = bentoApplicationNames(paneNames);
            } else {
                applicationName = applicationDisplayName(window);
            }
            const int count = model.stackSizeForId(cardId);
            const auto target = m_cardLabelTargets.constFind(window);
            if (target == m_cardLabelTargets.cend()) continue;
            m_cardLabelRenderer.render(renderTarget, viewport,
                *target,
                {applicationName, stackPositionLabel(
                    model.stackActivePositionForId(cardId), count, bentoGroup)});
        }
    }
    m_paintingOutput = nullptr;
}

void Effect::observeCardOwnership()
{
    std::vector<quintptr> cards;
    const auto &live = m_cardStage->liveCards();
    cards.reserve(static_cast<std::size_t>(live.size()));
    for (const auto &window : live) {
        if (window) cards.push_back(reinterpret_cast<quintptr>(window.data()));
    }
    const auto sessions = m_desktopStage->ownershipView();

    // Every window whose owner changed is put to the ledger as a transition.
    // The ledger accepts only the six the contract defines, so a change it
    // refuses is a change §14 does not permit, reported from this one place.
    std::vector<OwnershipViolation> reported;
    QSet<quintptr> present;
    const auto put = [&](quintptr window, CardOwner observed, const QString &output) {
        present.insert(window);
        if (m_ownership.ownerOf(window) == observed) return;
        if (!m_ownership.transfer(window, observed, output)) {
            reported.push_back({window, OwnershipViolation::Rule::TwoOwners, output});
        }
    };
    for (const auto &session : sessions) {
        for (const auto pane : session.panes) {
            if (pane) put(pane, CardOwner::BentoPane, session.output);
        }
    }
    for (const auto card : cards) {
        if (card) put(card, CardOwner::IndividualCard, QString());
    }
    // A window that left both containers returned to Plasma.
    for (const auto known : m_ownership.windowsOwnedAs(CardOwner::IndividualCard)) {
        if (!present.contains(known)) put(known, CardOwner::Native, QString());
    }
    for (const auto known : m_ownership.windowsOwnedAs(CardOwner::BentoPane)) {
        if (!present.contains(known)) put(known, CardOwner::Native, QString());
    }

    auto violations = m_ownership.reconcile(cards, sessions);
    violations.insert(violations.end(), reported.cbegin(), reported.cend());

    // Report a shape once. The observer runs on every published change, and a
    // standing defect must not bury the next new one in repetition.
    if (violations == m_observedOwnershipViolations) return;
    for (const auto &violation : violations) {
        if (std::find(m_observedOwnershipViolations.cbegin(),
                      m_observedOwnershipViolations.cend(), violation)
            != m_observedOwnershipViolations.cend()) continue;
        qWarning() << "Kadunce card ownership:"
                   << describeOwnershipViolation(violation);
    }
    m_observedOwnershipViolations = std::move(violations);
}

KWin::EffectWindow *Effect::selectedWindow() const
{
    return m_cardStage->selectedWindow();
}

void Effect::handleWindowAdded(KWin::EffectWindow *window)
{
    if (m_carriedWindow && m_carryRuntime) m_carryRuntime->cancel();
    Q_EMIT workspaceContextChanged();
    connectManagedWindow(window);
    const QPointer<KWin::EffectWindow> candidate(window);
    const auto admitReadyWindow = [this, candidate]() {
        if (!candidate || candidate->isDeleted() || !isCardWindow(candidate)
            || !candidate->window() || !candidate->window()->readyForPainting()) {
            return;
        }
        if (m_desktopStage->handleWindowAdded(candidate)) {
            observeCardOwnership();
            return;
        }
        // §8: the layout could not take it, so the layout leaves the screen
        // before the card stage puts a card where the panes were. Without
        // this the card is drawn over a running layout, which is the one
        // state §2 does not name.
        (void)retireLayoutIntoSpreadGroup(candidate->screen());
        (void)m_cardStage->handleWindowAdded(candidate);
        completeLauncherGuestForWindow(candidate);
        observeCardOwnership();
    };
    QTimer::singleShot(0, this, admitReadyWindow);
    if (window->window() && !window->window()->readyForPainting())
        connect(window->window(), &KWin::Window::readyForPaintingChanged,
                this, admitReadyWindow, Qt::SingleShotConnection);
}

void Effect::handleWindowClosed(KWin::EffectWindow *window)
{
    m_applicationDisplayNames.remove(window);
    m_cardLabelTargets.remove(window);
    // A destroyed window leaves ownership without a transition: it no longer
    // exists to own, and its identity may be reused by the next allocation.
    m_ownership.forget(reinterpret_cast<quintptr>(window));
    if (window == m_settlingWindow) clearDropSettle();
    if (window == m_carriedWindow && m_carryRuntime) m_carryRuntime->cancel();
    m_activationOrder.remove(windowIdentity(window));
    m_desktopStage->handleWindowClosed(window);
    m_cardStage->handleWindowClosed(window);
    Q_EMIT workspaceContextChanged();
}

void Effect::handleWindowActivated(KWin::EffectWindow *window)
{
    if (m_settlingWindow && window != m_settlingWindow) clearDropSettle();
    if (m_carriedWindow && window != m_carriedWindow && m_carryRuntime) m_carryRuntime->cancel();
    if (isApplicationWindow(window) && m_guestSwipeFocusReturn) {
        const bool restored = window == m_guestSwipeFocusReturn;
        m_guestSwipeFocusReturn.clear();
        if (restored) return;
    }
    if (isApplicationWindow(window)) {
        m_activationOrder.insert(windowIdentity(window), ++m_activationSequence);
        Q_EMIT workspaceContextChanged();
    }
    if (m_cardStage->launcherGuestActive()
        && isApplicationWindow(window)) {
        if (m_launcherGuestLaunchPending) {
            completeLauncherGuestForWindow(window);
            return;
        }
        if (!m_launcherGuestLaunchApps.isEmpty()) return; // Completion is already settling.
        dismissLauncherGuestFromInput();
    }
    if (admitActivatedCardToLiveBento(window)) return;
    m_cardStage->handleWindowActivated(window);
}

bool Effect::admitActivatedCardToLiveBento(KWin::EffectWindow *window)
{
    // §8: a display presenting its layout answers a called card with the
    // layout. §2 names three presentations and none of them is a card drawn
    // over live panes, so where the layout cannot show the card the layout
    // stops being what the display presents rather than being covered.
    if (!window || window->isDeleted() || !isCardWindow(window)
        || !m_cardStage->presentsBento()
        || m_cardStage->liveCardIndex(window) < 0) return false;
    KWin::LogicalOutput *output = window->screen();
    if (!output || !m_desktopStage->hasSessionOnOutput(output->name())) return false;
    if (m_desktopStage->admitCardToLiveBento(window,
            [this, window] { return m_cardStage->releaseCardToLiveBento(window); })) {
        observeCardOwnership();
        return true;
    }
    // No slot the card's minimum size fits, so §8's other answer applies: the
    // layout becomes a Spread group and the card can be Active beside it.
    if (!retireLayoutIntoSpreadGroup(output)) {
        // Rejection retains Bento. Answering here is what keeps the refusal
        // from falling through to a card drawn over it.
        return true;
    }
    observeCardOwnership();
    return false;
}

bool Effect::retireLayoutIntoSpreadGroup(KWin::LogicalOutput *output)
{
    // §8: a layout that cannot take an arrival stops being what the display
    // presents, rather than staying live behind the card the arrival becomes.
    // Both arrival paths need this, and the one that did not have it is what
    // put a launched application on top of a running layout.
    if (!output || output != tabletOutput() || !m_cardStage->presentsBento()
        || !m_desktopStage->hasSessionOnOutput(output->name())) return false;
    return m_desktopStage->transferTabletSessionToSpread(output,
        [this](const auto &projection, const auto &commit) {
            return m_cardStage->admitBentoStack(projection, commit);
        });
}

void Effect::handleLaunchWindowChanged()
{
    m_applicationDisplayNames.clear();
    KWin::effects->addRepaintFull();
    if (!m_launcherGuestLaunchPending) return;
    for (auto *window : KWin::effects->stackingOrder()) {
        if (window->window() == sender() && completeLauncherGuestForWindow(window)) return;
    }
}

bool Effect::completeLauncherGuestForWindow(KWin::EffectWindow *window)
{
    if (!m_launcherGuestLaunchPending || !m_cardStage->launcherGuestActive()
        || !isApplicationWindow(window) || !window->window()
        || !window->window()->readyForPainting() || !window->window()->isShown()) return false;
    bool matches = false;
    for (const auto &identity : m_launcherGuestLaunchApps) {
        matches |= LaunchIdentity::matches(identity, applicationIdentity(window));
    }
    if (!matches) return false;
    (void)m_cardStage->handleWindowAdded(window);
    m_launcherGuestLaunchPending = false;
    const auto generation = ++m_guestGeneration;
    m_cardStage->stageWindowArrival(window);
    QDBusMessage ready = QDBusMessage::createMethodCall(m_launcherGuestOwner,
        QStringLiteral("/Launcher"), QStringLiteral("io.github.carlsonjm.Tettegouche"),
        QStringLiteral("completeGuestLaunch"));
    ready.setArguments({m_launcherGuestLaunchToken});
    QDBusConnection::sessionBus().asyncCall(ready);
    QTimer::singleShot(220, this, [this, generation]() {
        if (generation != m_guestGeneration) return;
        endLauncherGuest();
    });
    return true;
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

void Effect::redirectPreviewSource(KWin::EffectWindow *window)
{
    const QRectF frame(window->frameGeometry());
    const std::array<QRectF, 3> sourceBounds = {
        QRectF(QPointF(), frame.size()),
        QRectF(window->bufferGeometry()).translated(-frame.topLeft()),
        QRectF(window->expandedGeometry()).translated(-frame.topLeft())};
    const auto previousBounds = m_previewSourceBounds.constFind(window);
    if (previousBounds != m_previewSourceBounds.cend() && *previousBounds != sourceBounds) {
        // Damage alone does not describe a changed frame/buffer mapping.
        qInfo() << "Kadunce preview source bounds changed" << window->caption()
                << "frame" << (*previousBounds)[0] << "->" << sourceBounds[0]
                << "buffer" << (*previousBounds)[1] << "->" << sourceBounds[1]
                << "expanded" << (*previousBounds)[2] << "->" << sourceBounds[2];
        unredirect(window);
    }
    m_previewSourceBounds.insert(window, sourceBounds);
    redirect(window);
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
        // Redirection is intentionally ephemeral. Active, Release,
        // hidden deck members, and shader failure all return
        // to KWin's exact r20 direct path.
        unredirect(window);
        m_previewSourceBounds.remove(window);
        KWin::OffscreenEffect::drawWindow(
            renderTarget, viewport, window, mask, deviceRegion, data);
        return;
    }

    redirectPreviewSource(window);
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
    m_fanApertureShader->setUniform("tiltedSampling",
        qFuzzyIsNull(data.rotationAngle()) ? 0.0F : 1.0F);
    glActiveTexture(GL_TEXTURE0);

    KWin::OffscreenEffect::drawWindow(
        renderTarget, viewport, window, mask, deviceRegion, data);
    KWin::ShaderManager::instance()->popShader();
    // Remain inside the draw-chain callback: OffscreenEffect's internal render
    // must continue AFTER this effect, not re-enter our drawWindow from paintScreen.
    // An empty final clip populates the live texture without painting on any output.
    if (!m_neighborPreparedThisFrame && m_paintingOutput
        && isTabletOutput(m_paintingOutput) && !m_preparationNeighbors.isEmpty()) {
        m_neighborPreparedThisFrame = true;
        if (m_neighborPreparationFrames > 0 && --m_neighborPreparationFrames > 0)
            m_continueRepaint = true; // only enough frames to visit both neighbors
        const auto neighbor = m_preparationNeighbors.at(
            m_neighborPreparationCursor++ % m_preparationNeighbors.size());
        if (neighbor && !neighbor->isDeleted() && neighbor != m_nativeCarry
            && m_cardStage->visibleSlot(neighbor) == 99 && neighbor->screen()) {
            const auto size = neighbor->expandedGeometry().size();
            const double scale = neighbor->screen()->scale();
            // At most two extra full-window surfaces, each bounded to32MiB.
            if (size.width() > 0 && size.height() > 0
                && size.width()*size.height()*scale*scale*4 <= 32*1024*1024) {
                redirectPreviewSource(neighbor);
                setShader(neighbor, nullptr);
                KWin::WindowPaintData preparation;
                KWin::OffscreenEffect::drawWindow(renderTarget, viewport, neighbor,
                    PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT,
                    KWin::Region(), preparation);
            }
        }
    }
}

void Effect::paintWindow(const KWin::RenderTarget &renderTarget,
                         const KWin::RenderViewport &viewport,
                         KWin::EffectWindow *window,
                         int mask,
                         const KWin::Region &deviceRegion,
                         KWin::WindowPaintData &data)
{
    if (guestNeighborOpacity() <= 0.0 && m_cardStage->launcherGuestActive()
        && m_paintingOutput == tabletOutput() && isCardWindow(window)
        && m_cardStage->paintSlot(window) != 99) return;
    const auto settle = window == m_settlingWindow ? dropSettleRect() : bentoMotionRect(window);
    if (settle && window != m_settlingWindow && m_paintingOutput
        && window->window()->moveResizeOutput() != m_paintingOutput) return;
    if (((window == m_carriedWindow && m_carryRuntime) || settle) && m_paintingOutput) {
        const auto plan = settle
            ? carryPaintPlan(*settle, settle->topLeft(), QRectF(m_paintingOutput->geometry()))
            : carryPaintPlan(m_carryPickup, m_carryRuntime->handoff.carry().position(),
                             QRectF(m_paintingOutput->geometry()));
        if (!plan || plan->clip.isEmpty()) return;
        KWin::Rect logicalRegion = window->expandedGeometry().toRect();
        const KWin::Rect target = KWin::RectF(plan->target).toRect();
        setPositionTransformations(data, logicalRegion, window, target, Qt::KeepAspectRatioByExpanding);
        const auto deviceTarget = viewport.mapToDeviceCoordinatesAligned(target);
        const bool rounded = m_fanApertureShader && data.xScale() > 0 && data.yScale() > 0;
        m_fanApertureWindow = rounded ? window : nullptr;
        m_fanPaintSize = QSizeF(data.xScale(), data.yScale());
        m_fanApertureOrigin = QPointF((target.x() - logicalRegion.x()) * viewport.scale(),
                                      (target.y() - logicalRegion.y()) * viewport.scale());
        m_fanApertureSize = QSizeF(deviceTarget.size());
        m_fanApertureRadius = CardCornerRadius * viewport.scale();
        const auto clip = deviceRegion
            & KWin::Region(viewport.mapToDeviceCoordinatesAligned(KWin::RectF(plan->clip)))
            & (rounded ? KWin::Region(deviceTarget) : roundedClip(deviceTarget, CardCornerRadius * viewport.scale()));
        KWin::effects->paintWindow(renderTarget, viewport, window, mask | PAINT_WINDOW_TRANSFORMED, clip, data);
        m_fanApertureWindow = nullptr; m_fanPaintSize = {}; m_fanApertureOrigin = {};
        m_fanApertureSize = {}; m_fanApertureRadius = 0;
        return;
    }
    // The keyboard pans an Active card's contents, never the card: the window
    // rose, and it is drawn only from the card's own top edge down, with the
    // card's corners there. Below that edge the whole output stays paintable
    // so the card keeps its side and bottom shadow.
    if (m_paintingOutput) {
        if (const auto frame = m_cardStage->keyboardRevealFrame(window)) {
            const KWin::Rect card = viewport.mapToDeviceCoordinatesAligned(*frame);
            const KWin::Rect output = viewport.mapToDeviceCoordinatesAligned(
                m_paintingOutput->geometry());
            const int radius = static_cast<int>(std::ceil(CardCornerRadius * viewport.scale()));
            const KWin::Rect below(output.x(), card.y() + radius, output.width(),
                std::max(0, output.y() + output.height() - card.y() - radius));
            KWin::effects->paintWindow(renderTarget, viewport, window, mask,
                deviceRegion & (roundedClip(card, radius) | below), data);
            return;
        }
    }
    // A Bento pane is the desktop stage's to paint. Card Stage hides what it
    // owns, and a pane is not one of its cards, so it must not be routed here.
    if (!m_cardStage->isActive() || !m_paintingOutput || !isCardWindow(window)
        || m_desktopStage->managesWindow(window)) {
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

    const int slot = m_cardStage->paintSlot(window);
    const bool grabbedWindow = m_cardStage->cardGrabActive()
        && window == m_cardStage->selectedWindow();
    const auto route = cardPaintRoute(m_paintingOutput == tablet,
        window->screen() == tablet, slot != 99, window == m_nativeCarry,
        grabbedWindow);
    if (route == CardPaintRoute::Hidden) return;
    if (route == CardPaintRoute::Native) {
        // Only the carried item may cross the fence. Passive tablet neighbors
        // remain hidden externally; the carrier is clipped per output.
        const auto outputClip = viewport.mapToDeviceCoordinatesAligned(m_paintingOutput->geometry());
        KWin::effects->paintWindow(renderTarget, viewport, window, mask,
            window == m_nativeCarry ? deviceRegion & KWin::Region(outputClip) : deviceRegion, data);
        return;
    }

    if (m_cardStage->presentation() == CardPresentation::Active) {
        KWin::effects->paintWindow(
            renderTarget, viewport, window, mask, deviceRegion, data);
        return;
    }

    // Render the virtual seam immediately below the elevated held card. This
    // reuses our card radius/destination outline, never copies a client surface
    // or changes the real held card's contact transform.
    if (grabbedWindow && m_paintingOutput == tablet && m_destinationShader
        && m_cardStage->stackInsertionPreviewValid()) {
        paintCardSurface(m_destinationShader.get(), renderTarget, viewport,
            deviceRegion & KWin::Region(viewport.mapToDeviceCoordinatesAligned(tablet->geometry())),
            QRectF(m_cardStage->stackPlaceholderTarget()), 0.6, 0.0f,
            0.65f);
    }
    KWin::Rect logicalRegion = window->expandedGeometry().toRect();
    KWin::Rect target = m_cardStage->previewTargetForWindow(tablet, window);
    double launcherGuestRotation = 0.0;
    if (m_cardStage->launcherGuestActive()) {
        // previewTargetForWindow already blends the guest's incoming shoulders.
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
    CardStackPose paintPose = m_cardStage->stackPoseForWindow(window, target.width());
    if (grabbedWindow) {
        target = m_cardStage->cardGrabTarget();
        const auto offset = m_cardStage->cardGrabOffset();
        target.translate(qRound(offset.x()), qRound(offset.y()));
    } else {
        target.translate(qRound(paintPose.x), qRound(paintPose.y));
    }
    const double poseOpacity = m_cardStage->applyPoseTransition(window, target, paintPose);
    data.multiplyOpacity(poseOpacity);
    if (m_cardStage->launcherGuestActive() && m_paintingOutput == tablet) {
        const double opacity = guestNeighborOpacity();
        data.multiplyOpacity(opacity);
        const int direction = target.center().x() < tablet->geometry().center().x() ? -1 : 1;
        target.translate(qRound(direction * (1.0 - opacity) * target.width() * 0.25), 0);
    }
    paintPose.rotation += launcherGuestRotation;
    if (!paintPose.visible) {
        return;
    }
    KWin::Rect visualTarget = target;
    KWin::Rect projectionPaneClip;
    const bool bentoProjection =
        m_cardStage->usesBentoProjectionAperture(window);
    BentoCompositeGeometry composite;
    if (bentoProjection && m_cardStage->isBentoProjectionPane(window)) {
        const auto workspace = m_cardStage->bentoProjectionWorkspace();
        const auto storedRect = m_cardStage->bentoProjectionRect(window);
        composite = makeBentoCompositeGeometry(
            {double(target.x()), double(target.y()),
             double(target.width()), double(target.height())},
            {double(workspace.x()), double(workspace.y()),
             double(workspace.width()), double(workspace.height())});
        const auto frame = window->frameGeometry();
        const auto expanded = window->expandedGeometry();
        const auto pane = storedRect ? makeBentoProjectedPaneGeometry(composite,
            {double(workspace.x()), double(workspace.y()),
             double(workspace.width()), double(workspace.height())}, *storedRect,
            {frame.x(), frame.y(), frame.width(), frame.height()},
            {expanded.x(), expanded.y(), expanded.width(), expanded.height()})
            : std::nullopt;
        if (!pane) return;
        visualTarget = KWin::Rect(qRound(pane->targetSurface.x),
            qRound(pane->targetSurface.y), qRound(pane->targetSurface.width),
            qRound(pane->targetSurface.height));
        projectionPaneClip = KWin::Rect(qRound(pane->targetClip.x),
            qRound(pane->targetClip.y), qRound(pane->targetClip.width),
            qRound(pane->targetClip.height));
    } else if (bentoProjection) {
        return; // CARD-LIFECYCLE.md §7: a sleeping group member is owned and
                // minimized, so the group shows its panes and not this window.
    }
    const bool rotatedFanCard = !qFuzzyIsNull(paintPose.rotation);
    bool paintProjectionBackdrop = false;
    if (bentoProjection) {
        for (auto *stacked : KWin::effects->stackingOrder()) {
            if (stacked && !stacked->isDeleted() && !stacked->isMinimized()
                && m_cardStage->isBentoProjectionPane(stacked)) {
                paintProjectionBackdrop = stacked == window;
                break;
            }
        }
    }
    if (!bentoProjection || paintProjectionBackdrop) {
        const auto surface = bentoProjection
            ? QRectF(composite.targetUnion.x, composite.targetUnion.y,
                     composite.targetUnion.width, composite.targetUnion.height)
            : QRectF(visualTarget);
        const KWin::Region outputFence(
            viewport.mapToDeviceCoordinatesAligned(m_paintingOutput->geometry()));
        paintCardSurface(m_destinationShader.get(), renderTarget, viewport,
            bentoProjection ? outputFence : deviceRegion & outputFence,
            surface, paintPose.rotation, float(data.opacity()), 0.0f,
            bentoProjection ? QVector3D(0.0F, 0.0F, 0.0F)
                            : QVector3D(0.075F, 0.075F, 0.075F),
            bentoProjection ? BentoWorkspaceTintOpacity : 1.0F);
    }
    m_cardLabelTargets.insert(window, bentoProjection
        ? QRectF(composite.targetUnion.x, composite.targetUnion.y,
                 composite.targetUnion.width, composite.targetUnion.height)
        : QRectF(target));
    // Ordinary cards keep the fixed backing path. A Bento projection maps every
    // live pane from its stored Bento rect through one authoritative desktop
    // work area while the canonical slot still owns Spread layout and input.
    KWin::Effect::setPositionTransformations(
        data, logicalRegion, window, visualTarget,
        Qt::KeepAspectRatio);
    if (rotatedFanCard) {
        // Rotate each visible aperture around its own lower-right corner. The
        // canonical slot still supplies the accepted fan offset and envelope.
        const double originX =
            (visualTarget.x() + visualTarget.width() - logicalRegion.x())
            / data.xScale();
        const double originY =
            (visualTarget.y() + visualTarget.height() - logicalRegion.y())
            / data.yScale();
        data.setRotationAngle(paintPose.rotation);
        data.setRotationOrigin(QVector3D(originX, originY, 0.0));
    }

    // deviceRegion is KWin's actual renderer clip. The visual aperture can be
    // smaller than the canonical slot for a projected Bento pane, but cannot
    // alter Spread pitch, input reservation, or the output fence.
    const KWin::Rect apertureTarget = bentoProjection
        ? projectionPaneClip : visualTarget;
    const KWin::Rect deviceAperture =
        viewport.mapToDeviceCoordinatesAligned(apertureTarget);
    // QRegion remains only the hard output fence. Each visible preview uses
    // the shared offscreen aperture for one physical pixel of fractional edge
    // coverage; this is independent of client alpha and therefore treats a
    // solid Spotify surface exactly like a translucent decoration.
    // A tilted aperture must not be cut by an unrotated bottom boundary.
    // Straight and tilted previews share the same antialiased aperture.
    // Keep the hard-rounded path solely as the shader-unavailable fallback.
    // Active has already returned above and remains on the system paint path.
    const bool useFanAperture = m_fanApertureShader
        && data.xScale() > 0 && data.yScale() > 0
        && !deviceAperture.isEmpty();
    const KWin::Region outputFence(viewport.mapToDeviceCoordinatesAligned(
        m_paintingOutput->geometry()));
    const KWin::Region compositeFence = bentoProjection
        ? KWin::Region(viewport.mapToDeviceCoordinatesAligned(KWin::Rect(
            qRound(composite.targetUnion.x), qRound(composite.targetUnion.y),
            qRound(composite.targetUnion.width), qRound(composite.targetUnion.height))))
        : outputFence;
    const KWin::Region paneFence = bentoProjection
        ? KWin::Region(viewport.mapToDeviceCoordinatesAligned(projectionPaneClip))
        : outputFence;
    const double apertureRadius = bentoProjection
        ? scaleBentoCompositeRadius(composite, CardCornerRadius)
            * viewport.scale()
        : CardCornerRadius * viewport.scale();
    const KWin::Region cardClip = (rotatedFanCard && !bentoProjection
        ? deviceRegion
        : deviceRegion & (useFanAperture ? KWin::Region(deviceAperture)
            : roundedClip(deviceAperture, apertureRadius)))
        & outputFence & compositeFence & paneFence;
    m_fanApertureWindow = useFanAperture ? window : nullptr;
    m_fanPaintSize = useFanAperture
        ? QSizeF(data.xScale(), data.yScale()) : QSizeF();
    m_fanApertureOrigin = useFanAperture
        ? QPointF((apertureTarget.x() - logicalRegion.x()) * viewport.scale(),
                  (apertureTarget.y() - logicalRegion.y()) * viewport.scale())
        : QPointF();
    m_fanApertureSize = useFanAperture
        ? QSizeF(deviceAperture.size()) : QSizeF();
    m_fanApertureRadius = useFanAperture
        ? static_cast<float>(apertureRadius) : 0.0F;

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
