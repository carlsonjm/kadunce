/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Effect.h"
#include "MotionTime.h"
#include <QScopeGuard>
#include <fstream>
#include <string>
#include "LaunchIdentity.h"
#include "PendingLaunch.h"
#include "PlacementAim.h"
#include "SpreadLayout.h"
#include "BentoCompositeGeometry.h"
#include "HeldTuck.h"
#include "DisplayHandoffPolicy.h"
#include "CarryPaintPlan.h"
#include "PaneArrival.h"
#include "NativeLanding.h"
#include "MonitorDropIntent.h"
#include "DeliberateEdgeEntry.h"
#include "TouchDisplay.h"
#include <core/inputdevice.h>

#include <core/output.h>
#include <core/region.h>
#include <core/renderviewport.h>
#include <core/rendertarget.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <input.h>
#include <pointer_input.h>
#include <touch_input.h>
#include <inputmethod.h>
#include <inputpanelv1window.h>
#include <main.h>
#include <opengl/glshadermanager.h>
#include <opengl/glutils.h>
#include <opengl/glvertexbuffer.h>
#include <window.h>
#include <workspace.h>
#include <virtualdesktops.h>
#include <wayland_server.h>
#include <wayland/seat.h>
#include <wayland/surface.h>

#include <KConfigGroup>
#include <KSharedConfig>
#include <KService>
#include <KSharedConfig>

#include <QAction>
#include <QKeyEvent>
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
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <vector>

namespace Kadunce
{

namespace
{
// A custom motion's duration at Plasma's animation speed (MotionTime.h).
int motion(int base)
{
    return motionDuration(base, KWin::effects ? KWin::effects->animationTimeFactor() : 1.0);
}
}

namespace
{
constexpr auto Revision = "0.1.0-kadunce-baseline";
constexpr double CardCornerRadius = 8.0;
constexpr float BentoWorkspaceTintOpacity = 0.22F;
// A swipe up from the bottom bezel has opened Spread all the way once it has
// risen this share of the tablet's height past where it committed, about the
// four centimetres KWin gives three fingers on the tablet.
constexpr double BezelSpreadTravel = 0.22;
// A finger still rising this fast as it lifts, in logical pixels per
// millisecond, opens Spread however short the swipe.
constexpr double BezelFlickSpeed = 0.5;
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
// The shape a window's border asks the pointer for. KWin 6.8 no longer
// exposes it, and there it reads as none.
template<typename Window>
QByteArray borderCursorName(Window *window)
{
    if constexpr (requires { window->cursor().name(); }) return window->cursor().name();
    else return QByteArray();
}

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

// KWin's Scripting is not an installed class. It is a child of the
// workspace, reached through its invokable methods.
QObject *kwinScripting()
{
    for (QObject *child : KWin::workspace()->children()) {
        if (child->inherits("KWin::Scripting")) return child;
    }
    return nullptr;
}

const QString DesktopPopupScript = QStringLiteral("desktopchangeosd");
const QString OverviewEffect = QStringLiteral("overview");

// Whether Overview's own settings give it the top-left corner, as KDE's do
// unless the person changed them.
bool overviewHoldsTopLeft()
{
    return KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Effect-overview"))
        .readEntry("BorderActivate", QList<int>{KWin::ElectricTopLeft}).contains(KWin::ElectricTopLeft);
}

// Whether the person gave the top-left corner to something other than
// Overview: one of KDE's own corner actions, or another effect or script.
bool topLeftCornerIsTheirs()
{
    const KSharedConfig::Ptr kwinrc = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    if (kwinrc->group(QStringLiteral("ElectricBorders")).readEntry("TopLeft", QStringLiteral("None"))
            .compare(QLatin1String("None"), Qt::CaseInsensitive) != 0)
        return true;
    for (const QString &name : kwinrc->groupList()) {
        const KConfigGroup group = kwinrc->group(name);
        for (const QString &key : group.keyList()) {
            if (!key.contains(QLatin1String("BorderActivate")) || key.contains(QLatin1String("Touch"))) continue;
            if (name == QLatin1String("Effect-overview") && key == QLatin1String("BorderActivate")) continue;
            if (group.readEntry(key, QList<int>{}).contains(KWin::ElectricTopLeft)) return true;
        }
    }
    return false;
}

const char *tableActionName(TableAction::Kind kind)
{
    switch (kind) {
    case TableAction::Kind::None: return "nothing";
    case TableAction::Kind::StayOpen: return "the tabs stay open";
    case TableAction::Kind::Close: return "cancelled";
    case TableAction::Kind::Enter: return "entered a workspace";
    case TableAction::Kind::Activate: return "opened a card";
    case TableAction::Kind::Move: return "moved a card";
    case TableAction::Kind::Create: return "made a workspace";
    case TableAction::Kind::New: return "made an empty workspace";
    case TableAction::Kind::Rename: return "renaming a workspace";
    }
    return "nothing";
}

// KDE names a desktop it makes "Desktop N". Table treats that as no name yet:
// the tab is its number until an application names it.
bool unnamedDesktop(const QString &name)
{
    static const QRegularExpression kdeDefault(QStringLiteral("^Desktop \\d+$"));
    return name.trimmed().isEmpty() || kdeDefault.match(name).hasMatch();
}

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

// One virtual desktop's ownership session. Its host is declared first so it
// outlives the stages that call it.
struct Effect::DesktopSession {
    QString desktopId;
    QPointer<KWin::VirtualDesktop> desktop;
    std::unique_ptr<SessionHost> host;
    std::unique_ptr<DesktopStageController> layouts;
    std::unique_ptr<CardStageController> cards;
};

// Names one session's stages for the length of a call. Leaving hands back the
// session named before, or the current desktop's when that was the one named,
// since a desktop switch inside the call makes another desktop current.
class Effect::SessionScope
{
public:
    SessionScope(Effect *effect, DesktopSession *session)
        : m_effect(effect)
        , m_previous(effect->scopedToCurrent() ? nullptr : effect->m_scopedSession)
    {
        effect->scopeTo(session);
    }
    ~SessionScope()
    {
        m_effect->scopeTo(m_previous ? m_previous : m_effect->m_currentSession);
    }
    SessionScope(const SessionScope &) = delete;
    SessionScope &operator=(const SessionScope &) = delete;

private:
    Effect *m_effect;
    DesktopSession *m_previous;
};

// Every host call a stage makes is answered for that stage's own desktop, so a
// stage whose desktop is not shown never reads the current desktop's windows as
// its own. The effect keeps the answers; this names whose session asks.
class Effect::SessionHost final : public CardStageHost, public DesktopStageHost
{
public:
    SessionHost(Effect *effect, DesktopSession *session)
        : m_effect(effect)
        , m_session(session)
    {
    }

    KWin::LogicalOutput *tabletOutputForCardStage() const override
    {
        SessionScope scope(m_effect, m_session);
        return cards().tabletOutputForCardStage();
    }
    bool isTabletOutputForCardStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session);
        return cards().isTabletOutputForCardStage(output);
    }
    bool isManagedWindowForCardStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session);
        return cards().isManagedWindowForCardStage(window);
    }
    bool mayHoldWindowForCardStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session);
        return cards().mayHoldWindowForCardStage(window);
    }
    std::optional<double> inputPanelTopForCardStage(KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session);
        return cards().inputPanelTopForCardStage(output);
    }
    bool keyboardTypesIntoForCardStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session);
        return cards().keyboardTypesIntoForCardStage(window);
    }
    KWin::RectF workAreaForCardStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session);
        return cards().workAreaForCardStage(output);
    }
    void setPagingShortcutsForCardStage(bool active) override
    {
        SessionScope scope(m_effect, m_session);
        cards().setPagingShortcutsForCardStage(active);
    }
    void cancelInputForCardStage() override
    {
        SessionScope scope(m_effect, m_session);
        cards().cancelInputForCardStage();
    }
    // A card whose application asked a question as it was flicked closed
    // opens with it. The opening waits a turn of the event loop and then acts
    // only if the shown desktop's stage still has that card selected.
    void presentSelectedForCardStage() override
    {
        SessionScope scope(m_effect, m_session);
        cards().presentSelectedForCardStage();
    }
    // A card let go on a pane of the Bento group took it: the group opens as
    // its layout, on the desktop whose stage has it, the card growing there.
    void openGroupAfterDropForCardStage(const QRectF &from) override
    {
        SessionScope scope(m_effect, m_session);
        cards().openGroupAfterDropForCardStage(from);
    }
    void connectManagedWindowForCardStage(KWin::EffectWindow *window) override
    {
        SessionScope scope(m_effect, m_session);
        cards().connectManagedWindowForCardStage(window);
    }
    void unredirectForCardStage(KWin::EffectWindow *window) override
    {
        SessionScope scope(m_effect, m_session);
        cards().unredirectForCardStage(window);
    }
    void retireBentoProjectionForCardStage(const QList<QPointer<KWin::EffectWindow>> &windows) override
    {
        SessionScope scope(m_effect, m_session);
        cards().retireBentoProjectionForCardStage(windows);
    }
    bool admitCardToDesktopStage(KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const KWin::RectF &geometry, const std::function<bool()> &commitSource,
        const std::function<void()> &releaseSource) override
    {
        SessionScope scope(m_effect, m_session);
        return cards().admitCardToDesktopStage(window, output, geometry, commitSource, releaseSource);
    }
    bool resumeBentoProjectionForCardStage(const BentoProjectionSession &projection,
        const std::function<bool()> &commitSource,
        const std::function<void()> &releaseSource) override
    {
        SessionScope scope(m_effect, m_session);
        return cards().resumeBentoProjectionForCardStage(projection, commitSource, releaseSource);
    }

    bool isTabletOutputForDesktopStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().isTabletOutputForDesktopStage(output);
    }
    bool usesDrawnZonesForDesktopStage() const override
    {
        return layouts().usesDrawnZonesForDesktopStage();
    }
    bool allowsDesktopStageOnOutput(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().allowsDesktopStageOnOutput(output);
    }
    bool isManagedWindowForDesktopStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().isManagedWindowForDesktopStage(window);
    }
    KWin::LogicalOutput *tabletOutputForDesktopStage() const override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().tabletOutputForDesktopStage();
    }
    bool outputCanOwnCards(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().outputCanOwnCards(output);
    }
    KWin::Rect activeTargetForDesktopStage(KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().activeTargetForDesktopStage(output);
    }
    void prepareOutputForDesktopStage(KWin::LogicalOutput *output) override
    {
        SessionScope scope(m_effect, m_session);
        layouts().prepareOutputForDesktopStage(output);
    }
    void retireOutputFromDesktopStage(KWin::LogicalOutput *output) override
    {
        SessionScope scope(m_effect, m_session);
        layouts().retireOutputFromDesktopStage(output);
    }
    QRectF bentoPresentationRect(KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().bentoPresentationRect(window);
    }
    void animateBentoLayout(KWin::LogicalOutput *output, const QList<QPointer<KWin::EffectWindow>> &windows,
        const QList<QRectF> &from, const QList<QRectF> &to) override
    {
        SessionScope scope(m_effect, m_session);
        layouts().animateBentoLayout(output, windows, from, to);
    }
    std::optional<NativeMoveSnapshot> activeRestoreForDesktopStage(KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().activeRestoreForDesktopStage(window);
    }
    bool admitDisplacedPaneToTablet(KWin::EffectWindow *window, const std::function<bool()> &commitSource,
        const NativeMoveSnapshot *restore = nullptr) override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().admitDisplacedPaneToTablet(window, commitSource, restore);
    }
    bool admitSleepingPaneToTablet(KWin::EffectWindow *window, const std::function<bool()> &commitSource,
        const NativeMoveSnapshot *restore = nullptr) override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().admitSleepingPaneToTablet(window, commitSource, restore);
    }
    bool admitTransferredWindowToTablet(KWin::EffectWindow *window, const std::function<bool()> &commitSource,
        const NativeMoveSnapshot *restore = nullptr) override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().admitTransferredWindowToTablet(window, commitSource, restore);
    }

private:
    CardStageHost &cards() const { return *m_effect; }
    DesktopStageHost &layouts() const { return *m_effect; }

    Effect *m_effect;
    DesktopSession *m_session;
};

template<typename F>
void Effect::forEachSession(F &&f)
{
    std::vector<DesktopSession *> sessions;
    sessions.reserve(m_sessions.size());
    for (const auto &entry : m_sessions) sessions.push_back(entry.second.get());
    for (DesktopSession *session : sessions) {
        SessionScope scope(this, session);
        f();
    }
}

Effect::DesktopSession *Effect::sessionFor(KWin::VirtualDesktop *desktop)
{
    const QString id = desktop ? desktop->id() : QString();
    auto &slot = m_sessions[id];
    if (!slot) {
        slot = std::make_unique<DesktopSession>();
        slot->desktopId = id;
        slot->desktop = desktop;
        slot->host = std::make_unique<SessionHost>(this, slot.get());
        slot->cards = std::make_unique<CardStageController>(
            static_cast<CardStageHost *>(slot->host.get()));
        slot->layouts = std::make_unique<DesktopStageController>(
            static_cast<DesktopStageHost *>(slot->host.get()));
    }
    return slot.get();
}

Effect::DesktopSession *Effect::sessionHolding(const KWin::EffectWindow *window) const
{
    if (!window) return nullptr;
    auto *held = const_cast<KWin::EffectWindow *>(window);
    for (const auto &entry : m_sessions) {
        if (entry.second->cards->liveCardIndex(window) >= 0
            || entry.second->layouts->managesWindow(held)) return entry.second.get();
    }
    return nullptr;
}

void Effect::scopeTo(DesktopSession *session)
{
    m_scopedSession = session;
    m_cardStage = session ? session->cards.get() : nullptr;
    m_desktopStage = session ? session->layouts.get() : nullptr;
}


Effect::Effect()
{
    connect(KWin::effects, &KWin::EffectsHandler::windowDeleted, this,
        [this](KWin::EffectWindow *window) {
            m_previewSourceBounds.remove(window);
            m_previewRefs.remove(window);
        });
    connect(KWin::effects, &KWin::EffectsHandler::windowClosed, this,
        [this](KWin::EffectWindow *window) {
            if (window == m_leavingPanel) releaseLeavingKeys();
        });
    m_leavingKeysLimit.setSingleShot(true);
    m_leavingKeysLimit.setInterval(600);
    connect(&m_leavingKeysLimit, &QTimer::timeout, this, &Effect::releaseLeavingKeys);
    m_currentSession = sessionFor(KWin::effects->currentDesktop());
    scopeTo(m_currentSession);
    loadNamedDesktops();
    m_usesDrawnZones = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Bento"))
        .readEntry(QStringLiteral("UseDrawnZones"), false);
    // A menu bar's tab held still becomes a rename.
    m_tableHold.setSingleShot(true);
    m_tableHold.setTimerType(Qt::PreciseTimer);
    connect(&m_tableHold, &QTimer::timeout, this, [this] {
        const TableAction action = m_table.hold(tableNow());
        if (action.kind == TableAction::Kind::Rename) beginTableRename(action.workspace, m_tablePressTouch);
        else if (const qint64 left = m_table.holdLeft(tableNow()); left > 0) m_tableHold.start(int(left));
    });
    refreshCardOutput();
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

    // Kadunce's keys sit on Meta and are taken only while it runs (INPUT.md
    // § Keyboard shortcuts), so Ctrl stays with applications and nothing is
    // written to the person's shortcut settings.
    m_keyRoute = std::make_unique<KeyRoute>(
        [this](int key, Qt::KeyboardModifiers modifiers, bool repeat) {
            return answerKey(key, modifiers, repeat);
        });

    // Three fingers down on the touchscreen, or four on a touchpad, open Spread
    // under the fingers. KDE's Overview, which claims the same moves, is set
    // aside while Kadunce runs.
    m_spreadGestureAction = new QAction(this);
    const auto followSpread = [this](qreal progress) { followSpreadGesture(progress); };
    KWin::effects->registerTouchscreenSwipeShortcut(
        KWin::SwipeDirection::Down, 3, m_spreadGestureAction, followSpread);
    KWin::effects->registerTouchpadSwipeShortcut(
        KWin::SwipeDirection::Down, 4, m_spreadGestureAction, followSpread);
    connect(m_spreadGestureAction, &QAction::triggered,
            this, &Effect::finishSpreadGesture);
    QMetaObject::invokeMethod(this, &Effect::holdOverviewOff, Qt::QueuedConnection);
    connect(KWin::workspace(), &KWin::Workspace::configChanged,
            this, &Effect::holdOverviewOff);

    // Three fingers up on the touchscreen, or four on a touchpad, bring Table
    // down as its menu bar once past halfway: on the tablet from its screen,
    // under the pointer from a touchpad. Its key is Meta+W (KadunceKeys.h).
    m_tableGestureAction = new QAction(this);
    KWin::effects->registerTouchscreenSwipeShortcut(
        KWin::SwipeDirection::Up, 3, m_tableGestureAction,
        [this](qreal progress) { followTableGesture(progress, tabletOutput()); });
    m_tablePadGestureAction = new QAction(this);
    KWin::effects->registerTouchpadSwipeShortcut(
        KWin::SwipeDirection::Up, 4, m_tablePadGestureAction, [this](qreal progress) {
            followTableGesture(progress, KWin::effects->screenAt(KWin::effects->cursorPos().toPoint()));
        });
    for (QAction *action : {m_tableGestureAction, m_tablePadGestureAction})
        connect(action, &QAction::triggered, this, [this]() { m_tableGestureDone = false; });
    // A pointer pushed into a display's top-left corner:
    // along the whole top edge it opened whenever a window's top was reached.
    // Overview gives its corner up again whenever it loads or reconfigures,
    // and KWin's loader, which says when it loads one, is not exported.
    holdTableCorner();
    connect(KWin::workspace(), &KWin::Workspace::configChanged, this, &Effect::holdTableCorner);
    connect(KWin::options, &KWin::Options::animationSpeedChanged, this, &Effect::holdTableCorner);
    for (QObject *child : KWin::effects->children()) {
        if (child->inherits("KWin::EffectLoader"))
            connect(child, SIGNAL(effectLoaded(KWin::Effect*,QString)), this, SLOT(holdTableCorner()));
    }

    // The Z13 tablet kit selects Kadunce's richer direct four-edge router.
    // Every other touchscreen delegates top and bottom gesture recognition to
    // Plasma/KWin and only receives the resulting semantic action here. The
    // router is told which backend owns those edges so one swipe cannot be
    // consumed or toggled twice.
    m_usesDirectSystemEdges = z13TabletKitAvailable();
    if (!m_usesDirectSystemEdges) {
        // Plasma's bottom touch edge opens Spread, as the bezel does on the
        // tablet, and so do three fingers on any touchscreen.
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
    connect(KWin::effects, &KWin::EffectsHandler::desktopChanged, this,
            [this](KWin::VirtualDesktop *previous, KWin::VirtualDesktop *current) {
                handleDesktopChanged(previous, current);
            });
    connect(KWin::effects, &KWin::EffectsHandler::desktopRemoved,
            this, &Effect::handleDesktopRemoved);
    connect(KWin::effects, &KWin::EffectsHandler::sessionStateChanged,
            this, &Effect::handleSessionStateChanged);
    connect(KWin::effects, &KWin::EffectsHandler::screenRemoved,
            this, &Effect::handleScreenRemoved);
    connect(KWin::effects, &KWin::EffectsHandler::screenAdded, this,
            [this](KWin::LogicalOutput *output) {
                forEachSession([&] { m_desktopStage->handleScreenAdded(output); });
                refreshCardOutput();
                scheduleCardDisplaySettle();
            });
    // A display that moves, as the tablet does when a monitor is arranged
    // beside it, is a layout KWin may put windows back into as well.
    connect(KWin::effects, &KWin::EffectsHandler::virtualScreenGeometryChanged,
            this, &Effect::scheduleCardDisplaySettle);
    // A panel took its room, gave it up or took it back. KWin announces the
    // rearrangement before making it, so the card is placed on the next turn.
    m_keysWorkAreaRelease.setSingleShot(true);
    m_keysWorkAreaRelease.setInterval(1000);
    connect(&m_keysWorkAreaRelease, &QTimer::timeout, this, &Effect::releaseKeysWorkArea);
    // A card let go lands, and settles, within this; its dialogs follow it
    // there and are then left where they stand.
    m_carriedDialogsRelease.setSingleShot(true);
    m_carriedDialogsRelease.setInterval(1000);
    connect(&m_carriedDialogsRelease, &QTimer::timeout, this, &Effect::releaseCarriedDialogs);
    connect(KWin::workspace(), &KWin::Workspace::aboutToRearrange, this, [this]() {
        if (m_workAreaCheckQueued) return;
        m_workAreaCheckQueued = true;
        QTimer::singleShot(0, this, [this]() {
            m_workAreaCheckQueued = false;
            followKeysWorkArea();
            m_cardStage->followWorkArea();
        });
    });
    // Displays that came or went since the keys came up are not the ones held.
    connect(KWin::effects, &KWin::EffectsHandler::virtualScreenGeometryChanged,
            this, [this]() { m_keysWorkAreas.clear(); followKeysWorkArea(); });
    // A touchscreen plugged in or out can move which display holds cards. The
    // turn lets KWin place a new one on its display first.
    if (KWin::input()) {
        const auto deferRefresh = [this](KWin::InputDevice *device) {
            if (device && device->isTouch())
                QTimer::singleShot(0, this, &Effect::refreshCardOutput);
        };
        connect(KWin::input(), &KWin::InputRedirection::deviceAdded, this, deferRefresh);
        connect(KWin::input(), &KWin::InputRedirection::deviceRemoved, this, deferRefresh);
    }
    // KeyboardOverlayPolicy stops KWin lifting the focused window for the
    // keyboard, so the card stage alone decides what a card does about it: the
    // Active card gives up the room the keys take while they type into it. It
    // hears every change to that room: the keyboard showing, hiding or
    // changing height, and text focus moving to another window.
    const auto watchInputPanel = [this]() {
        disconnect(m_inputPanelGeometry);
        // KWin announces a panel before the panel has a window an effect can
        // see, so a keyboard changing height is heard from the panel itself.
        KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
        if (KWin::Window *panel = method ? method->panel() : nullptr) {
            m_inputPanelGeometry = connect(panel,
                &KWin::Window::frameGeometryChanged, this,
                [this, panel](const KWin::RectF &old) {
                    m_cardStage->repaintKeyboardEdge(old, panel->frameGeometry());
                    m_cardStage->refreshKeyboardRoom();
                });
        }
        m_cardStage->refreshKeyboardRoom();
    };
    connect(KWin::effects, &KWin::EffectsHandler::inputPanelChanged, this,
            watchInputPanel);
    watchInputPanel();
    if (KWin::InputMethod *method = KWin::kwinApp()->inputMethod()) {
        // Before the room is made: a keyboard put back down takes no room.
        connect(method, &KWin::InputMethod::visibleChanged, this,
                &Effect::keepUnaskedKeyboardDown);
        // After the decision above: keys it put back down lend nothing, and
        // keys it keeps drawing on their way out still do.
        connect(method, &KWin::InputMethod::visibleChanged, this,
                &Effect::followKeysWorkArea);
        connect(method, &KWin::InputMethod::visibleChanged, this,
                [this]() { m_cardStage->keepKeyboardRoomPlacement(); });
        for (const auto signal : {&KWin::InputMethod::visibleChanged,
                                  &KWin::InputMethod::activeWindowChanged}) {
            connect(method, signal, this,
                    [this]() { m_cardStage->refreshKeyboardRoom(); });
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
        takeCarriedDialogs(m_carriedWindow);
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
        takeCarriedDialogs(m_nativeCarry);
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
    // A workspace spans every display, so switching stays whole, and KDE's
    // desktop-name pop-up stays off while this runs.
    m_perOutputDesktopsPolicy =
        std::make_unique<PerOutputDesktopsPolicy<KWin::Options>>(KWin::options);
    connect(KWin::options, &KWin::Options::configChanged, this, [this] {
        if (m_nativeEdgePolicy) m_nativeEdgePolicy->refresh();
        if (m_keyboardOverlayPolicy) m_keyboardOverlayPolicy->refresh();
        if (m_perOutputDesktopsPolicy) m_perOutputDesktopsPolicy->refresh();
    });
    // KWin loads scripts when the workspace is ready and again on every
    // reconfigure; these run after it has, being connected after it.
    holdDesktopPopupOff();
    connect(KWin::workspace(), &KWin::Workspace::workspaceInitialized, this, &Effect::holdDesktopPopupOff);
    connect(KWin::workspace(), &KWin::Workspace::configChanged, this, &Effect::holdDesktopPopupOff);

    qInfo() << "Kadunce" << Revision
            << (m_usesDirectSystemEdges
                    ? "direct Z13 system edges"
                    : "Plasma-native system edges")
            << "and output-local Bento ready; fan aperture"
            << (m_fanApertureShader ? "enabled" : "r20 fallback");
    // Switching Kadunce on puts the display that can own cards in cards. The
    // turn lets KWin finish loading the effect before the first placement.
    QTimer::singleShot(0, this, [this] { (void)startTabletInCards(nullptr); });
}

bool Effect::startTabletInCards(KWin::EffectWindow *arrival)
{
    // CARD-LIFECYCLE.md §3: while Kadunce is on, the display that can own
    // cards holds its windows as cards, the one in use Active. This runs when
    // Kadunce is switched on and when a window opens on that display while it
    // holds none. It is the same adoption a first edge action performs.
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet || m_cardStage->isActive() || m_carriedWindow
        || m_desktopStage->hasSessionOnOutput(tablet->name())) return false;
    const auto eligible = [this, tablet](KWin::EffectWindow *window) {
        return window && !window->isDeleted() && isCardWindow(window)
            && window->screen() == tablet && window->window()
            && window->window()->readyForPainting()
            && !window->isUserMove() && !window->isUserResize();
    };
    KWin::EffectWindow *active = eligible(arrival) ? arrival : nullptr;
    if (!active && eligible(KWin::effects->activeWindow()))
        active = KWin::effects->activeWindow();
    if (!active) {
        const auto stack = KWin::effects->stackingOrder();
        for (auto it = stack.crbegin(); it != stack.crend() && !active; ++it)
            if (eligible(*it)) active = *it;
    }
    // A stack Table carried here comes back with its face in front.
    if (active && KWin::effects->currentDesktop()) {
        for (const auto &deck : m_pendingDecks.value(KWin::effects->currentDesktop()->id()))
            if (deck.contains(active) && deck.first() && eligible(deck.first())) active = deck.first();
    }
    if (!active || !m_cardStage->adoptDisplayWithActive(active, [] { return true; }))
        return false;
    restackCarriedDecks();
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
    return true;
}

Effect::~Effect()
{
    m_releasing = true;
    if (m_gapHeld) KWin::effects->stopMouseInterception(this);
    m_table.close();
    if (m_tablePresenter) m_tablePresenter->hide();
    if (m_tableKeyboard) KWin::effects->ungrabKeyboard();
    giveTableCornerBack();
    endDesktopPreview();
    if (m_nativeCarry) KWin::effects->setElevatedWindow(m_nativeCarry, false);
    forEachSession([this] { m_desktopStage->cancelRestoredMinimizations(); });
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
    if (m_overviewHeld) {
        // Overview comes back as Kadunce stops, unless the person turned it
        // off. It loads after this effect has gone.
        QTimer::singleShot(0, KWin::effects, []() {
            const bool wanted = KSharedConfig::openConfig(QStringLiteral("kwinrc"))
                ->group(QStringLiteral("Plugins")).readEntry("overviewEnabled", true);
            if (wanted && !KWin::effects->isEffectLoaded(OverviewEffect))
                KWin::effects->loadEffect(OverviewEffect);
        });
    }
    // Every desktop's windows go back, not only the shown desktop's (§13).
    bool owning = false;
    forEachSession([&] { owning = owning || m_cardStage->isActive() || hasActiveDesktopStage(); });
    if (owning) release();
    returnDependentWindows();
    m_nativeEdgePolicy.reset();
    if (m_desktopPopupHeld) {
        if (QObject *scripting = kwinScripting())
            QMetaObject::invokeMethod(scripting, "start", Qt::DirectConnection);
    }
}

void Effect::holdDesktopPopupOff()
{
    QObject *scripting = kwinScripting();
    bool loaded = false;
    if (!scripting
        || !QMetaObject::invokeMethod(scripting, "isScriptLoaded", Qt::DirectConnection,
                                      Q_RETURN_ARG(bool, loaded), Q_ARG(QString, DesktopPopupScript))
        || !loaded) return;
    bool unloaded = false;
    QMetaObject::invokeMethod(scripting, "unloadScript", Qt::DirectConnection,
                              Q_RETURN_ARG(bool, unloaded), Q_ARG(QString, DesktopPopupScript));
    m_desktopPopupHeld = m_desktopPopupHeld || unloaded;
}

void Effect::holdTableCorner()
{
    KSharedConfig::openConfig(QStringLiteral("kwinrc"))->reparseConfiguration();
    if (topLeftCornerIsTheirs()) {
        giveTableCornerBack();
        return;
    }
    if (!m_tableCornerReserved) KWin::effects->reserveElectricBorder(KWin::ElectricTopLeft, this);
    m_tableCornerReserved = true;
    KWin::Effect *overview = KWin::effects->findEffect(OverviewEffect);
    if (overview && overviewHoldsTopLeft()) {
        KWin::effects->unreserveElectricBorder(KWin::ElectricTopLeft, overview);
        m_overviewCornerHeld = true;
    }
}

void Effect::giveTableCornerBack()
{
    if (m_tableCornerReserved) KWin::effects->unreserveElectricBorder(KWin::ElectricTopLeft, this);
    m_tableCornerReserved = false;
    if (!m_overviewCornerHeld) return;
    m_overviewCornerHeld = false;
    // During KWin's own shutdown the effects are already out of its list, so
    // there is no Overview to give it to.
    KWin::Effect *overview = KWin::effects->findEffect(OverviewEffect);
    if (!overview || !overviewHoldsTopLeft()) return;
    // KWin counts reservations, so a corner Overview took back itself is
    // released before it is given, never counted twice.
    KWin::effects->unreserveElectricBorder(KWin::ElectricTopLeft, overview);
    KWin::effects->reserveElectricBorder(KWin::ElectricTopLeft, overview);
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

bool Effect::isTabletOutput(const KWin::LogicalOutput *output) const
{
    return output && !m_cardOutputName.isEmpty() && output->name() == m_cardOutputName;
}

void Effect::refreshCardOutput()
{
    QList<TouchDisplayOutput> outputs;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (output)
            outputs.append({output->name(), output->isInternal(), QSizeF(output->physicalSize())});
    }
    QList<TouchDisplayDevice> devices;
    if (KWin::input()) {
        for (KWin::InputDevice *device : KWin::input()->devices()) {
            // Only a kernel touchscreen drives a display. Fake and virtual
            // input deliver contacts in global coordinates and belong to none.
            if (!device || !device->isTouch() || !device->isEnabled()
                || device->sysPath().isEmpty()) continue;
            devices.append({device->outputName(), device->property("size").toSizeF()});
        }
    }
    const QString name = cardOutputName(outputs, devices);
    if (name == m_cardOutputName) return;
    // Cards cannot follow a touchscreen to another display. They return to
    // the desktop, and the display that now holds cards starts its own.
    if (m_cardStage->isActive()) {
        cancelInputForCardStage();
        m_cardStage->release();
    }
    m_cardOutputName = name;
    qInfo() << "Kadunce" << Revision << "cards belong to"
            << (name.isEmpty() ? QStringLiteral("no display: no touchscreen drives one") : name);
    Q_EMIT workspaceContextChanged();
}

bool Effect::isCardWindow(const KWin::EffectWindow *window) const
{
    // A card held aside while the desktop shows is hidden by Card Stage, and
    // is still a card.
    return isApplicationWindow(window)
        && !window->isMinimized()
        && (!window->isHidden() || window->data(CardAsideRole).toBool());
}

bool Effect::isApplicationWindow(const KWin::EffectWindow *window) const
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
    // A dialog that names its application follows that application's card
    // (§4), including one that names it only after it is shown
    // (handleTransientChanged). One that names none is a window of its own.
    // A session holds the windows of its own desktop, whichever is shown. A
    // window on every desktop belongs to none of them, and floats over cards
    // as a panel does. A window excluded from normal task switching is no card
    // either (§4).
    const bool onSessionDesktop = m_scopedSession && !m_scopedSession->desktopId.isEmpty()
        ? m_scopedSession->desktop && window->isOnDesktop(m_scopedSession->desktop)
        : window->isOnCurrentDesktop();
    return onSessionDesktop && !window->isOnAllDesktops()
        && window->isOnCurrentActivity()
        && window->isNormalWindow() && !window->isSkipSwitcher()
        && !dependentLead(window);
}

KWin::EffectWindow *Effect::dependentLead(const KWin::EffectWindow *window)
{
    if (!window || window->isDeleted() || !window->window()) return nullptr;
    auto *client = window->window();
    KWin::Window *lead = client;
    for (int depth = 0; depth < 16 && lead->transientFor(); ++depth) lead = lead->transientFor();
    if (lead == client) {
        // An X11 group transient names its group rather than one window.
        if (!client->isTransient()) return nullptr;
        const auto mains = client->allMainWindows();
        if (mains.isEmpty()) return nullptr;
        lead = mains.constFirst();
    }
    auto *effectLead = lead ? lead->effectWindow() : nullptr;
    return effectLead && effectLead != window && !effectLead->isDeleted() ? effectLead : nullptr;
}

void Effect::handleDesktopChanged(KWin::VirtualDesktop *previous, KWin::VirtualDesktop *current)
{
    if (previous == current) return;
    // Table's own switches close it first; any other ends it.
    if (m_table.isOpen()) closeTable();
    // A switch made some other way ends a preview of a different desktop. A
    // commit keeps it, and the screen, until the switch has been announced.
    if (m_previewDesktop && current != m_previewDesktop) endDesktopPreview();
    // Nothing started on the desktop being left may still be in hand. Its
    // session keeps its cards and layouts exactly as they are.
    cancelInputForCardStage();
    m_currentSession = sessionFor(current);
    scopeTo(m_currentSession);
    setPagingShortcutsActive(m_cardStage->isActive());
    // Coming back finds the cards presented, not a Spread left open. It cannot
    // close on the way out: its cards are not on the desktop being shown.
    if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread)
        toggleOwnedPresentation();
    if (m_cardStage->isActive()) {
        // Windows that opened or arrived here while it was not shown become
        // this desktop's cards, as windows arriving on the display do.
        scheduleCardDisplaySettle();
    } else {
        // §3 for a desktop with no cards yet: its windows become cards, the
        // one in use Active, as when Kadunce is switched on. The turn lets
        // KWin finish activating a window on the desktop it switched to. A
        // desktop presenting its layout starts no cards; what arrived there
        // while it was not shown waits behind the layout instead.
        QTimer::singleShot(0, this, [this] {
            if (!startTabletInCards(nullptr)) scheduleCardDisplaySettle();
        });
    }
    scheduleDependentSync();
    observeCardOwnership();
    // The desktop just left may now be an empty, unnamed workspace.
    scheduleDissolve();
    Q_EMIT workspaceContextChanged();
}

void Effect::handleDesktopRemoved(KWin::VirtualDesktop *desktop)
{
    if (m_table.isOpen()) closeTable();
    if (desktop && m_namedDesktops.removeAll(desktop->id()) > 0) saveNamedDesktops();
    // KWin has already moved the desktop's windows to a neighbour. Its session
    // lets them go as a release would, and the desktop they stand on now takes
    // them as its own.
    const QString id = desktop ? desktop->id() : QString();
    auto removed = m_sessions.find(id);
    if (removed == m_sessions.end() || removed->second.get() == m_currentSession) return;
    {
        SessionScope scope(this, removed->second.get());
        if (hasActiveDesktopStage()) {
            m_desktopStage->stopPendingSettle();
            m_desktopStage->restoreAllSessions();
        }
        m_cardStage->release();
    }
    m_sessions.erase(removed);
    scheduleCardDisplaySettle();
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
}

void Effect::handleWindowDesktopsChanged(KWin::EffectWindow *window)
{
    if (!window || window->isDeleted()) return;
    DesktopSession *holder = sessionHolding(window);
    if (holder && holder->desktop
        && (!window->isOnDesktop(holder->desktop) || window->isOnAllDesktops())) {
        SessionScope scope(this, holder);
        if (m_cardStage->liveCardIndex(window) >= 0) (void)m_cardStage->releaseCard(window);
        else m_desktopStage->releaseWindow(window);
    }
    if (window->isOnCurrentDesktop()) scheduleCardDisplaySettle();
    syncDesktopPreview();
    scheduleDissolve();
    scheduleDependentSync();
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
}

bool Effect::hiddenOnOtherDesktop(const KWin::EffectWindow *window) const
{
    if (!window || window->isOnCurrentDesktop()) return false;
    const DesktopSession *holder = sessionHolding(window);
    // A Spread left open there comes back as its Active card, so that card is
    // the one shown; a desktop presenting its layout shows only its panes.
    return holder && holder != m_currentSession && holder->cards->liveCardIndex(window) >= 0
        && !(holder->cards->presentation() != CardPresentation::Bento
             && holder->cards->selectedWindow() == window);
}

bool Effect::hiddenByDesktopPreview(const KWin::EffectWindow *window) const
{
    if (!m_previewDesktop || !window || window->isDeleted()) return false;
    if (window->isDock() || window->isDesktop() || window->isOnAllDesktops()
        || window == KWin::effects->inputPanel()) return false;
    if (!window->isOnDesktop(m_previewDesktop)) return true;
    // A desktop's cards show as it presents them: the card Table's finger is
    // on, or else its front card. Its panes and plain windows show as they are.
    const DesktopSession *holder = sessionHolding(window);
    if (!holder || holder->cards->liveCardIndex(window) < 0) return false;
    if (m_previewCard && holder->cards->liveCardIndex(m_previewCard) >= 0) return window != m_previewCard;
    return !(holder->cards->presentation() != CardPresentation::Bento
             && holder->cards->selectedWindow() == window);
}

void Effect::syncDesktopPreview()
{
    if (!m_previewDesktop) return;
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        const bool held = !window->isDeleted() && window->isOnDesktop(m_previewDesktop)
            && !window->isOnCurrentDesktop();
        if (held && !m_previewRefs.contains(window)) {
            m_previewRefs.insert(window,
                KWin::EffectWindowVisibleRef(window, KWin::EffectWindow::PAINT_DISABLED_BY_DESKTOP));
        } else if (!held) {
            m_previewRefs.remove(window);
        }
    }
}

void Effect::endDesktopPreview()
{
    if (!m_previewDesktop) return;
    m_previewDesktop.clear();
    m_previewCard.clear();
    m_previewRefs.clear();
    if (KWin::effects->activeFullScreenEffect() == this)
        KWin::effects->setActiveFullScreenEffect(nullptr);
    KWin::effects->addRepaintFull();
}

bool Effect::previewDesktop(const QString &desktopId)
{
    KWin::VirtualDesktop *desktop = desktopId.isEmpty()
        ? nullptr : KWin::VirtualDesktopManager::self()->desktopForId(desktopId);
    if (!desktop || desktop == KWin::effects->currentDesktop()) {
        endDesktopPreview();
        return desktopId.isEmpty() || desktop;
    }
    // Another effect holding the screen, such as Overview, keeps it.
    if (KWin::effects->hasActiveFullScreenEffect()
        && KWin::effects->activeFullScreenEffect() != this) return false;
    showDesktopPreview(desktop, nullptr);
    return true;
}

void Effect::showDesktopPreview(KWin::VirtualDesktop *desktop, KWin::EffectWindow *card)
{
    if (!desktop) {
        endDesktopPreview();
        return;
    }
    if (m_previewDesktop == desktop && m_previewCard == card) return;
    if (KWin::effects->hasActiveFullScreenEffect()
        && KWin::effects->activeFullScreenEffect() != this) return;
    // Holding the screen is what makes KWin's switching effects stand aside
    // when the preview is committed (slide.cpp's desktopChanged).
    KWin::effects->setActiveFullScreenEffect(this);
    m_previewDesktop = desktop;
    m_previewCard = card;
    syncDesktopPreview();
    KWin::effects->addRepaintFull();
}

bool Effect::commitDesktopPreview()
{
    if (!m_previewDesktop) return false;
    KWin::effects->setCurrentDesktop(m_previewDesktop);
    endDesktopPreview();
    return true;
}

KWin::LogicalOutput *Effect::tableOutput() const
{
    return m_tableOutput ? m_tableOutput.data() : tabletOutput();
}

QRectF Effect::tableDisplay() const
{
    KWin::LogicalOutput *output = tableOutput();
    return output ? QRectF(output->geometry()) : QRectF();
}

qint64 Effect::tableNow() const
{
    return m_tableTime.isValid() ? m_tableTime.elapsed() : 0;
}

QList<Effect::TableWorkspace> Effect::tableWorkspaces()
{
    const auto stack = KWin::effects->stackingOrder();
    // What keeps a workspace and hangs in its row: an application's own
    // window, not a dialog, not one on every desktop.
    const auto application = [](KWin::EffectWindow *window) {
        return window && !window->isDeleted() && window->isNormalWindow()
            && !window->isOnAllDesktops() && !window->isSkipSwitcher() && !dependentLead(window);
    };
    const auto cardOf = [this](KWin::EffectWindow *window) {
        TableCard card;
        card.windows.append(window);
        card.application = applicationDisplayName(window);
        card.title = window->caption();
        card.icon = window->icon();
        return card;
    };
    QList<TableWorkspace> workspaces;
    for (KWin::VirtualDesktop *desktop : KWin::effects->desktops()) {
        TableWorkspace workspace;
        workspace.desktop = desktop;
        workspace.name = desktop->name();
        workspace.named = m_namedDesktops.contains(desktop->id());
        workspace.current = desktop == KWin::effects->currentDesktop();
        QSet<KWin::EffectWindow *> taken;
        const auto found = m_sessions.find(desktop->id());
        if (found != m_sessions.end()) {
            const DesktopSession *session = found->second.get();
            // Its front card first, then the rest in Spread's order.
            KWin::EffectWindow *front = session->cards->isActive() ? session->cards->selectedWindow() : nullptr;
            QList<KWin::EffectWindow *> ordered;
            if (front) ordered.append(front);
            for (const auto &window : session->cards->liveCards())
                if (window && window != front) ordered.append(window);
            for (KWin::EffectWindow *window : std::as_const(ordered)) {
                if (!window || window->isDeleted() || taken.contains(window)) continue;
                // A stack is one card, its face in front.
                const QList<KWin::EffectWindow *> deck = session->cards->stackFrontToBack(window);
                TableCard card = cardOf(deck.isEmpty() ? window : deck.first());
                for (KWin::EffectWindow *member : deck)
                    if (member && !member->isDeleted() && member != card.windows.first()) card.windows.append(member);
                card.stacked = int(card.windows.size()) - 1;
                for (const auto &member : std::as_const(card.windows)) taken.insert(member);
                workspace.cards.append(card);
            }
            // A layout's panes are cards of their own.
            for (const auto &view : session->layouts->ownershipView()) {
                for (const quintptr pane : view.panes) {
                    auto *window = reinterpret_cast<KWin::EffectWindow *>(pane);
                    if (!window || window->isDeleted() || taken.contains(window)) continue;
                    workspace.cards.append(cardOf(window));
                    taken.insert(window);
                }
            }
        }
        // A stack carried here whose windows are not cards yet is one card.
        const auto pending = m_pendingDecks.value(desktop->id());
        for (auto it = stack.crbegin(); it != stack.crend(); ++it) {
            KWin::EffectWindow *window = *it;
            if (!application(window) || taken.contains(window) || !window->isOnDesktop(desktop)) continue;
            QList<KWin::EffectWindow *> deck;
            for (const auto &carried : pending) {
                if (!carried.contains(window)) continue;
                for (const auto &member : carried)
                    if (member && application(member) && !taken.contains(member) && member->isOnDesktop(desktop))
                        deck.append(member);
            }
            TableCard card = cardOf(deck.isEmpty() ? window : deck.first());
            for (KWin::EffectWindow *member : std::as_const(deck))
                if (member != card.windows.first()) card.windows.append(member);
            card.stacked = int(card.windows.size()) - 1;
            for (const auto &member : std::as_const(card.windows)) taken.insert(member);
            workspace.cards.append(card);
        }
        // A workspace nothing has named takes the name of the application it
        // holds first, as one made by carrying a card to + does.
        if (unnamedDesktop(workspace.name) && !workspace.cards.isEmpty()) {
            const QString name = workspace.cards.first().application;
            if (!name.isEmpty()) {
                desktop->setName(name);
                KWin::VirtualDesktopManager::self()->save();
                workspace.name = name;
            }
        }
        if (unnamedDesktop(workspace.name)) workspace.name.clear();
        workspaces.append(workspace);
    }
    return workspaces;
}

void Effect::relayoutTable()
{
    QList<TableTabContent> tabs;
    QList<QList<TableCardContent>> cards;
    for (const auto &workspace : std::as_const(m_tableWorkspaces)) {
        const bool editing = m_tableRenaming == tabs.size();
        tabs.append({editing ? m_tableRenameText : workspace.name, workspace.current,
            int(std::min<qsizetype>(workspace.cards.size(), 4)), int(tabs.size()) + 1, editing});
        QList<TableCardContent> own;
        for (const auto &card : workspace.cards) own.append({card.application, card.title, card.stacked});
        cards.append(own);
    }
    // KDE holds at most 25 desktops; at that, + goes.
    auto *manager = KWin::VirtualDesktopManager::self();
    const bool plus = manager->count() < manager->maximum();
    // A name cleared while it is typed offers to remove the workspace; KDE
    // keeps at least one.
    const bool removable = m_tableRenaming >= 0 && m_tableRenameText.trimmed().isEmpty() && manager->count() > 1;
    m_tableLayout = layoutTable(tableDisplay().size(), tabs, cards, m_table.carrying() ? tr("New") : QString(),
        tr("Empty. It dissolves when you leave."),
        TableMetrics{&TablePresenter::measure}, plus, removable ? tr("Remove workspace") : QString());
    QList<double> tabCentres;
    for (const QRectF &tab : std::as_const(m_tableLayout.tabs)) tabCentres.append(tab.center().x());
    QList<QList<double>> cardCentres;
    for (const auto &row : std::as_const(m_tableLayout.cards)) {
        QList<double> centres;
        for (const QRectF &card : row) centres.append(card.center().x());
        cardCentres.append(centres);
    }
    m_table.setLayout(tabCentres,
        plus ? m_tableLayout.plus.center().x() : std::numeric_limits<double>::quiet_NaN(), cardCentres);
    // Past the line just under the tabs the cards hang, past the one just
    // under the cards a card lifts, and above the tabs a lift cancels.
    const double height = tableDisplay().height();
    if (height > 0)
        m_table.setThresholds(m_tableLayout.depth / height, m_tableLayout.tabsTop / height, m_tableLayout.lift / height);
}

void Effect::refreshTable()
{
    if (!m_table.isOpen()) return;
    KWin::LogicalOutput *output = tableOutput();
    if (!output) {
        closeTable();
        return;
    }
    const QRectF display = tableDisplay();
    const int shown = m_table.shownWorkspace();
    const bool shownValid = shown >= 0 && shown < m_tableWorkspaces.size();
    KWin::EffectWindow *shownCard = nullptr;
    if (shownValid && m_table.shownCard() >= 0 && m_table.shownCard() < m_tableWorkspaces[shown].cards.size()) {
        const TableCard &card = m_tableWorkspaces[shown].cards[m_table.shownCard()];
        if (!card.windows.isEmpty()) shownCard = card.windows.first();
    }
    // Over Spread, the tabs leave Spread showing: a pull previews once it
    // reaches a workspace's cards, and a menu bar once a tap chooses
    // another workspace's tab.
    const bool keepSpread = m_tableOverSpread && m_table.level() == TableLevel::Tabs
        && (m_table.scrubbing() || shown == m_table.current());
    showDesktopPreview(shownValid && !keepSpread ? m_tableWorkspaces[shown].desktop.data() : nullptr,
        shownCard);

    QVariantMap model;
    model[QStringLiteral("row")] = m_tableLayout.row;
    model[QStringLiteral("cancelling")] = m_table.cancelling();
    model[QStringLiteral("carrying")] = m_table.carrying();
    // Float draws no line: the cards come up to meet a
    // finger nearing them, and the chosen one leans toward the lift line as
    // it is pulled there.
    const QPointF finger = m_table.finger();
    const double tabsBottom = m_tableLayout.tabsTop + m_tableLayout.row;
    double approach = 1.0;
    double lean = 0.0;
    if (m_table.scrubbing() && !m_table.carrying()) {
        if (m_table.level() == TableLevel::Tabs) {
            approach = std::clamp((finger.y() - tabsBottom) / std::max(1.0, m_tableLayout.depth - tabsBottom), 0.0, 1.0);
        } else if (m_table.card() >= 0) {
            const double middle = m_tableLayout.trayTop + m_tableLayout.cardHeight / 2.0;
            lean = std::clamp((finger.y() - middle) / std::max(1.0, m_tableLayout.lift - middle), 0.0, 1.0);
        }
    }
    model[QStringLiteral("approach")] = approach;
    model[QStringLiteral("lean")] = lean;
    // The finger quiets the screen as it pulls: during a
    // stroke the band's darkness follows how far it has come, full by the
    // tabs; a flick, the corner and the key bring it in on its own.
    model[QStringLiteral("scrubbing")] = m_table.scrubbing();
    model[QStringLiteral("pull")] = m_table.scrubbing()
        ? std::clamp(finger.y() / std::max(1.0, tabsBottom), 0.0, 1.0) : 1.0;
    // Letting go at the edge changes nothing, and says so beneath the tabs.
    // While a name is typed, the rule it serves is said beneath the tabs.
    model[QStringLiteral("hint")] = m_table.cancelling() ? tr("Let go to cancel")
        : m_tableRenaming >= 0 ? tr("A named workspace stays, even empty") : QString();
    model[QStringLiteral("hintY")] = tabsBottom + 12.0;
    switch (m_tableLayout.parts) {
    case TableTabParts::All: model[QStringLiteral("parts")] = QStringLiteral("all"); break;
    case TableTabParts::NoColours: model[QStringLiteral("parts")] = QStringLiteral("noColours"); break;
    case TableTabParts::NamesOnly: model[QStringLiteral("parts")] = QStringLiteral("namesOnly"); break;
    case TableTabParts::Numbers: model[QStringLiteral("parts")] = QStringLiteral("numbers"); break;
    }
    QVariantList tabs;
    for (int i = 0; i < m_tableWorkspaces.size() && i < m_tableLayout.tabs.size(); ++i) {
        const TableWorkspace &workspace = m_tableWorkspaces[i];
        const QRectF rect = m_tableLayout.tabs[i];
        QString state = QStringLiteral("idle");
        if (m_table.carrying() && m_table.over() == TableGesture::Over::Workspace && m_table.overWorkspace() == i)
            state = QStringLiteral("target");
        else if (m_table.level() == TableLevel::Cards && m_table.locked() == i) state = QStringLiteral("locked");
        else if (m_table.hovered() == i) state = QStringLiteral("hover");
        QVariantList icons;
        for (int c = 0; c < workspace.cards.size() && c < 4; ++c) icons.append(QVariant::fromValue(workspace.cards[c].icon));
        tabs.append(QVariantMap{
            {QStringLiteral("x"), rect.x()}, {QStringLiteral("y"), rect.y()},
            {QStringLiteral("width"), rect.width()}, {QStringLiteral("height"), rect.height()},
            {QStringLiteral("name"), workspace.name}, {QStringLiteral("nameWidth"), m_tableLayout.nameWidths.value(i)},
            {QStringLiteral("number"), i + 1},
            {QStringLiteral("current"), workspace.current},
            {QStringLiteral("state"), state}, {QStringLiteral("icons"), icons},
            {QStringLiteral("editing"), m_tableRenaming == i},
        });
    }
    model[QStringLiteral("tabs")] = tabs;
    const QRectF remove = m_tableLayout.remove;
    model[QStringLiteral("remove")] = QVariantMap{
        {QStringLiteral("visible"), !remove.isEmpty()},
        {QStringLiteral("x"), remove.x()}, {QStringLiteral("y"), remove.y()},
        {QStringLiteral("width"), remove.width()}, {QStringLiteral("height"), remove.height()},
        {QStringLiteral("label"), tr("Remove workspace")},
    };
    const QRectF plus = m_tableLayout.plus;
    model[QStringLiteral("plus")] = QVariantMap{
        {QStringLiteral("visible"), !plus.isEmpty()},
        {QStringLiteral("x"), plus.x()}, {QStringLiteral("y"), plus.y()},
        {QStringLiteral("width"), plus.width()}, {QStringLiteral("height"), plus.height()},
        {QStringLiteral("label"), m_table.carrying() ? tr("New") : QString()},
        {QStringLiteral("hover"), m_table.onPlus()},
        {QStringLiteral("target"), m_table.carrying() && m_table.over() == TableGesture::Over::Plus},
    };
    const int tray = m_table.trayWorkspace();
    QVariantList cards;
    if (tray >= 0 && tray < m_tableWorkspaces.size() && tray < m_tableLayout.cards.size()) {
        const auto &rects = m_tableLayout.cards[tray];
        for (int i = 0; i < rects.size() && i < m_tableWorkspaces[tray].cards.size(); ++i) {
            const TableCard &card = m_tableWorkspaces[tray].cards[i];
            cards.append(QVariantMap{
                {QStringLiteral("x"), rects[i].x()}, {QStringLiteral("y"), rects[i].y()},
                {QStringLiteral("width"), rects[i].width()}, {QStringLiteral("height"), rects[i].height()},
                {QStringLiteral("application"), card.application}, {QStringLiteral("title"), card.title},
                {QStringLiteral("textWidth"), m_tableLayout.textWidths[tray].value(i)},
                {QStringLiteral("icon"), QVariant::fromValue(card.icon)},
                {QStringLiteral("stacked"), std::min(card.stacked, TableSizes::SliversShown)},
                {QStringLiteral("chosen"), m_table.level() == TableLevel::Cards && m_table.card() == i},
                {QStringLiteral("carried"), m_table.carrying() && m_table.carryFrom() == tray && m_table.carryCard() == i},
            });
        }
        if (m_tableWorkspaces[tray].cards.isEmpty()) {
            const QRectF empty = m_tableLayout.empty.value(tray);
            model[QStringLiteral("empty")] = QVariantMap{
                {QStringLiteral("visible"), true}, {QStringLiteral("x"), empty.x()}, {QStringLiteral("y"), empty.y()},
                {QStringLiteral("width"), empty.width()}, {QStringLiteral("height"), empty.height()},
                {QStringLiteral("text"), m_tableWorkspaces[tray].named ? tr("Empty. Named, so it stays.")
                                                                         : tr("Empty. It dissolves when you leave.")},
            };
        }
    }
    model[QStringLiteral("cards")] = cards;
    // The card in hand is the card itself, hanging from the finger by its top
    // edge and never higher than just under the tabs, so the tab it is carried
    // to stays in view.
    const int from = m_table.carryFrom();
    const int held = m_table.carryCard();
    if (m_table.carrying() && from >= 0 && from < m_tableWorkspaces.size() && from < m_tableLayout.cards.size()
        && held >= 0 && held < m_tableWorkspaces[from].cards.size() && held < m_tableLayout.cards[from].size()) {
        const TableCard &card = m_tableWorkspaces[from].cards[held];
        const QRectF rect = m_tableLayout.cards[from][held];
        m_tableCarried = QVariantMap{
            {QStringLiteral("x"), finger.x() - rect.width() / 2.0},
            {QStringLiteral("y"), std::max(finger.y() - 12.0, tabsBottom + 8.0)},
            {QStringLiteral("width"), rect.width()}, {QStringLiteral("height"), rect.height()},
            {QStringLiteral("textWidth"), m_tableLayout.textWidths[from].value(held)},
            {QStringLiteral("application"), card.application}, {QStringLiteral("title"), card.title},
            {QStringLiteral("icon"), QVariant::fromValue(card.icon)},
            {QStringLiteral("stacked"), std::min(card.stacked, TableSizes::SliversShown)},
        };
        model[QStringLiteral("carried")] = m_tableCarried;
    }
    model[QStringLiteral("drop")] = m_tableDrop;
    if (!m_tablePresenter) m_tablePresenter = std::make_unique<TablePresenter>();
    m_tablePresenterOutput = output;
    m_tablePresenter->show(display.toRect(), output->scale(), model);
}

void Effect::openTable(bool sticky, KWin::LogicalOutput *output)
{
    if (m_table.isOpen()) return;
    m_tableOutput = output ? output : tabletOutput();
    if (!m_tableOutput) return;
    // Nothing begun elsewhere may still be in hand.
    if (m_carryRuntime) m_carryRuntime->cancel();
    m_tableWorkspaces = tableWorkspaces();
    int current = -1;
    for (int i = 0; i < m_tableWorkspaces.size(); ++i)
        if (m_tableWorkspaces[i].current) current = i;
    if (current < 0) return;
    m_tableTime.start();
    m_tableOverSpread = m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Spread;
    m_table.open(current, tableNow(), sticky);
    m_tableKeyboard = KWin::effects->grabKeyboard(this);
    quietCardGap(KWin::effects->cursorPos());
    relayoutTable();
    refreshTable();
}

void Effect::closeTable()
{
    m_table.close();
    finishTable();
}

void Effect::finishTable()
{
    m_tableHold.stop();
    endTableRename(false);
    if (m_tableKeyboard) KWin::effects->ungrabKeyboard();
    m_tableKeyboard = false;
    m_tableOutput.clear();
    m_tableOverSpread = false;
    endDesktopPreview();
    m_tableCarried.clear();
    m_tableDrop.clear();
    if (m_tablePresenter) m_tablePresenter->close();
    quietCardGap(KWin::effects->cursorPos());
    scheduleDissolve();
}

int Effect::tableCardAt(const QPointF &local) const
{
    const int tray = m_table.trayWorkspace();
    if (tray < 0 || tray >= m_tableLayout.cards.size()) return -1;
    const auto &cards = m_tableLayout.cards[tray];
    for (int i = 0; i < cards.size(); ++i)
        if (cards[i].contains(local)) return i;
    return -1;
}

TableHit Effect::tableHitAt(const QPointF &local) const
{
    TableHit hit;
    for (int i = 0; i < m_tableLayout.tabs.size(); ++i)
        if (m_tableLayout.tabs[i].contains(local)) hit.tab = i;
    hit.plus = !m_tableLayout.plus.isEmpty() && m_tableLayout.plus.contains(local);
    hit.card = tableCardAt(local);
    const int tray = m_table.trayWorkspace();
    const bool onEmpty = tray >= 0 && m_tableLayout.empty.value(tray).contains(local);
    hit.insideRows = hit.tab >= 0 || hit.plus || hit.card >= 0 || onEmpty
        || (local.y() >= m_tableLayout.tabsTop && local.y() <= m_tableLayout.tabsTop + m_tableLayout.row);
    return hit;
}

bool Effect::aboveActiveCardForInput(const QPointF &position) const
{
    if (!m_cardStage->isActive() || m_cardStage->presentation() != CardPresentation::Active) return false;
    const KWin::EffectWindow *front = m_cardStage->selectedWindow();
    return front && !front->isDeleted() && isTabletPoint(position)
        && position.y() < front->frameGeometry().top();
}

void Effect::beginTableFromInput(const QPointF &position)
{
    openTable(false);
    m_tableStrokeFrom = position;
}

void Effect::pressTableFromInput(const QPointF &position, bool touch)
{
    // While a name is typed, a press only says where the next lift is.
    if (m_tableRenaming >= 0) {
        m_tableRenamePress = true;
        return;
    }
    const QPointF local = position - tableDisplay().topLeft();
    const TableHit hit = tableHitAt(local);
    m_tablePressTouch = touch;
    m_table.press(local, tableCardAt(local), hit.tab, tableNow());
    if (m_table.sticky() && hit.tab >= 0) m_tableHold.start(m_table.config().holdMs);
    else m_tableHold.stop();
    refreshTable();
}

void Effect::contextTableFromInput(const QPointF &position)
{
    if (!m_table.isOpen()) return;
    if (m_tableRenaming >= 0) {
        endTableRename(true);
        return;
    }
    const TableAction action = m_table.contextOn(tableHitAt(position - tableDisplay().topLeft()).tab);
    if (action.kind == TableAction::Kind::Rename) beginTableRename(action.workspace, false);
}

void Effect::moveTableFromInput(const QPointF &position)
{
    if (!m_table.isOpen() || m_tableRenaming >= 0) return;
    const QRectF display = tableDisplay();
    const QPointF local = position - display.topLeft();
    const bool carrying = m_table.carrying();
    m_table.move(local, display.height() > 0 ? local.y() / display.height() : 0.0);
    if (!carrying && m_table.carrying()) {
        const int from = m_table.carryFrom();
        const int card = m_table.carryCard();
        if (from < 0 || from >= m_tableWorkspaces.size() || card < 0 || card >= m_tableWorkspaces[from].cards.size()) {
            m_table.refuseCarry();
        }
    }
    if (carrying != m_table.carrying()) relayoutTable();
    refreshTable();
}

void Effect::releaseTableFromInput(const QPointF &position)
{
    if (!m_table.isOpen()) return;
    m_tableHold.stop();
    const QPointF local = position - tableDisplay().topLeft();
    if (m_tableRenaming >= 0) {
        // The finger that held the tab lets go and the name waits to be
        // typed. A later tap on Remove removes the workspace, on the tab
        // leaves the typing alone, and anywhere else keeps what was typed.
        if (!m_tableRenamePress) {
            (void)m_table.release(local, tableNow());
            return;
        }
        m_tableRenamePress = false;
        if (!m_tableLayout.remove.isEmpty() && m_tableLayout.remove.contains(local)) removeTableWorkspace(m_tableRenaming);
        else if (!m_tableLayout.tabs.value(m_tableRenaming).contains(local)) endTableRename(true);
        return;
    }
    const bool carrying = m_table.carrying();
    const bool stroke = m_table.scrubbing();
    const bool onCards = m_table.level() == TableLevel::Cards;
    const qint64 held = tableNow();
    const TableAction action = m_table.release(local, held, tableHitAt(local));
    // Each stroke says how it went, so one that surprised the hand can be
    // read back against the lines it crossed.
    if (stroke) {
        qInfo() << "Kadunce Table stroke" << held << "ms," << (position.y() - m_tableStrokeFrom.y())
                << "px down, let go" << (carrying ? "carrying a card" : onCards ? "on the cards" : "on the tabs")
                << "with the cards line at" << m_tableLayout.depth << "and the lift line at" << m_tableLayout.lift
                << ":" << tableActionName(action.kind);
    }
    if (carrying && m_table.isOpen()) relayoutTable();
    applyTableAction(action);
}

void Effect::cancelTableFromInput()
{
    closeTable();
}

void Effect::hoverTableFromInput(const QPointF &position)
{
    if (!m_table.isOpen() || !m_table.sticky()) return;
    const TableHit hit = tableHitAt(position - tableDisplay().topLeft());
    if (hit.tab < 0 && hit.card < 0 && !hit.plus) return;
    const int shown = m_table.shownWorkspace();
    const int card = m_table.shownCard();
    const TableLevel level = m_table.level();
    const bool onPlus = m_table.onPlus();
    m_table.hover(hit.tab, hit.tab >= 0 ? -1 : hit.card, hit.plus);
    if (shown != m_table.shownWorkspace() || card != m_table.shownCard() || level != m_table.level()
        || onPlus != m_table.onPlus())
        refreshTable();
}

void Effect::wheelTableFromInput(int steps)
{
    if (!m_table.isOpen() || m_table.down()) return;
    m_table.step(steps);
    refreshTable();
}

void Effect::pointerMovedForInput(const QPointF &position)
{
    quietCardGap(position);
    tracePointerAtTop(position);
    // Each arrival at a display's top-left corner says whether Table opened,
    // so a push that did not open it can be read back.
    KWin::LogicalOutput *corner = nullptr;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        const KWin::Rect geometry = output->geometry();
        if (std::abs(position.x() - geometry.x()) < 3.0 && std::abs(position.y() - geometry.y()) < 3.0) corner = output;
    }
    if (corner == m_pointerCorner) return;
    m_pointerCorner = corner;
    if (!corner) return;
    qInfo() << "Kadunce pointer reached the top-left corner of" << corner->name() << "at" << position
            << (m_table.isOpen() ? "with Table open" : "");
    QTimer::singleShot(600, this, [this] {
        qInfo() << "Kadunce Table" << (m_table.isOpen() ? "is open" : "did not open")
                << "0.6 s after the pointer reached a corner; it is at" << KWin::effects->cursorPos();
    });
}

void Effect::tracePointerAtTop(const QPointF &position)
{
    // Near a display's top edge, each change in what the pointer is over, or
    // in the image it shows, is logged, so a pointer changing shape there can
    // be read back. A client sets its own pointer after
    // the motion that asked for it, so a change shows on the next motion.
    bool nearTop = false;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        const KWin::Rect geometry = output->geometry();
        if (position.x() >= geometry.x() && position.x() < geometry.x() + geometry.width()
            && position.y() >= geometry.y() && position.y() < geometry.y() + 40.0) nearTop = true;
    }
    if (!nearTop) {
        m_topPointer.clear();
        return;
    }
    KWin::Window *under = KWin::input()->findToplevel(position);
    const KWin::PlatformCursorImage cursor = KWin::effects->cursorImage();
    const QImage image = cursor.image();
    const size_t picture = image.isNull() ? 0 : qHashBits(image.constBits(), size_t(image.sizeInBytes()));
    const QByteArray border = under ? borderCursorName(under) : QByteArray();
    const QString seen = QStringLiteral("%1 %2 %3").arg(quintptr(under)).arg(QString::fromLatin1(border)).arg(picture);
    // An animated pointer changes its image by itself, so the log is held
    // to one line in 150 ms.
    if (seen == m_topPointer || (m_topPointerLogged.isValid() && m_topPointerLogged.elapsed() < 150)) return;
    m_topPointer = seen;
    m_topPointerLogged.start();
    const bool inside = under && under->frameGeometry().contains(position);
    qInfo().nospace() << "Kadunce pointer near the top at " << position << " is over "
                      << (under ? under->caption() : QStringLiteral("nothing"))
                      << (under ? (inside ? " inside its frame " : " outside its frame, in its input margin ") : " ")
                      << (under ? under->frameGeometry() : KWin::RectF())
                      << "; its border asks for " << (border.isEmpty() ? QByteArrayLiteral("no shape") : border)
                      << "; the pointer shows image " << picture << " with hot spot " << cursor.hotSpot()
                      << " at " << image.size();
}

void Effect::followTableGesture(qreal progress, KWin::LogicalOutput *output)
{
    if (m_tableGestureDone || progress <= 0.5) return;
    m_tableGestureDone = true;
    if (m_table.isOpen()) {
        if (!m_table.down()) closeTable();
        return;
    }
    openTable(true, output);
}

void Effect::toggleTable()
{
    if (m_table.isOpen()) {
        if (!m_table.down()) closeTable();
        return;
    }
    openTable(true, KWin::effects->screenAt(KWin::effects->cursorPos().toPoint()));
}

bool Effect::borderActivated(KWin::ElectricBorder border)
{
    if (border != KWin::ElectricTopLeft) return false;
    qInfo() << "Kadunce Table's corner fired at" << KWin::effects->cursorPos();
    if (!m_table.isOpen())
        openTable(true, KWin::effects->screenAt(KWin::effects->cursorPos().toPoint()));
    return true;
}

void Effect::grabbedKeyboardEvent(QKeyEvent *event)
{
    if (!m_table.isOpen()) return;
    if (m_tableRenaming >= 0) {
        // Enter keeps the name typed, Escape leaves the old one, and every
        // other key is the name's.
        const bool press = event->type() == QEvent::KeyPress;
        if (press && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) endTableRename(true);
        else if (press && event->key() == Qt::Key_Escape) endTableRename(false);
        else if (m_tablePresenter) m_tablePresenter->forwardKey(event);
        return;
    }
    if (event->type() != QEvent::KeyPress) return;
    using Key = TableGesture::Key;
    std::optional<Key> key;
    switch (event->key()) {
    case Qt::Key_Left: key = Key::Left; break;
    case Qt::Key_Right: key = Key::Right; break;
    case Qt::Key_Up: key = Key::Up; break;
    case Qt::Key_Down: key = Key::Down; break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space: key = Key::Enter; break;
    case Qt::Key_Escape: key = Key::Escape; break;
    case Qt::Key_F2: key = Key::Rename; break;
    default: break;
    }
    // Keys wait while a finger or a button is on Table.
    if (!key || m_table.down()) return;
    applyTableAction(m_table.key(*key));
}

void Effect::moveTableCard(int workspace, int card, KWin::VirtualDesktop *destination)
{
    if (!destination || workspace < 0 || workspace >= m_tableWorkspaces.size()
        || card < 0 || card >= m_tableWorkspaces[workspace].cards.size()) return;
    const TableCard moved = m_tableWorkspaces[workspace].cards[card];
    // A stack goes whole and is stacked again there, as it left. Its windows
    // leave this desktop's cards as any window does;
    // they are that desktop's cards once it is shown.
    for (auto &decks : m_pendingDecks)
        decks.removeIf([&moved](const auto &deck) {
            return std::any_of(deck.cbegin(), deck.cend(), [&moved](const auto &window) { return moved.windows.contains(window); });
        });
    if (moved.stacked > 0) m_pendingDecks[destination->id()].append(moved.windows);
    for (const auto &window : moved.windows)
        if (window && !window->isDeleted()) KWin::effects->windowToDesktops(window, {destination});
    m_tableWorkspaces = tableWorkspaces();
    int destinationIndex = -1;
    int cardIndex = -1;
    for (int i = 0; i < m_tableWorkspaces.size(); ++i) {
        if (m_tableWorkspaces[i].desktop != destination) continue;
        destinationIndex = i;
        for (int c = 0; c < m_tableWorkspaces[i].cards.size(); ++c)
            if (!moved.windows.isEmpty() && m_tableWorkspaces[i].cards[c].windows.contains(moved.windows.first())) cardIndex = c;
    }
    relayoutTable();
    if (destinationIndex >= 0) m_table.showAfterMove(destinationIndex, cardIndex);
    // The card lands from where it was let go into its place in the row it
    // joined, which Table now shows.
    if (!m_tableCarried.isEmpty() && destinationIndex >= 0 && destinationIndex < m_tableLayout.cards.size()
        && cardIndex >= 0 && cardIndex < m_tableLayout.cards[destinationIndex].size()) {
        const QRectF slot = m_tableLayout.cards[destinationIndex][cardIndex];
        QVariantMap drop = m_tableCarried;
        drop[QStringLiteral("seq")] = ++m_tableDrops;
        drop[QStringLiteral("toX")] = slot.x();
        drop[QStringLiteral("toY")] = slot.y();
        drop[QStringLiteral("card")] = cardIndex;
        m_tableDrop = drop;
    }
    m_tableCarried.clear();
    refreshTable();
}

void Effect::applyTableAction(const TableAction &action)
{
    const auto desktopAt = [this](int index) -> KWin::VirtualDesktop * {
        return index >= 0 && index < m_tableWorkspaces.size() ? m_tableWorkspaces[index].desktop.data() : nullptr;
    };
    switch (action.kind) {
    case TableAction::Kind::None:
    case TableAction::Kind::StayOpen:
        refreshTable();
        return;
    case TableAction::Kind::Close:
        finishTable();
        return;
    case TableAction::Kind::Enter: {
        KWin::VirtualDesktop *desktop = desktopAt(action.workspace);
        if (desktop && desktop != KWin::effects->currentDesktop()) {
            showDesktopPreview(desktop, nullptr);
            commitDesktopPreview();
        }
        finishTable();
        return;
    }
    case TableAction::Kind::Activate: {
        KWin::VirtualDesktop *desktop = desktopAt(action.workspace);
        QPointer<KWin::EffectWindow> window;
        if (desktop && action.card >= 0 && action.card < m_tableWorkspaces[action.workspace].cards.size()
            && !m_tableWorkspaces[action.workspace].cards[action.card].windows.isEmpty())
            window = m_tableWorkspaces[action.workspace].cards[action.card].windows.first();
        if (desktop && desktop != KWin::effects->currentDesktop()) {
            showDesktopPreview(desktop, nullptr);
            commitDesktopPreview();
        }
        finishTable();
        // Active in its own workspace (§6: selecting a card makes it Active).
        if (window && !window->isDeleted()) KWin::effects->activateWindow(window);
        return;
    }
    case TableAction::Kind::Move:
        moveTableCard(action.workspace, action.card, desktopAt(action.destination));
        return;
    case TableAction::Kind::Create: {
        if (action.workspace < 0 || action.workspace >= m_tableWorkspaces.size()
            || action.card < 0 || action.card >= m_tableWorkspaces[action.workspace].cards.size()) {
            refreshTable();
            return;
        }
        // Named after the application of the card that made it.
        auto *manager = KWin::VirtualDesktopManager::self();
        KWin::VirtualDesktop *created = manager->createVirtualDesktop(
            manager->count(), m_tableWorkspaces[action.workspace].cards[action.card].application);
        if (!created) {
            m_table.showAfterMove(action.workspace, action.card);
            refreshTable();
            return;
        }
        moveTableCard(action.workspace, action.card, created);
        return;
    }
    case TableAction::Kind::New: {
        // KDE names it; its tab is its number until an application names it.
        auto *manager = KWin::VirtualDesktopManager::self();
        if (KWin::VirtualDesktop *created = manager->createVirtualDesktop(manager->count())) {
            showDesktopPreview(created, nullptr);
            commitDesktopPreview();
        }
        finishTable();
        return;
    }
    case TableAction::Kind::Rename:
        beginTableRename(action.workspace, false);
        return;
    }
}

void Effect::beginTableRename(int workspace, bool touch)
{
    if (workspace < 0 || workspace >= m_tableWorkspaces.size() || !m_tableWorkspaces[workspace].desktop) return;
    m_tableRenaming = workspace;
    m_tableRenamePress = false;
    m_tableRenameByTouch = touch;
    m_tableRenameText = m_tableWorkspaces[workspace].name;
    relayoutTable();
    refreshTable();
    if (!m_tablePresenter) return;
    // KWin gives the keys' text to the focused application's field first;
    // while a name is typed that application has none.
    auto *seat = KWin::waylandServer()->seat();
    m_tableTextFocus = seat->focusedTextInputSurface();
    seat->setFocusedTextInputSurface(nullptr);
    m_tablePresenter->beginEditing(m_tableRenameText, [this](const QString &typed) {
        if (m_tableRenaming < 0) return;
        m_tableRenameText = typed;
        relayoutTable();
        refreshTable();
    });
    // A finger held the tab, so the keys come up, as the person asked for
    // them; a mouse or the keyboard has keys already.
    if (touch) raiseKeyboard();
    quietCardGap(KWin::effects->cursorPos());
}

void Effect::endTableRename(bool commit)
{
    if (m_tableRenaming < 0) return;
    const int workspace = m_tableRenaming;
    const QString typed = (m_tablePresenter ? m_tablePresenter->editedText() : m_tableRenameText).trimmed();
    m_tableRenaming = -1;
    m_tableRenamePress = false;
    m_tableRenameByTouch = false;
    m_tableRenameText.clear();
    if (m_tablePresenter) m_tablePresenter->endEditing();
    quietCardGap(KWin::effects->cursorPos());
    auto *seat = KWin::waylandServer()->seat();
    if (!seat->focusedTextInputSurface() && m_tableTextFocus && m_tableTextFocus == seat->focusedKeyboardSurface())
        seat->setFocusedTextInputSurface(m_tableTextFocus);
    m_tableTextFocus.clear();
    KWin::VirtualDesktop *desktop = workspace < m_tableWorkspaces.size() ? m_tableWorkspaces[workspace].desktop.data() : nullptr;
    if (commit && desktop) {
        // A name the person gives keeps the workspace, even one kept as it
        // was; cleared, it goes back to its application's name and keeps
        // it no longer.
        if (!typed.isEmpty()) {
            desktop->setName(typed);
            if (!m_namedDesktops.contains(desktop->id())) m_namedDesktops.append(desktop->id());
        } else {
            desktop->setName(QString());
            m_namedDesktops.removeAll(desktop->id());
        }
        KWin::VirtualDesktopManager::self()->save();
        saveNamedDesktops();
    }
    if (!m_table.isOpen()) return;
    m_tableWorkspaces = tableWorkspaces();
    relayoutTable();
    refreshTable();
}

void Effect::removeTableWorkspace(int workspace)
{
    auto *manager = KWin::VirtualDesktopManager::self();
    KWin::VirtualDesktop *desktop = workspace >= 0 && workspace < m_tableWorkspaces.size()
        ? m_tableWorkspaces[workspace].desktop.data() : nullptr;
    if (!desktop || manager->count() <= 1) {
        endTableRename(false);
        return;
    }
    endTableRename(false);
    m_namedDesktops.removeAll(desktop->id());
    saveNamedDesktops();
    // KDE moves its windows to the workspace that takes its place, or the one
    // before it when it was the last, and closes Table as a desktop goes;
    // Table comes back as the menu bar it was, on the same display.
    const QPointer<KWin::LogicalOutput> output = m_tableOutput;
    endDesktopPreview();
    manager->removeVirtualDesktop(desktop);
    if (!m_table.isOpen() && output) openTable(true, output);
}

QString Effect::tableState() const
{
    QJsonArray workspaces;
    for (const auto &workspace : m_tableWorkspaces) {
        QJsonArray cards;
        QJsonArray titles;
        QJsonArray stacked;
        for (const auto &card : workspace.cards) {
            cards.append(card.application);
            titles.append(card.title);
            stacked.append(card.stacked);
        }
        workspaces.append(QJsonObject{
            {QStringLiteral("id"), workspace.desktop ? workspace.desktop->id() : QString()},
            {QStringLiteral("name"), workspace.name},
            {QStringLiteral("named"), workspace.named},
            {QStringLiteral("current"), workspace.current},
            {QStringLiteral("cards"), cards},
            {QStringLiteral("titles"), titles},
            {QStringLiteral("stacked"), stacked},
        });
    }
    const auto centre = [](const QRectF &rect) {
        return QJsonArray{rect.center().x(), rect.center().y()};
    };
    QJsonArray tabs;
    for (const QRectF &rect : m_tableLayout.tabs) tabs.append(centre(rect));
    QJsonArray tray;
    const int trayWorkspace = m_table.trayWorkspace();
    if (trayWorkspace >= 0 && trayWorkspace < m_tableLayout.cards.size())
        for (const QRectF &rect : m_tableLayout.cards[trayWorkspace]) tray.append(centre(rect));
    QJsonArray allCards;
    for (const auto &row : m_tableLayout.cards) {
        QJsonArray centres;
        for (const QRectF &rect : row) centres.append(centre(rect));
        allCards.append(centres);
    }
    const QJsonObject layout{
        {QStringLiteral("tabs"), tabs},
        {QStringLiteral("plus"), centre(m_tableLayout.plus)},
        {QStringLiteral("remove"), m_tableLayout.remove.isEmpty() ? QJsonValue() : QJsonValue(centre(m_tableLayout.remove))},
        {QStringLiteral("tray"), tray},
        {QStringLiteral("cards"), allCards},
        {QStringLiteral("depth"), m_tableLayout.depth},
        {QStringLiteral("trayTop"), m_tableLayout.trayTop},
        {QStringLiteral("lift"), m_tableLayout.lift},
    };
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("layout"), layout},
        {QStringLiteral("open"), m_table.isOpen()},
        // Where the layout's display starts, for a pointer on another display.
        {QStringLiteral("origin"), QJsonArray{tableDisplay().x(), tableDisplay().y()}},
        {QStringLiteral("keys"), m_tableKeyboard},
        {QStringLiteral("sticky"), m_table.sticky()},
        {QStringLiteral("level"), m_table.level() == TableLevel::Cards ? QStringLiteral("cards") : QStringLiteral("tabs")},
        {QStringLiteral("hovered"), m_table.hovered()},
        {QStringLiteral("locked"), m_table.locked()},
        {QStringLiteral("card"), m_table.card()},
        {QStringLiteral("carrying"), m_table.carrying()},
        {QStringLiteral("cancelling"), m_table.cancelling()},
        {QStringLiteral("preview"), m_previewDesktop ? m_previewDesktop->id() : QString()},
        {QStringLiteral("previewCard"), m_previewCard ? m_previewCard->caption() : QString()},
        {QStringLiteral("named"), QJsonArray::fromStringList(m_namedDesktops)},
        {QStringLiteral("renaming"), m_tableRenaming},
        {QStringLiteral("typing"), m_tablePresenter && m_tablePresenter->hasTextFocus()},
        {QStringLiteral("textFocusedApplication"), KWin::waylandServer()->seat()->focusedTextInputSurface() != nullptr},
        {QStringLiteral("renameText"), m_tableRenaming >= 0 && m_tablePresenter ? m_tablePresenter->editedText() : QString()},
        {QStringLiteral("workspaces"), workspaces},
    }).toJson(QJsonDocument::Compact));
}

void Effect::loadNamedDesktops()
{
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Table"));
    m_namedDesktops = group.readEntry(QStringLiteral("NamedDesktops"), QStringList{});
    // A workspace pinned before names kept them stays kept, as named.
    if (group.hasKey("PinnedDesktops")) {
        for (const QString &id : group.readEntry(QStringLiteral("PinnedDesktops"), QStringList{}))
            if (!m_namedDesktops.contains(id)) m_namedDesktops.append(id);
        group.deleteEntry(QStringLiteral("PinnedDesktops"));
        saveNamedDesktops();
    }
}

void Effect::setUsesDrawnZones(bool uses)
{
    if (uses == m_usesDrawnZones) return;
    m_usesDrawnZones = uses;
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Bento"));
    group.writeEntry(QStringLiteral("UseDrawnZones"), uses);
    group.sync();
    forEachSession([this] { m_desktopStage->zoneModeChanged(); });
}

// Meta+Shift+B says which arrangement the monitors now take, as Plasma's own
// switches do.
void Effect::switchZoneMode()
{
    setUsesDrawnZones(!m_usesDrawnZones);
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/org/kde/osdService"), QStringLiteral("org.kde.osdService"), QStringLiteral("showText"));
    message << QStringLiteral("view-grid")
            << (m_usesDrawnZones ? QStringLiteral("Monitors: my zones") : QStringLiteral("Monitors: auto fill"));
    QDBusConnection::sessionBus().asyncCall(message);
}

void Effect::saveNamedDesktops()
{
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Table"));
    group.writeEntry(QStringLiteral("NamedDesktops"), m_namedDesktops);
    group.sync();
}

void Effect::scheduleDissolve()
{
    if (m_dissolveQueued) return;
    m_dissolveQueued = true;
    // A closing or departing window is still counted inside the call that
    // announces it; the turn after sees where everything stands.
    QTimer::singleShot(0, this, &Effect::dissolveEmptyWorkspaces);
}

void Effect::dissolveEmptyWorkspaces()
{
    m_dissolveQueued = false;
    if (m_table.isOpen()) return;
    auto *manager = KWin::VirtualDesktopManager::self();
    const auto desktops = KWin::effects->desktops();
    const auto stack = KWin::effects->stackingOrder();
    for (KWin::VirtualDesktop *desktop : desktops) {
        if (manager->count() <= 1) return;
        // The workspace the person is in stays until they leave it.
        if (desktop == KWin::effects->currentDesktop() || m_namedDesktops.contains(desktop->id())) continue;
        const bool kept = std::any_of(stack.cbegin(), stack.cend(), [desktop](KWin::EffectWindow *window) {
            return !window->isDeleted() && window->isNormalWindow() && !window->isOnAllDesktops()
                && !dependentLead(window) && window->isOnDesktop(desktop);
        });
        if (!kept) manager->removeVirtualDesktop(desktop);
    }
}

bool Effect::isDependentWindow(const KWin::EffectWindow *window)
{
    return window && !window->isDeleted()
        && (window->isNormalWindow() || window->isDialog() || window->isUtility())
        && dependentLead(window);
}

bool Effect::dependentShown(const KWin::EffectWindow *lead) const
{
    const DesktopSession *holder = sessionHolding(lead);
    if (!lead || !holder || holder->cards->liveCardIndex(lead) < 0) return true;
    // A card on a desktop that is not shown is not in front.
    if (holder != m_currentSession) return false;
    return m_cardStage->presentation() == CardPresentation::Active
        && m_cardStage->selectedWindow() == lead;
}

void Effect::scheduleDependentSync()
{
    if (m_dependentSyncQueued) return;
    m_dependentSyncQueued = true;
    // KWin activates a new window inside the call that announces it, and an
    // activation unhides; the decision runs once that call has returned.
    QTimer::singleShot(0, this, [this] { syncDependentWindows(); });
}

void Effect::syncDependentWindows()
{
    m_dependentSyncQueued = false;
    m_dependents.removeIf([](const auto &w) { return !w || w->isDeleted(); });
    m_heldDependents.removeIf([](const auto &w) { return !w || w->isDeleted(); });
    KWin::Window *focus = nullptr;
    {
        QScopedValueRollback<bool> holding(m_holdingDependents, true);
        for (auto *window : KWin::effects->stackingOrder()) {
            if (!isDependentWindow(window)) continue;
            auto *lead = dependentLead(window);
            if (!m_dependents.contains(window)) m_dependents.append(window);
            const bool held = m_heldDependents.contains(window);
            if (!dependentShown(lead)) {
                if (!window->window()->isHidden()) {
                    // Hiding the focused window would have KWin focus the next
                    // one, which for a modal dialog is the dialog again. The
                    // keys go to the card in front, and nowhere when that card
                    // is the one the dialog is waiting with.
                    if (window->window()->isActive()) {
                        auto *front = m_cardStage->selectedWindow();
                        if (front && front != lead && front->window()) {
                            KWin::workspace()->activateWindow(front->window());
                        } else {
                            KWin::workspace()->setActiveWindow(nullptr);
                            KWin::workspace()->focusToNull();
                        }
                    }
                    window->window()->setHidden(true);
                }
                if (!held) m_heldDependents.append(window);
                if (!lead->window()->isActive() && !lead->window()->isDemandingAttention()) {
                    lead->window()->demandAttention(true);
                    if (!m_waitingLeads.contains(lead->window())) m_waitingLeads.append(lead->window());
                }
            } else if (held) {
                m_heldDependents.removeAll(window);
                window->window()->setHidden(false);
                // The person came to the application, so its dialog takes the
                // keys as it would have had it opened in front.
                if (lead->window()->isActive() || window->window()->isActive()) focus = window->window();
            }
        }
        for (const auto &lead : std::as_const(m_waitingLeads)) {
            if (!lead) continue;
            const bool stillWaiting = std::any_of(m_heldDependents.cbegin(), m_heldDependents.cend(),
                [&lead](const auto &w) { return w && dependentLead(w) && dependentLead(w)->window() == lead; });
            if (!stillWaiting) lead->demandAttention(false);
        }
        m_waitingLeads.removeIf([](const auto &lead) {
            return !lead || !lead->isDemandingAttention();
        });
    }
    if (focus) KWin::workspace()->activateWindow(focus);
}

void Effect::handleWindowOutputChanged()
{
    auto *client = qobject_cast<KWin::Window *>(sender());
    const QPointer<KWin::EffectWindow> window = client ? client->effectWindow() : nullptr;
    if (!window) return;
    // A window KWin sends to another display some other way than a display
    // change, such as its window-to-screen shortcut, is answered as one that
    // opened there. KWin reports the change inside the move that makes it, so
    // the turn after sees where the window stands. A card, a pane or a window
    // being carried already has an owner deciding where it goes.
    QTimer::singleShot(0, this, [this, window] {
        if (!window || m_releasing || !isCardWindow(window) || isDependentWindow(window)
            || !window->window() || !window->window()->readyForPainting()
            || !window->isOnCurrentDesktop() || window->isUserMove() || window->isUserResize()
            || window == m_carriedWindow || window == m_nativeCarry
            || sessionHolding(window)) return;
        KWin::LogicalOutput *tablet = tabletOutput();
        if (tablet && window->window()->moveResizeOutput() == tablet) {
            // The card display hides any window that is not a card, so the
            // arrival becomes one the way a window a display change moved does.
            scheduleCardDisplaySettle();
            return;
        }
        // A display presenting a layout takes it as a pane; one without a
        // layout leaves it an ordinary window.
        if (m_desktopStage->handleWindowAdded(window)) {
            observeCardOwnership();
            Q_EMIT workspaceContextChanged();
        }
    });
}

void Effect::handleTransientChanged()
{
    auto *client = qobject_cast<KWin::Window *>(sender());
    KWin::EffectWindow *window = client ? client->effectWindow() : nullptr;
    if (!isDependentWindow(window) || m_dependents.contains(window)) return;
    KWin::EffectWindow *lead = dependentLead(window);
    // The box was what the person was looking at, so its application is.
    const bool inFront = m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Active
        && m_cardStage->selectedWindow() == window;
    const bool card = m_cardStage->liveCardIndex(window) >= 0;
    const bool pane = m_desktopStage->managesWindow(window);
    if (card) m_cardStage->handleWindowClosed(window);
    if (pane) m_desktopStage->handleWindowClosed(window);
    if (card || pane) {
        // Placed as a card or a pane; it floats over its application instead.
        const KWin::RectF over = lead->frameGeometry();
        const KWin::RectF box = client->frameGeometry();
        client->move(onLeadDisplay(over, QSizeF(box.width(), box.height()),
            QPointF(over.center().x() - box.width() / 2.0, over.center().y() - box.height() / 2.0)));
        m_cardLabelTargets.remove(window);
        observeCardOwnership();
        qInfo() << "Kadunce" << window->caption() << "belongs to" << lead->caption()
                << "and waits with it rather than as a card";
    }
    m_dependents.append(window);
    if (inFront) {
        m_cardStage->handleWindowActivated(lead);
        // It had the keys as a card and keeps them over its application.
        KWin::workspace()->activateWindow(client);
    }
    scheduleDependentSync();
    Q_EMIT workspaceContextChanged();
}

void Effect::returnDependentWindows()
{
    for (const auto &window : std::as_const(m_heldDependents))
        if (window && !window->isDeleted() && window->window()) window->window()->setHidden(false);
    for (const auto &lead : std::as_const(m_waitingLeads))
        if (lead) lead->demandAttention(false);
    m_heldDependents.clear();
    m_waitingLeads.clear();
    m_drawnDialogs.clear();
    releaseCarriedDialogs();
}

void Effect::updateDrawnDialogs()
{
    const bool spread = m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Spread;
    bool changed = false;
    for (auto it = m_drawnDialogs.begin(); it != m_drawnDialogs.end();) {
        if (spread && m_heldDependents.contains(it->first)) { ++it; continue; }
        it = m_drawnDialogs.erase(it);
        changed = true;
    }
    if (spread) {
        for (const auto &dialog : std::as_const(m_heldDependents)) {
            if (!dialog || dialog->isDeleted() || m_drawnDialogs.contains(dialog)) continue;
            m_drawnDialogs.emplace(dialog, std::make_unique<KWin::EffectWindowVisibleRef>(
                dialog, KWin::EffectWindow::PAINT_DISABLED));
            changed = true;
        }
    }
    if (changed) KWin::effects->addRepaintFull();
}

QList<KWin::EffectWindow *> Effect::drawnDialogsOf(const KWin::EffectWindow *lead) const
{
    QList<KWin::EffectWindow *> dialogs;
    if (m_drawnDialogs.empty()) return dialogs;
    for (auto *window : KWin::effects->stackingOrder()) {
        if (m_drawnDialogs.contains(window) && !window->isDeleted() && dependentLead(window) == lead)
            dialogs.append(window);
    }
    return dialogs;
}

bool Effect::paintDialogsOn(const KWin::RenderTarget &renderTarget,
                            const KWin::RenderViewport &viewport, KWin::EffectWindow *lead,
                            const QList<KWin::EffectWindow *> &dialogs, const KWin::Region &clip,
                            const KWin::WindowPaintData &leadData)
{
    for (auto *dialog : dialogs) {
        // Its application is scaled about its own corner and moved; the same
        // mapping about the dialog's corner moves it by its offset's change.
        const QPointF offset = dialog->pos() - lead->pos();
        KWin::WindowPaintData data;
        data.setOpacity(leadData.opacity());
        data.setXScale(leadData.xScale());
        data.setYScale(leadData.yScale());
        data.setXTranslation(leadData.xTranslation() + offset.x() * (leadData.xScale() - 1.0));
        data.setYTranslation(leadData.yTranslation() + offset.y() * (leadData.yScale() - 1.0));
        if (!qFuzzyIsNull(leadData.rotationAngle())) {
            const QVector3D origin = leadData.rotationOrigin();
            data.setRotationAngle(leadData.rotationAngle());
            data.setRotationOrigin(QVector3D(origin.x() - offset.x(), origin.y() - offset.y(), origin.z()));
        }
        if (!painted([&] {
                return KWin::effects->paintWindow(renderTarget, viewport, dialog,
                    PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT, clip, data);
            }))
            return false;
    }
    return true;
}

void Effect::takeCarriedDialogs(KWin::EffectWindow *lead)
{
    releaseCarriedDialogs();
    if (!lead || lead->isDeleted() || !lead->window()) return;
    const KWin::RectF frame = lead->frameGeometry();
    if (frame.width() <= 0 || frame.height() <= 0) return;
    for (const auto &dialog : std::as_const(m_dependents)) {
        if (!dialog || dialog->isDeleted() || !dialog->window() || dependentLead(dialog) != lead) continue;
        const KWin::RectF box = dialog->frameGeometry();
        // One wider or taller than its application has no place on it that
        // way but its middle; where the screen pushed it is not one.
        m_carriedDialogs.append({dialog, QPointF(
            box.width() >= frame.width() ? 0.5 : (box.center().x() - frame.x()) / frame.width(),
            box.height() >= frame.height() ? 0.5 : (box.center().y() - frame.y()) / frame.height())});
        // KWin sends a dialog to its application's new display on its own,
        // keeping its place on the display rather than on the application.
        m_carriedDialogWatch.append(connect(dialog->window(), &KWin::Window::frameGeometryChanged,
                                            this, &Effect::placeCarriedDialogs));
    }
    if (m_carriedDialogs.isEmpty()) return;
    m_dialogsLead = lead;
    m_carriedDialogWatch.append(connect(lead->window(), &KWin::Window::frameGeometryChanged,
                                        this, &Effect::placeCarriedDialogs));
}

void Effect::placeCarriedDialogs()
{
    if (m_placingCarriedDialogs || !m_dialogsLead || m_dialogsLead->isDeleted()) return;
    QScopedValueRollback<bool> placing(m_placingCarriedDialogs, true);
    const KWin::RectF frame = m_dialogsLead->frameGeometry();
    for (const auto &carried : std::as_const(m_carriedDialogs)) {
        auto *dialog = carried.dialog.data();
        if (!dialog || dialog->isDeleted() || !dialog->window()) continue;
        const KWin::RectF box = dialog->frameGeometry();
        const QPointF to = onLeadDisplay(frame, QSizeF(box.width(), box.height()),
            QPointF(std::round(frame.x() + carried.place.x() * frame.width() - box.width() / 2.0),
                    std::round(frame.y() + carried.place.y() * frame.height() - box.height() / 2.0)));
        if (QPointF(box.x(), box.y()) != to) dialog->window()->move(to);
    }
}

QPointF Effect::onLeadDisplay(const KWin::RectF &lead, const QSizeF &size, QPointF to)
{
    auto *output = KWin::effects->screenAt(lead.center().toPoint());
    if (!output) return to;
    const KWin::RectF area = KWin::effects->clientArea(KWin::MaximizeArea, output);
    to.setX(std::max(area.x(), std::min(to.x(), area.x() + area.width() - size.width())));
    to.setY(std::max(area.y(), std::min(to.y(), area.y() + area.height() - size.height())));
    return to;
}

void Effect::releaseCarriedDialogs()
{
    m_carriedDialogsRelease.stop();
    placeCarriedDialogs();
    for (const auto &connection : std::as_const(m_carriedDialogWatch)) disconnect(connection);
    m_carriedDialogWatch.clear();
    m_carriedDialogs.clear();
    m_dialogsLead.clear();
}

QList<KWin::EffectWindow *> Effect::carriedDialogs() const
{
    QList<KWin::EffectWindow *> dialogs;
    for (const auto &carried : std::as_const(m_carriedDialogs))
        if (carried.dialog && !carried.dialog->isDeleted()) dialogs.append(carried.dialog);
    return dialogs;
}

bool Effect::drawnWithCarriedCard(const KWin::EffectWindow *window) const
{
    if (!m_dialogsLead || m_dialogsLead->isDeleted()) return false;
    const bool carried = std::any_of(m_carriedDialogs.cbegin(), m_carriedDialogs.cend(),
        [window](const auto &entry) { return entry.dialog == window; });
    if (!carried) return false;
    KWin::EffectWindow *lead = m_dialogsLead;
    return (lead == m_carriedWindow && m_carryRuntime)
        || (lead == m_settlingWindow && dropSettleRect())
        || bentoMotionRect(lead);
}

KWin::LogicalOutput *Effect::tabletOutput() const
{
    const QList<KWin::LogicalOutput *> outputs = KWin::effects->screens();
    const auto it = std::find_if(outputs.cbegin(), outputs.cend(),
                                 [this](const KWin::LogicalOutput *output) {
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
    // Keys raised for nobody are never drawn, so they take no room either;
    // keys on their way out after the compositor hid them still do.
    KWin::EffectWindow *panel = KWin::effects->inputPanel();
    if (!output || !m_keysForPerson || !panel || panel->isDeleted()
        || !(panel->isVisible() || panel == m_leavingPanel)) {
        return std::nullopt;
    }
    const KWin::RectF covered =
        KWin::RectF(panel->frameGeometry()).intersected(
            KWin::RectF(output->geometry()));
    // Keys held below the screen's edge leave a sliver of panel a pixel or two
    // tall. The dock steps aside only for keys on screen, and the card makes
    // room only for them too.
    constexpr double KeysOnScreen = 8.0;
    if (covered.isEmpty() || covered.height() < KeysOnScreen) {
        return std::nullopt;
    }
    return covered.top();
}

KWin::RectF Effect::workAreaForCardStage(const KWin::LogicalOutput *output) const
{
    const KWin::RectF work = KWin::effects->clientArea(KWin::MaximizeArea, output);
    // A panel that gives its room up to the keys lends it to them, not to the
    // cards: a card chosen while the keys are up, or in the moment before the
    // panel is back, stops where the panel stood. One that appears meanwhile
    // still takes its room.
    const auto held = m_keysWorkAreas.constFind(output);
    return held == m_keysWorkAreas.cend() ? work : work.intersected(*held);
}

void Effect::followKeysWorkArea()
{
    KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
    if ((method && method->isVisible()) || m_leavingPanel) {
        m_keysWorkAreaRelease.stop();
        // Keys back before a panel returned find the area already held.
        if (m_keysWorkAreas.isEmpty()) {
            for (const KWin::LogicalOutput *output : KWin::effects->screens())
                m_keysWorkAreas.insert(output, KWin::effects->clientArea(KWin::MaximizeArea, output));
        }
        return;
    }
    if (m_keysWorkAreas.isEmpty()) return;
    // The keys have gone. Nothing lent them room, or it is back: let go now.
    // Otherwise wait for the panel, but not for one that never returns.
    for (const KWin::LogicalOutput *output : KWin::effects->screens()) {
        const auto held = m_keysWorkAreas.constFind(output);
        if (held == m_keysWorkAreas.cend()) continue;
        const KWin::RectF work = KWin::effects->clientArea(KWin::MaximizeArea, output);
        if (work.intersected(*held) != work) {
            if (!m_keysWorkAreaRelease.isActive()) m_keysWorkAreaRelease.start();
            return;
        }
    }
    releaseKeysWorkArea();
}

void Effect::releaseKeysWorkArea()
{
    m_keysWorkAreaRelease.stop();
    if (m_keysWorkAreas.isEmpty()) return;
    m_keysWorkAreas.clear();
    m_cardStage->followWorkArea();
}

bool Effect::keyboardTypesIntoForCardStage(
    const KWin::EffectWindow *window) const
{
    // KWin sends text to one window. A card behind a layout or a launcher is
    // not it, and a dialog floating over its own card is the card's.
    KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
    const KWin::Window *target = method ? method->activeWindow() : nullptr;
    const KWin::Window *client = window ? window->window() : nullptr;
    return target && client && (target == client || client->hasTransient(target, true));
}

void Effect::keyboardHeading(double height, int durationMs)
{
    KWin::EffectWindow *panel = KWin::effects->inputPanel();
    if (!m_keysForPerson || !panel || panel->isDeleted()
        || !(panel->isVisible() || panel == m_leavingPanel)
        || !std::isfinite(height) || height < 0 || durationMs < 0 || durationMs > 2000) {
        return;
    }
    // The keys stand on the bottom edge of their panel, which is where the
    // height is measured from.
    const KWin::RectF keys = panel->frameGeometry();
    m_cardStage->keyboardHeading(keys.y() + keys.height() - height, durationMs);
}

void Effect::raiseKeyboard()
{
    KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
    if (!method) return;
    m_keyboardAskedSince.start();
    // Plasma's touch-only setting shows the keys only while a touch or a pen
    // was the last input, so a click on the tray's entry asked for nothing.
    // A request here is the person's own however it was made, so it counts
    // as a touch; KWin's next input of any kind sets it back.
    if (KWin::input()) KWin::input()->setLastInputHandler(KWin::input()->touch());
    method->forceActivate();
}

const char *Effect::keyboardRefusal(const KWin::InputMethod &method) const
{
    // A request through raiseKeyboard is the person's own, from a swipe up
    // from the bottom bezel or the precision surface. It is honoured for the
    // moment the raise takes.
    constexpr qint64 AskedMs = 1500;
    if (m_keyboardAskedSince.isValid() && m_keyboardAskedSince.elapsed() < AskedMs)
        return nullptr;
    // Otherwise the compositor raised the keys because a field was enabled
    // soon after a touch, which is also true when the application focused
    // the field itself after a touch somewhere else in it. A tap on a field
    // puts the text cursor on the line the finger landed on; an application's
    // own focus leaves it wherever the field is. Across the line the cursor
    // lands at the end of the text, far from the finger, so only the line is
    // read.
    KWin::TouchInputRedirection *touch = KWin::input() ? KWin::input()->touch() : nullptr;
    const KWin::Window *target = method.activeWindow();
    if (!target) return "no window holds the text";
    if (!touch || KWin::input()->lastInputHandler() != touch)
        return "the last input was not a touch";
    // A touch Kadunce kept, such as the tap that chose a card in Spread,
    // reached no application, so wherever it landed it was not on the text.
    if (m_inputRouter && m_inputRouter->latestTouchKept())
        return "the last touch was Kadunce's own";
    // Keys a tap brought have answered it once they are put away; with no
    // touch since, the application took the focus back on its own.
    if (m_inputRouter && m_inputRouter->touchEvents() == m_answeredTouchEvents)
        return "keys put away since answered the last touch";
    const QPointF finger = touch->position();
    // Typing on the keys is asking for them.
    if (const KWin::EffectWindow *panel = KWin::effects->inputPanel();
        panel && panel->frameGeometry().contains(finger)) return nullptr;
    if (!target->frameGeometry().contains(finger)) return "the last touch was outside its window";
    // A field that asks within a moment of a tap in its own window is the one
    // tapped. The cursor a browser reports then cannot be read: moving into a
    // box in a frame from another site, it gives the last box's, or this
    // one's from before the window last changed size.
    constexpr qint64 TapJustNowMs = 1000;
    if (m_inputRouter && m_inputRouter->msSinceLatestTouch() < TapJustNowMs
        && m_inputRouter->latestTouchWindow() == target)
        return nullptr;
    const KWin::RectF cursor = method.cursorRectangle();
    // A client that never says where its cursor is cannot be read; the touch
    // landing in its window is all there is to go on.
    if (cursor.height() <= 0.0) return nullptr;
    const double reach = cursor.height() / 2.0;
    if (finger.y() >= cursor.top() - reach && finger.y() <= cursor.bottom() + reach) return nullptr;
    return "the last touch was off the line of its text";
}

void Effect::keepUnaskedKeyboardDown()
{
    // Decided once for each time the keys come up: a client that enables its
    // field again with every keystroke must not have each one read afresh.
    KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
    if (!method || !method->isVisible()) {
        // Hidden with its picture still there, as an application asking for
        // the keys to go does: drawn on while the keyboard takes them out.
        KWin::EffectWindow *panel = KWin::effects->inputPanel();
        KWin::Window *surfaceWindow = method ? method->panel() : nullptr;
        KWin::SurfaceInterface *surface = surfaceWindow ? surfaceWindow->surface() : nullptr;
        if (m_keysForPerson && panel && !panel->isDeleted() && surface && surface->isMapped()) {
            if (!m_leavingKeys) {
                answerTouches();
                m_leavingKeys = std::make_unique<KWin::EffectWindowVisibleRef>(
                    panel, KWin::EffectWindow::PAINT_DISABLED);
                m_leavingPanel = panel;
                m_leavingKeysUnmap = connect(surface, &KWin::SurfaceInterface::unmapped,
                                             this, &Effect::releaseLeavingKeys);
                m_leavingKeysLimit.start();
            }
            return;
        }
        if (m_keysForPerson && !m_leavingKeys) answerTouches();
        releaseLeavingKeys();
        m_keysForPerson = false;
        return;
    }
    // Back before they were out: the same keys, still the person's.
    if (m_leavingKeys) {
        disconnect(m_leavingKeysUnmap);
        m_leavingKeysLimit.stop();
        m_leavingKeys.reset();
        m_leavingPanel.clear();
    }
    if (m_keysForPerson) return;
    const char *refusal = keyboardRefusal(*method);
    if (!refusal) {
        m_keysForPerson = true;
        if (KWin::EffectWindow *panel = KWin::effects->inputPanel())
            KWin::effects->addRepaint(panel->expandedGeometry().toAlignedRect());
        return;
    }
    method->hide();
    const KWin::Window *target = method->activeWindow();
    qInfo().nospace() << "Kadunce kept the keyboard down for "
                      << (target ? target->caption() : QStringLiteral("no window"))
                      << ": " << refusal;
}

void Effect::answerTouches()
{
    // A touch that ended just before the keys began to go may be what sent
    // them: a tap on another field, which a browser focuses only once the
    // finger has lifted and the field before has let go. That field may still
    // ask, so such a touch is left to ask.
    constexpr qint64 SentAwayMs = 1000;
    if (m_inputRouter && m_inputRouter->msSinceLatestTouch() >= SentAwayMs)
        m_answeredTouchEvents = m_inputRouter->touchEvents();
}

void Effect::releaseLeavingKeys()
{
    disconnect(m_leavingKeysUnmap);
    m_leavingKeysLimit.stop();
    if (!m_leavingKeys) return;
    if (m_leavingPanel && !m_leavingPanel->isDeleted())
        KWin::effects->addRepaint(m_leavingPanel->expandedGeometry().toAlignedRect());
    m_leavingKeys.reset();
    m_leavingPanel.clear();
    KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
    if (!method || !method->isVisible()) m_keysForPerson = false;
    m_cardStage->refreshKeyboardRoom();
    followKeysWorkArea();
}

bool Effect::mayHoldWindowForCardStage(
    const KWin::EffectWindow *window) const
{
    // §7 keeps a minimized window owned as an individual card, so what card
    // ownership may hold cannot exclude the one state the section is about.
    // `isCardWindow` answers what this effect can present, and excludes it.
    return isApplicationWindow(window)
        && (!window->isHidden() || window->data(CardAsideRole).toBool());
}

void Effect::setPagingShortcutsForCardStage(bool active)
{
    // The shortcuts page the cards on screen; a desktop not shown has none.
    if (scopedToCurrent()) setPagingShortcutsActive(active);
}

void Effect::cancelInputForCardStage()
{
    // Input and motion belong to the desktop on screen. A session on another
    // desktop changing its cards has none to cancel.
    if (!scopedToCurrent()) return;
    // A Table stroke taken away leaves the menu bar, not a half-finished pull.
    if (m_table.isOpen() && m_table.down()) {
        m_table.interrupt();
        relayoutTable();
        refreshTable();
    }
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
    if (m_placementDrop && window == m_placementDrop->card())
        return m_desktopStage->transferPreparedCard(*m_placementDrop, commitSource, releaseSource);
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
        || m_cardStage->presentation() == CardPresentation::Bento
        || m_cardStage->presentation() == CardPresentation::Desktop) {
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

bool Effect::scrollFromFingersForInput() const
{
    // KWin counts every finger on the glass, those its recogniser took for a
    // gesture too, until each has lifted.
    const KWin::TouchInputRedirection *touch = KWin::input() ? KWin::input()->touch() : nullptr;
    return m_cardStage->spreadOpening() || (touch && touch->touchPointCount() > 0);
}

void Effect::beginBezelSpreadFromInput()
{
    m_spreadGesture = {};
}

void Effect::followBezelSpreadFromInput(double rise)
{
    const KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet) return;
    followSpreadGesture(rise / (tablet->geometry().height() * BezelSpreadTravel));
}

void Effect::finishBezelSpreadFromInput(double rise, double speed, bool cancelled)
{
    using Mode = SpreadGesture::Mode;
    auto &gesture = m_spreadGesture;
    if (cancelled) {
        gesture.progress = 0.0;
    } else if (speed >= BezelFlickSpeed) {
        // A flick opens as surely as a pull past halfway, even one that lifts
        // before the row has begun to form.
        if (gesture.mode == Mode::Idle) gesture.mode = beginSpreadGesture();
        if (gesture.mode == Mode::Follow) {
            gesture.progress = 1.0;
        } else if (gesture.mode == Mode::Commit) {
            gesture.mode = Mode::Opened;
            toggle();
        }
    }
    qInfo() << "Kadunce bezel swipe let go, risen" << rise << "at" << speed << "px/ms"
            << (cancelled ? "cancelled" : "");
    finishSpreadGesture();
}

void Effect::dismissLauncherGuestFromInput()
{
    callLauncherGuestOwner(QStringLiteral("dismissGuest"));
    endLauncherGuest();
}

void Effect::callLauncherGuestOwner(const QString &method, const QVariantList &arguments)
{
    if (m_launcherGuestOwner.isEmpty()) return;
    QDBusMessage request = QDBusMessage::createMethodCall(m_launcherGuestOwner,
        m_launcherGuestPath, m_launcherGuestInterface, method);
    request.setArguments(arguments);
    QDBusConnection::sessionBus().asyncCall(request);
}

void Effect::tapBesideLauncherGuestFromInput(const QPointF &position)
{
    // Search closes. A tap on a card beside it opens that card, as a tap on
    // any card in Spread does; Search grown to the whole display has none.
    const bool card = !m_launcherGuestExpanded
        && m_cardStage->endLauncherGuestOnCard(position);
    dismissLauncherGuestFromInput();
    if (card) activateSelectedFromInput();
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
    // The largest output that is not the tablet. CARD-LIFECYCLE.md §3 gives a
    // Bento action to it while one is attached.
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
    // CARD-LIFECYCLE.md §3 places by the gesture, and this gesture contacts no
    // edge, so the side is stated rather than read: the Active card keeps the
    // left pane and its partner is named by walking right from it.
    return pairActiveCardIntoBento(output, BentoSidePlacement{false, true}, false);
}

bool Effect::pairActiveCardIntoBento(KWin::LogicalOutput *output,
    BentoSidePlacement side, bool walkLeft)
{
    auto *active = m_cardStage->activeCardIdentity();
    if (!output || !active) return false;
    auto *partner = m_cardStage->partnerForSideSnap(active, walkLeft);
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
    // CARD-LIFECYCLE.md §3: external Bento while docked, tablet Bento while
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
                if (!guarded) return;
                DesktopSession *holder = sessionHolding(guarded);
                SessionScope scope(this, holder ? holder : m_currentSession);
                m_desktopStage->handleWindowMinimizedChanged(guarded);
                // §3: the display that can own cards, holding none, takes a
                // window picked back from minimized as its Active card, as it
                // takes a window that opens there. Only that display does: a
                // monitor's windows stay native until a snap or a key asks.
                if (!m_releasing && !guarded->isMinimized() && !m_cardStage->isActive())
                    (void)startTabletInCards(guarded);
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
        connect(window->window(), &KWin::Window::transientChanged,
                this, &Effect::handleTransientChanged, Qt::UniqueConnection);
        connect(window->window(), &KWin::Window::outputChanged,
                this, &Effect::handleWindowOutputChanged, Qt::UniqueConnection);
    }
    connect(window, &KWin::EffectWindow::windowFrameGeometryChanged,
            this, &Effect::handleActiveGeometryChanged,
            Qt::UniqueConnection);
    connect(window, &KWin::EffectWindow::windowDesktopsChanged,
            this, &Effect::handleWindowDesktopsChanged,
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
            traceNativeMove(window, "stage-refused");
        } else {
            traceNativeMove(window, "no-carry-source");
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
    m_carryCardEntryOutput.clear(); m_carryCardExitOutput.clear();
    if (m_dialogsLead) m_carriedDialogsRelease.start();
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
            {QStringLiteral("target"), geometryContext(KWin::RectF(m.to).toRect())},
            {QStringLiteral("arrival"), m.arrival},
            {QStringLiteral("started"), m.timer.isValid()}});
    }
    const auto settling = dropSettleRect();
    const auto &reservation = m_carriedWindow ? m_carryDestination : m_lineDestination;
    const auto &preview = m_carriedWindow ? m_carryPreview : m_linePreview;
    // A Card Stage entry has no Bento reservation to validate; its destination
    // is the Active card target, and it shows the same placement outline.
    const bool cardEntry = m_carriedWindow ? bool(m_carryCardEntryOutput)
                                           : bool(m_lineCardEntryOutput);
    const bool cardExit = m_carriedWindow && m_carryCardExitOutput;
    const bool previewValid = (m_carriedWindow || m_cardStage->cardGrabActive()) && preview
        && (cardEntry || cardExit || (reservation && m_desktopStage->cardDropValid(*reservation)));
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
            previewValid && (cardEntry || cardExit || reservation->showsPlacementOutline())},
        {QStringLiteral("detachPreview"),
            previewValid && (cardExit || (!cardEntry && reservation->detachesToDesktop()))},
        {QStringLiteral("rendererActive"), isActive()},
        {QStringLiteral("destinationRect"), previewValid ? geometryContext(preview->toRect()) : QJsonObject{}},
        {QStringLiteral("lineAnimating"), m_cardStage->animationsRunning()},
        {QStringLiteral("lineRotation"), linePose.rotation},
        {QStringLiteral("lineRect"), QJsonObject{{QStringLiteral("x"), lineRect.x()},
            {QStringLiteral("y"), lineRect.y()}, {QStringLiteral("width"), lineRect.width()},
            {QStringLiteral("height"), lineRect.height()}}},
        {QStringLiteral("carrying"), bool(m_carriedWindow)},
        {QStringLiteral("inputBusy"), m_carryRuntime && m_carryRuntime->route.busy()},
        {QStringLiteral("destination"), bool(m_carryDestination) || bool(m_carryCardEntryOutput)
            || bool(m_carryCardExitOutput)},
        {QStringLiteral("lineCarrying"), m_cardStage->cardGrabActive()},
        {QStringLiteral("lineDestination"),
            bool(m_lineDestination) || bool(m_lineCardEntryOutput)},
        // What a held card would do if let go, where, and how far the row
        // under it has stepped back.
        {QStringLiteral("carryAim"), m_cardStage->carryAimName()},
        {QStringLiteral("carryIndex"), m_cardStage->carryAimIndex()},
        {QStringLiteral("carryPane"), m_cardStage->carryAimPane()},
        {QStringLiteral("carryScale"), m_cardStage->carryScale()}
    }).toJson(QJsonDocument::Compact));
}

void Effect::updateNativeCarryDestination(QPointF contact)
{
    const auto traceDestination = qScopeGuard([this] {
        const QString state = m_carryCardEntryOutput
            ? QStringLiteral("destination-card:%1").arg(m_carryCardEntryOutput->name())
            : m_carryCardExitOutput
            ? QStringLiteral("destination-exit:%1").arg(m_carryCardExitOutput->name())
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
    m_carryCardExitOutput.clear();
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
    const bool atBottomEdge = contact.y() >= double(target->geometry().bottom()) - 24.0;
    // Where a window let go at the bottom edge lands on the ordinary desktop:
    // its floating size, centred under the contact, above the dock.
    const auto exitBox = [&] {
        // KWin keeps a floating geometry only for a window it maximized, tiled
        // or made full screen; any other window's own size is the geometry it
        // had before Kadunce placed it.
        const auto &saved = handoff.source()->restoreSnapshot();
        const bool heldAside = saved.maximizeMode != KWin::MaximizeRestore
            || saved.quickTileMode != KWin::QuickTileMode{} || saved.fullScreen;
        // An empty size, which KWin reports for a window it never had to
        // restore, is no size: the next record answers instead.
        QSizeF size = QRectF(heldAside ? saved.floatingGeometry : saved.geometry).size();
        if (size.isEmpty()) size = QRectF(saved.floatingGeometry).size();
        if (size.isEmpty()) size = m_carryPickup.size();
        size.setWidth(std::clamp(size.width(), 200.0, std::max(200.0, area.width() * .8)));
        size.setHeight(std::clamp(size.height(), 150.0, std::max(150.0, area.height() * .8)));
        QRectF box(contact.x() - size.width() / 2, exitBottom - size.height(), size.width(), size.height());
        box.moveLeft(std::clamp(box.left(), area.left(), area.right() - box.width()));
        return box;
    };
    // §10: the bottom edge releases a card to the ordinary desktop, as it does
    // a pane, and the desktop is shown with it.
    if (local && tablet && !bento && !desktopWindow && atBottomEdge
        && m_cardStage->nativeCarrySourceValid(*handoff.source())) {
        const KWin::RectF box(exitBox());
        QPointer<KWin::EffectWindow> carried = m_carriedWindow;
        QPointer<KWin::LogicalOutput> output = target;
        m_carryCardExitOutput = target;
        m_carryPreview = box;
        handoff.previewDrop({CarryDestinationKind::NativeDesktop, target->name(), target->name(), 0, 0},
            [carried, output] {
                return carried && !carried->isDeleted() && output
                    && KWin::effects->screens().contains(output.data()) && carried->screen() == output;
            },
            [this, output, box](const PreparedCarrySource &source) {
                return m_cardStage->releaseNativeCarryToDesktop(source, output, box);
            });
        return;
    }
    if (local && bento && !desktopWindow && atBottomEdge) {
        const QRectF box = exitBox();
        const auto reserved = m_desktopStage->prepareCardDrop(m_carriedWindow, target,
            KWin::RectF(box), DesktopStageController::CardDropIntent::NativeDesktop);
        if (!reserved) return;
        m_carryDestination = reserved;
        m_carryPreview = m_desktopStage->cardDropPreview(*reserved);
        QPointer<KWin::LogicalOutput> output = target;
        QPointer<KWin::EffectWindow> released = m_carriedWindow;
        handoff.previewDrop({CarryDestinationKind::NativeDesktop, target->name(), target->name(), 0, 0},
            [this, reserved] { return m_desktopStage->cardDropValid(*reserved); },
            [this, reserved, output, released](const PreparedCarrySource &source) {
                const bool committed = m_desktopStage->transferNativeCarryToDesktop(source, *reserved);
                // §10 and §2 on the card display: the desktop is shown with
                // the pane, and what Kadunce still holds there goes aside into
                // Spread. It waits for the carry to finish, as a layout
                // retiring into Spread cancels input.
                if (committed && output && isTabletOutput(output))
                    QTimer::singleShot(0, this, [this, output, released] {
                        showDesktopWithReturned(output, released);
                    });
                return committed;
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

void Effect::showDesktopWithReturned(KWin::LogicalOutput *tablet, KWin::EffectWindow *returned)
{
    if (!tablet || !returned || returned->isDeleted()) return;
    if (m_desktopStage->hasSessionOnOutput(tablet->name())) {
        m_desktopStage->transferTabletSessionToSpread(tablet,
            [this](const auto &projection, const auto &commit) {
                return m_cardStage->admitBentoStack(projection, commit);
            });
        // A layout that cannot become its Spread group stays, and so does
        // what it shows.
        if (m_desktopStage->hasSessionOnOutput(tablet->name())) {
            observeCardOwnership();
            return;
        }
    }
    m_cardStage->returnWindowToDesktop(returned);
    observeCardOwnership();
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
        // An arrival not yet drawn moving stands where the card was let go,
        // and is dropped if no frame draws it at all.
        const bool started = m.timer.isValid();
        const int duration = motion(m.arrival ? PaneArrivalDuration : 220);
        if (!window || window->isDeleted() || !window->window() || !m.output
            || (started ? m.timer.elapsed() >= duration
                        : !m.arrival || !m.asked.isValid() || m.asked.elapsed() >= PaneArrivalAbandon)
            || window->isUserMove() || window->isUserResize() || window->isMinimized()
            || QRectF(m.output->geometry()) != m.outputGeometry
            || window->window()->moveResizeOutput() != m.output
            || QRectF(window->window()->moveResizeGeometry()) != m.to) return std::nullopt;
        const double t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
            paneArrivalProgress(started, started ? m.timer.elapsed() : 0, duration));
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
    // A layout on a desktop not shown settles without being watched.
    if (!scopedToCurrent()) return;
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
    // accepted target; the client's buffer may follow it late. Painting maps
    // whichever buffer is current. A move may wait a short, bounded time for
    // that buffer, as a card arriving in a pane does (PaneArrival.h), but it
    // never holds input or retries geometry.
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
    const double Duration = motion(220);
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
            DesktopSession *holder = sessionHolding(window);
            SessionScope scope(this, holder ? holder : m_currentSession);
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
        if (m_dialogsLead) m_carriedDialogsRelease.start();
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
    if (m_table.isOpen() && tableOutput() == output) closeTable();
    forEachSession([&] { m_desktopStage->handleScreenRemoved(output); });
    // KWin still lists the display while announcing its removal.
    QTimer::singleShot(0, this, &Effect::refreshCardOutput);
    scheduleCardDisplaySettle();
}

void Effect::scheduleCardDisplaySettle()
{
    m_cardDisplaySettleRounds = 0;
    if (m_cardDisplaySettleQueued) return;
    m_cardDisplaySettleQueued = true;
    // KWin announces a display inside the call that rearranges windows for it,
    // and the placement it remembers for a layout is put back at the end of
    // that call. The turn after is the first one that sees where KWin left
    // every window, and it follows the card display's own refresh.
    QTimer::singleShot(0, this, &Effect::settleCardsOnDisplays);
}

void Effect::settleCardsOnDisplays()
{
    m_cardDisplaySettleQueued = false;
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet || !KWin::effects->screens().contains(tablet)) return;
    // The paint route hides any window on the card display that is not a card,
    // so a window KWin moved here must become one or it cannot be seen. So
    // must one that opened or arrived on this desktop while another was shown.
    // A window another desktop's session still holds is that session's.
    const auto arrived = [this, tablet](KWin::EffectWindow *window) {
        return window && isCardWindow(window) && window->isNormalWindow()
            && !isDependentWindow(window) && window->window()
            && window->window()->moveResizeOutput() == tablet
            && window->window()->readyForPainting()
            && window->isOnCurrentDesktop()
            && !window->isUserMove() && !window->isUserResize()
            && window != m_carriedWindow && window != m_nativeCarry
            && !sessionHolding(window);
    };
    QList<QPointer<KWin::EffectWindow>> arrivals;
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder())
        if (arrived(window)) arrivals.append(window);
    bool strayed = false;
    for (const auto &card : m_cardStage->liveCards()) {
        if (card && !card->isDeleted() && card->window()
            && card->window()->moveResizeOutput() != tablet) strayed = true;
    }
    // A desktop presenting its layout and owning no card takes what arrives
    // as cards behind the panes, as one that owns cards does. Left alone they
    // would lie over the panes as windows no one owns, taking every touch: a
    // stack Table carried back to a desktop holding only a layout did.
    const bool behindLayout = !m_cardStage->isActive() && !arrivals.isEmpty()
        && m_desktopStage->hasSessionOnOutput(tablet->name()) && m_cardStage->startBehindLayout();
    // With no card owned and no layout nothing is hidden, so an arrival is an
    // ordinary window there until ownership starts the ordinary way.
    if (!m_cardStage->isActive() || (arrivals.isEmpty() && !strayed)) return;
    cancelInputForCardStage();
    // A client can still acknowledge the placement KWin asked of it before the
    // return, and KWin applies an acknowledged placement where it was asked:
    // a card that kept its size goes straight back, then that answer takes it
    // to the other display again. Look once more after the round trip, until
    // nothing needs to move, a bounded number of times.
    constexpr int SettleRounds = 8;
    if (m_cardStage->returnCardsToDisplay() > 0
        && ++m_cardDisplaySettleRounds < SettleRounds) {
        QTimer::singleShot(250, this, [this] {
            if (!m_cardDisplaySettleQueued) settleCardsOnDisplays();
        });
    }
    // The arrival the person used last is the one they pick up again, so it
    // becomes the Active card and the others wait behind it. The card that was
    // in front stays in the line. Ties go to the higher window in the stack.
    KWin::EffectWindow *resume = nullptr;
    quint64 latest = 0;
    for (const auto &window : std::as_const(arrivals)) {
        if (!window || !m_cardStage->admitArrivalAsCard(window)) continue;
        const quint64 used = m_activationOrder.value(windowIdentity(window));
        if (!resume || used >= latest) {
            resume = window;
            latest = used;
        }
    }
    if (behindLayout) m_cardStage->stopBehindLayoutIfEmpty();
    restackCarriedDecks();
    // A card stacked behind another is reached through its face.
    if (resume) {
        const QList<KWin::EffectWindow *> deck = m_cardStage->stackFrontToBack(resume);
        (void)m_cardStage->promoteToActive(deck.isEmpty() ? resume : deck.first());
    }
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
}

void Effect::restackCarriedDecks()
{
    KWin::VirtualDesktop *desktop = KWin::effects->currentDesktop();
    const auto found = desktop ? m_pendingDecks.find(desktop->id()) : m_pendingDecks.end();
    if (found == m_pendingDecks.end()) return;
    auto &decks = found.value();
    for (auto it = decks.begin(); it != decks.end();) {
        QList<KWin::EffectWindow *> alive;
        for (const auto &window : std::as_const(*it))
            if (window && !window->isDeleted() && window->isOnDesktop(desktop)) alive.append(window);
        if (alive.size() < 2) {
            it = decks.erase(it);
            continue;
        }
        // Until every one of its windows is a card here, it waits.
        if (!std::all_of(alive.cbegin(), alive.cend(), [this](KWin::EffectWindow *window) {
                return m_cardStage->liveCardIndex(window) >= 0; })) {
            ++it;
            continue;
        }
        if (!m_cardStage->stackBehind(alive.first(), alive.mid(1)))
            qInfo() << "Kadunce could not stack again the" << alive.size() << "cards Table carried here";
        it = decks.erase(it);
    }
    if (decks.isEmpty()) m_pendingDecks.erase(found);
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
    // excluded because a swipe from the dock is the bottom swipe. So is the
    // desktop background, also a layer surface, which is all the bezel lies
    // over when no dock is on this display.
    const KWin::Window *window = KWin::input()->findToplevel(position);
    return window && window->inherits("KWin::LayerShellV1Window")
        && !window->isDock() && !window->isDesktop() && !window->isAppletPopup();
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
            // Where the row stands, and where Spread draws the card.
            const int id = m_cardStage->liveCards().indexOf(window) + 1;
            application.insert(QStringLiteral("entry"), m_cardStage->model().entryIndexForId(id));
            auto *tablet = tabletOutput();
            if (tablet && m_cardStage->isActive()
                && m_cardStage->presentation() == CardPresentation::Spread
                && m_cardStage->paintSlot(window) != 99) {
                auto rect = m_cardStage->previewTargetForWindow(tablet, window);
                const auto pose = m_cardStage->stackPoseForWindow(window, rect.width());
                rect.translate(qRound(pose.x), qRound(pose.y));
                application.insert(QStringLiteral("spreadRect"), geometryContext(rect));
            }
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
                ? QStringLiteral("bento")
            : m_cardStage->presentation() == CardPresentation::Desktop
                ? QStringLiteral("desktop") : QStringLiteral("active");

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

// Placement requests (docs/REQUESTS.md). The dock, or any other requester,
// carries an application to a point; Kadunce shows where it would go and puts
// it there when it is let go.
int Effect::placementProtocolVersion() const
{
    return 1;
}

QList<PlacementOutput> Effect::placementOutputs() const
{
    QList<PlacementOutput> outputs;
    for (auto *output : KWin::effects->screens()) {
        if (output) outputs.append({output->name(), QRectF(output->geometry()), m_cardStage->canOwnCards(output)});
    }
    return outputs;
}

PlacementAim Effect::placementAimAt(QPointF point) const
{
    auto aim = placementAim(placementOutputs(), point);
    // CARD-LIFECYCLE.md §3: a side snap on a display Kadunce does not yet own
    // gives an Active card, as the middle does.
    if ((aim.kind == PlacementAimKind::Left || aim.kind == PlacementAimKind::Right)
        && !m_cardStage->isActive()) {
        for (auto *output : KWin::effects->screens()) {
            if (output && output->name() == aim.output && m_cardStage->canOwnCards(output))
                aim.kind = PlacementAimKind::Card;
        }
    }
    return aim;
}

std::optional<KWin::RectF> Effect::placementPreview(const PlacementAim &aim, QPointF point) const
{
    KWin::LogicalOutput *output = nullptr;
    for (auto *candidate : KWin::effects->screens()) {
        if (candidate && candidate->name() == aim.output) output = candidate;
    }
    if (!output) return std::nullopt;
    QRectF area = QRectF(KWin::effects->clientArea(KWin::MaximizeArea, output))
        .intersected(QRectF(output->geometry()));
    area.setBottom(std::min(area.bottom(), double(output->geometry().bottom()) - PlacementDockBand));
    switch (aim.kind) {
    case PlacementAimKind::Card:
    case PlacementAimKind::Top:
        return KWin::RectF(m_cardStage->activeTarget(output));
    case PlacementAimKind::Left:
    case PlacementAimKind::Right: {
        const bool right = aim.kind == PlacementAimKind::Right;
        const auto side = bentoSideChoice(right, point.y(), QRectF(output->geometry()).center().y());
        const double width = area.width() * (side.large ? 2.0 / 3.0 : 1.0 / 3.0);
        return KWin::RectF(right ? area.right() - width : area.left(), area.top(), width, area.height());
    }
    case PlacementAimKind::Display:
        return KWin::RectF(area);
    case PlacementAimKind::None:
        break;
    }
    return std::nullopt;
}

QString Effect::aimPlacement(double x, double y)
{
    const QPointF point(x, y);
    const auto aim = placementAimAt(point);
    const auto preview = aim.kind == PlacementAimKind::None ? std::nullopt : placementPreview(aim, point);
    m_placementAim = aim;
    m_placementPreview = preview;
    m_placementPreviewOutput.clear();
    for (auto *output : KWin::effects->screens()) {
        if (output && preview && output->name() == aim.output) m_placementPreviewOutput = output;
    }
    KWin::effects->addRepaintFull();
    return placementAimName(aim.kind);
}

void Effect::clearPlacementAim()
{
    if (!m_placementAim && !m_placementPreview) return;
    m_placementAim.reset();
    m_placementPreview.reset();
    m_placementPreviewOutput.clear();
    KWin::effects->addRepaintFull();
}

bool Effect::placeApplication(const QStringList &applicationIds, const QString &windowId,
                              double x, double y, const QString &requestToken)
{
    const QPointF point(x, y);
    const auto aim = placementAimAt(point);
    const auto aimed = m_placementAim;
    clearPlacementAim();
    // A request is placed only where it was last aimed, never where a release
    // happens to land.
    if (requestToken.isEmpty() || aim.kind == PlacementAimKind::None || !aimed || *aimed != aim)
        return false;
    QString requested = windowId.trimmed();
    requested.remove(QLatin1Char('{')).remove(QLatin1Char('}'));
    if (requested.isEmpty()) {
        auto launch = PendingLaunch::make(applicationIds, requestToken);
        if (!launch) return false;
        if (m_placementLaunch)
            Q_EMIT placementSettled(m_placementLaunch->token, QString(), false);
        m_placementLaunch = std::move(*launch);
        m_placementLaunchAim = aim;
        m_placementLaunchPoint = point;
        const auto generation = ++m_placementGeneration;
        QTimer::singleShot(PlacementLaunchTimeoutMs, this, [this, generation] {
            if (generation == m_placementGeneration && m_placementLaunch) cancelPlacement(m_placementLaunch->token);
        });
        return true;
    }
    KWin::EffectWindow *window = nullptr;
    for (auto *candidate : KWin::effects->stackingOrder()) {
        if (isApplicationWindow(candidate) && candidate->window()
            && windowIdentity(candidate).compare(requested, Qt::CaseInsensitive) == 0) window = candidate;
    }
    if (!window) return false;
    const bool placed = placeWindow(window, aim, point);
    Q_EMIT placementSettled(requestToken, windowIdentity(window), placed);
    return placed;
}

void Effect::cancelPlacement(const QString &requestToken)
{
    if (!m_placementLaunch || m_placementLaunch->token != requestToken) return;
    ++m_placementGeneration;
    const auto token = m_placementLaunch->token;
    m_placementLaunch.reset();
    Q_EMIT placementSettled(token, QString(), false);
}

void Effect::completePlacementForWindow(KWin::EffectWindow *window)
{
    if (!m_placementLaunch || !isApplicationWindow(window)
        || !m_placementLaunch->matches(applicationIdentity(window))) return;
    const auto token = m_placementLaunch->token;
    const auto aim = m_placementLaunchAim;
    const auto point = m_placementLaunchPoint;
    m_placementLaunch.reset();
    ++m_placementGeneration;
    // The arrival is admitted first, as every arrival is, and placed from there.
    QPointer<KWin::EffectWindow> arrival = window;
    QTimer::singleShot(0, this, [this, arrival, token, aim, point] {
        const bool placed = arrival && !arrival->isDeleted() && placeWindow(arrival, aim, point);
        Q_EMIT placementSettled(token, windowIdentity(arrival), placed);
    });
}

bool Effect::placeWindow(KWin::EffectWindow *window, const PlacementAim &aim, QPointF point)
{
    KWin::LogicalOutput *output = nullptr;
    for (auto *candidate : KWin::effects->screens()) {
        if (candidate && candidate->name() == aim.output) output = candidate;
    }
    if (!output || !window || window->isDeleted() || !window->window()) return false;
    const bool card = m_cardStage->isActive() && m_cardStage->liveCardIndex(window) >= 0;
    if (m_cardStage->canOwnCards(output)) {
        // An ordinary window shown elsewhere comes to the card display as one
        // carried there would, and is then handled as a card already here. A
        // pane keeps its layout.
        if (!card && window->screen() != output) {
            if (m_carriedWindow || m_nativeCarry || m_desktopStage->managesWindow(window)
                || !admitTransferredWindowToTablet(window, [] { return true; })
                || m_cardStage->liveCardIndex(window) < 0) return false;
            (void)m_cardStage->promoteToActive(window);
            observeCardOwnership();
        }
        if (!activateApplicationWindow(windowIdentity(window))) return false;
        if (aim.kind == PlacementAimKind::Card) return true;
        // CARD-LIFECYCLE.md §3, as the Active card carried to that edge.
        const bool right = aim.kind == PlacementAimKind::Right;
        const auto side = bentoSideChoice(right, point.y(), QRectF(output->geometry()).center().y());
        QPointer<KWin::LogicalOutput> destination = output;
        QPointer<KWin::EffectWindow> placed = window;
        QTimer::singleShot(0, this, [this, destination, placed, side, right] {
            if (!destination || !placed || m_cardStage->activeCardIdentity() != placed) return;
            (void)pairActiveCardIntoBento(destination, side, !right);
            observeCardOwnership();
        });
        return true;
    }
    // A card goes to another display as the Active card carried there would.
    // A pane keeps its layout.
    if (card) return placeCardOnDisplay(window, output, aim, point);
    if (m_desktopStage->managesWindow(window)) {
        return aim.kind == PlacementAimKind::Display && window->screen() == output
            && activateApplicationWindow(windowIdentity(window));
    }
    if (aim.kind == PlacementAimKind::Display && window->screen() == output)
        return activateApplicationWindow(windowIdentity(window));
    std::optional<BentoSidePlacement> side;
    if (aim.kind == PlacementAimKind::Left || aim.kind == PlacementAimKind::Right)
        side = bentoSideChoice(aim.kind == PlacementAimKind::Right, point.y(),
                               QRectF(output->geometry()).center().y());
    const auto drop = m_desktopStage->prepareCardDrop(window, output, KWin::RectF(window->frameGeometry()),
        aim.kind == PlacementAimKind::Display ? DesktopStageController::CardDropIntent::OpenSpace
                                              : DesktopStageController::CardDropIntent::ActivateBento, side);
    if (!drop) return false;
    const bool placed = m_desktopStage->transferPreparedCard(*drop, [] { return true; }, [] {});
    observeCardOwnership();
    return placed;
}

bool Effect::placeCardOnDisplay(KWin::EffectWindow *card, KWin::LogicalOutput *output,
    const PlacementAim &aim, QPointF point)
{
    if (!card || !output || m_cardStage->canOwnCards(output) || card->screen() == output
        || m_carriedWindow || m_nativeCarry || m_placementDrop) return false;
    std::optional<BentoSidePlacement> side;
    if (aim.kind == PlacementAimKind::Left || aim.kind == PlacementAimKind::Right)
        side = bentoSideChoice(aim.kind == PlacementAimKind::Right, point.y(),
                               QRectF(output->geometry()).center().y());
    // A layout begins or is joined at an edge; anywhere else the card opens
    // there, at the size it had before it was a card, around the point.
    const auto intent = aim.kind == PlacementAimKind::Display
        ? DesktopStageController::CardDropIntent::OpenSpace
        : DesktopStageController::CardDropIntent::ActivateBento;
    const QRectF area = QRectF(output->geometry());
    const auto landingFor = [&](QSizeF size) {
        size = size.boundedTo(area.size());
        QRectF landing(QPointF(), size);
        landing.moveCenter(point);
        landing.moveLeft(std::clamp(landing.left(), area.left(), area.right() - size.width()));
        landing.moveTop(std::clamp(landing.top(), area.top(), area.bottom() - size.height()));
        return KWin::RectF(landing);
    };
    // The destination accepts before anything about the card changes, so a
    // refusal leaves the card as it was.
    if (!m_desktopStage->prepareCardDrop(card, output, landingFor(QSizeF(card->frameGeometry().width(), card->frameGeometry().height())),
            intent, side)) return false;
    // The card leaves as the Active card does, with the record of where it
    // was before it was a card.
    if (m_cardStage->presentation() != CardPresentation::Active
        || m_cardStage->selectedWindow() != card) (void)m_cardStage->promoteToActive(card);
    const auto source = m_cardStage->prepareNativeCarrySource(card);
    if (!source) return false;
    const KWin::RectF landing = landingFor(QSizeF(source->restoreSnapshot().geometry.width(),
        source->restoreSnapshot().geometry.height()));
    m_placementDrop = m_desktopStage->prepareCardDrop(card, output, landing, intent, side);
    if (!m_placementDrop) return false;
    const bool placed = m_cardStage->transferNativeCarryToDesktop(*source, output, landing);
    m_placementDrop.reset();
    observeCardOwnership();
    return placed;
}

int Effect::launcherGuestProtocolVersion() const
{
    return 3;
}

QString Effect::beginLauncherGuest(const QString &ownerService)
{
    return acceptLauncherGuest(ownerService, QStringLiteral("/Launcher"),
        QStringLiteral("io.github.carlsonjm.Tettegouche"), launcherGuestProtocolVersion(), true);
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
        companionGuestProtocolVersion(), false);
}

QString Effect::acceptLauncherGuest(const QString &ownerService, const QString &objectPath,
    const QString &interfaceName, int protocol, bool openSpread)
{
    QJsonObject reply{
        {QStringLiteral("protocol"), protocol},
        {QStringLiteral("accepted"), false},
    };
    const QString owner = ownerService.trimmed();
    if (!owner.startsWith(QLatin1Char(':'))) {
        return QString::fromUtf8(
            QJsonDocument(reply).toJson(QJsonDocument::Compact));
    }

    // The card that has focus as the guest arrives is where KWin hands focus
    // back as the guest goes.
    if (auto *focused = KWin::effects->activeWindow(); isApplicationWindow(focused)) {
        m_lastActiveApplication = focused;
    }
    if (presentationForInput() != WorkspacePresentation::Spread) {
        if (!openSpread) {
            return QString::fromUtf8(
                QJsonDocument(reply).toJson(QJsonDocument::Compact));
        }
        showCardLine();
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
    if (!m_guestNeighborMotion.isValid() || m_guestNeighborMotion.elapsed() >= motion(220)) return target;
    const double t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
        m_guestNeighborMotion.elapsed() / double(motion(220)));
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
        m_guestFocusReturn = isApplicationWindow(focusReturn)
            ? focusReturn : m_cardStage->selectedWindow();
        const auto generation = m_guestGeneration;
        QTimer::singleShot(1000, this, [this, generation]() {
            if (generation == m_guestGeneration || !m_cardStage->launcherGuestActive())
                m_guestFocusReturn.clear();
        });
        QTimer::singleShot(motion(220), this, [this, generation]() {
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

// The gutters are quiet. Every card and Bento pane
// stands a gutter in from the display's edges and from its neighbours, and an
// application drawing its own title bar keeps an invisible resize border wider
// than that, so in a gutter the pointer was over a resize border and took a
// resize shape. While the pointer is in the gutter around one of Kadunce's own
// cards or panes, Kadunce holds it: a plain arrow, and nothing there takes a
// press. KWin picks the pointer's window before any input filter runs, so the
// hold is the mouse interception KWin gives effects such as Overview.
void Effect::quietCardGap(const QPointF &position)
{
    // An open Table holds the pointer too, as Overview does: it is drawn over
    // the windows rather than being one, so without the hold KWin hands the
    // pointer to the client beneath, which sets its own shape and never hears
    // the pointer move again. While a finger types a name
    // on the keys it lets go: while an effect holds the pointer KWin finds no
    // window under any touch, so the keys would never feel their own taps.
    bool quiet = !(m_tableRenaming >= 0 && m_tableRenameByTouch);
    if (!m_table.isOpen()) {
        // A press, a drag and a move or resize under way keep what they have.
        if (KWin::input()->pointer()->areButtonsPressed() || KWin::waylandServer()->seat()->isDragPointer()
            || KWin::workspace()->moveResizeWindow()) return;
        quiet = inCardGap(position);
    }
    if (quiet == m_gapHeld) return;
    m_gapHeld = quiet;
    if (quiet) KWin::effects->startMouseInterception(this, Qt::ArrowCursor);
    else KWin::effects->stopMouseInterception(this);
}

// Outside every window's frame, but inside the margin a card or pane keeps
// around its frame: the topmost window whose frame or input reaches the
// position decides.
bool Effect::inCardGap(const QPointF &position) const
{
    // A divider between panes lies in their gutter, and its press is the
    // router's: held here, KWin would hand that press to this effect and the
    // divider would never hear it.
    for (const auto &rail : m_desktopStage->grabRails())
        if (rail.hitArea.contains(position)) return false;
    const auto stack = KWin::workspace()->stackingOrder();
    for (auto it = stack.crbegin(); it != stack.crend(); ++it) {
        KWin::Window *window = *it;
        if (!window || window->isDeleted() || !window->isShown() || !window->isOnCurrentDesktop()
            || window->isHiddenByShowDesktop()) continue;
        if (window->frameGeometry().contains(position)) return false;
        if (!window->hitTest(position)) continue;
        const KWin::EffectWindow *effectWindow = window->effectWindow();
        return effectWindow
            && m_ownership.ownerOf(reinterpret_cast<quintptr>(effectWindow)) != CardOwner::Native;
    }
    return false;
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
    if (m_cardStage->cardGrabActive()) takeCarriedDialogs(m_cardStage->selectedWindow());
}

void Effect::updateCardGrab(const QPointF &position)
{
    const auto previousDestination = m_lineDestination;
    m_linePreview.reset();
    m_lineDestination.reset(); m_lineDestinationWindow.clear();
    m_lineCardEntryOutput.clear();
    m_cardStage->updateCardGrab(position);
    // A card held in its own Stack goes nowhere but its Stack.
    if (!m_cardStage->cardGrabActive() || m_cardStage->heldInStack()) return;
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
        // Spread's side edges only move the row under the card: Bento comes
        // from the Active card (CARD-LIFECYCLE.md §3). The top edge still
        // makes the carried card Active (§10).
        if (m_cardStage->canOwnCards(output) && edge != CarryEdge::Top) continue;
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

void Effect::finishCardGrab(bool commit)
{
    m_cardStage->finishCardGrab(commit);
    m_lineDestination.reset(); m_lineDestinationWindow.clear();
    m_lineCardEntryOutput.clear();
    if (m_dialogsLead) m_carriedDialogsRelease.start();
}

bool Effect::finishCardGrabOnOutput(const QPointF &position)
{
    if (m_dialogsLead) m_carriedDialogsRelease.start();
    // A release cannot invent a destination that was never evaluated in motion.
    if (position != m_lineDestinationContact) {
        m_lineDestination.reset();
        m_lineCardEntryOutput.clear();
    }
    // A card held in its own Stack is let go into it, wherever the finger is.
    if (m_cardStage->heldInStack()) return false;
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
    // §10: the top edge's action commits only when released inside its zone,
    // and a release inside it is that action whether or not it was admitted.
    // Neither branch above claimed this one, so handing it to the ordinary
    // Spread drop would place the card where the person did not aim, which
    // §14 forbids a refused gesture from doing. Cancelling the grab restores
    // the membership the card was lifted from. Spread's side edges have no
    // action of their own: a release there is an ordinary drop.
    if (auto *edgeOutput = KWin::effects->screenAt(position.toPoint());
        edgeOutput && QRectF(edgeOutput->geometry()).contains(position)
        && m_cardStage->canOwnCards(edgeOutput) && !isPanelPoint(position)
        && monitorCarryEdge(QRectF(edgeOutput->geometry()), position) == CarryEdge::Top) {
        m_cardStage->finishCardGrab(false);
        m_lineDestinationWindow.clear();
        return true;
    }
    const bool result = m_cardStage->finishCardGrabOnOutput(position);
    m_lineDestination.reset(); m_lineDestinationWindow.clear();
    return result;
}


bool Effect::cardAtForInput(const QPointF &position) const
{
    return m_cardStage->cardAtPoint(position);
}

void Effect::tapSpreadFromInput(const QPointF &position)
{
    if (m_cardStage->tapSpread(position) == CardStageController::SpreadTap::Open)
        activateSelectedFromInput();
}

void Effect::syncSelectedElevation()
{
    m_cardStage->syncSelectedElevation();
}

void Effect::holdOverviewOff()
{
    // KDE's Overview claims three fingers up and down, four on a touchpad,
    // Meta+W, Meta+G and the top-left corner; unloading it gives them all up,
    // and the destructor loads it again.
    if (!KWin::effects->isEffectLoaded(OverviewEffect)) return;
    KWin::effects->unloadEffect(OverviewEffect);
    m_overviewHeld = true;
    qInfo() << "Kadunce" << Revision << "set KDE's Overview aside while it runs";
}

Effect::SpreadGesture::Mode Effect::beginSpreadGesture()
{
    using Mode = SpreadGesture::Mode;
    if (m_cardStage->launcherGuestActive() || m_cardStage->cardGrabActive() || m_table.isOpen())
        return Mode::Refused;
    if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread)
        return Mode::Refused;
    // From the Active card the row forms under the fingers; from a layout or
    // the desktop the row has to be made first, so it opens past halfway.
    return m_cardStage->beginOpenSpread() ? Mode::Follow : Mode::Commit;
}

void Effect::followSpreadGesture(qreal progress)
{
    using Mode = SpreadGesture::Mode;
    auto &gesture = m_spreadGesture;
    if (gesture.mode == Mode::Idle) gesture.mode = beginSpreadGesture();
    gesture.progress = std::clamp(double(progress), 0.0, 1.0);
    if (gesture.mode == Mode::Follow) {
        m_cardStage->followOpenSpread(gesture.progress);
    } else if (gesture.mode == Mode::Commit && gesture.progress > 0.5) {
        gesture.mode = Mode::Opened;
        toggle();
    }
}

void Effect::finishSpreadGesture()
{
    // Past halfway Spread opens; short of it the card springs back.
    if (m_spreadGesture.mode == SpreadGesture::Mode::Follow)
        m_cardStage->finishOpenSpread(m_spreadGesture.progress > 0.5);
    m_spreadGesture = {};
}

void Effect::setPagingShortcutsActive(bool active)
{
    m_cardsShownForKeys = active;
}

bool Effect::answerKey(int key, Qt::KeyboardModifiers modifiers, bool repeat)
{
    KeyContext context;
    context.table = true;
    context.cardsShown = m_cardsShownForKeys;
    // Plain keys go to an effect holding the keyboard, as Table does while open.
    context.spreadShown = context.cardsShown && !KWin::effects->hasKeyboardGrab()
        && m_cardStage->presentation() == CardPresentation::Spread
        && !m_cardStage->launcherGuestActive() && !m_cardStage->cardGrabActive();
    const KeyAction action = keyActionFor(key, modifiers, context);
    if (action == KeyAction::None || (repeat && !keyActionRepeats(action))) return false;
    // Open, Table keeps every key but its own.
    if (m_table.isOpen() && action != KeyAction::Table) return false;
    // The action runs after the key's own handling, never inside an input
    // filter's callback.
    QTimer::singleShot(0, this, [this, action]() { runKeyAction(action); });
    return true;
}

void Effect::runKeyAction(KeyAction action)
{
    switch (action) {
    case KeyAction::Spread: toggle(); break;
    case KeyAction::Previous: pageLeft(); break;
    case KeyAction::Next: pageRight(); break;
    case KeyAction::StackPrevious: pageStackUp(); break;
    case KeyAction::StackNext: pageStackDown(); break;
    case KeyAction::Bento: toggleBento(); break;
    case KeyAction::ZoneMode: switchZoneMode(); break;
    case KeyAction::Release: release(); break;
    case KeyAction::Open: activateSelectedFromInput(); break;
    case KeyAction::Back:
        if (m_cardStage->backFromSpread()) activateSelectedFromInput();
        break;
    case KeyAction::Table: toggleTable(); break;
    case KeyAction::None: break;
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
                } else if (m_paneArrivalWindow == requested) {
                    // A card let go on a pane grows into it from where it was.
                    startPaneArrival(requested, tabletOutput(), m_paneArrivalFrom);
                }
            } else if (requested->isMinimized()) {
                // §2: selecting a sleeping card wakes it and presents it as
                // Active, as picking it from the dock does.
                KWin::effects->activateWindow(requested);
            } else {
                toggleOwnedPresentation(true);
            }
        }
        m_paneArrivalWindow.clear();
    });
}

void Effect::openGroupAfterDropForCardStage(const QRectF &from)
{
    // CARD-LIFECYCLE.md §5: a card let go on a pane takes it, and the group
    // opens as its layout, as choosing it in Spread does.
    m_paneArrivalWindow = selectedWindow();
    m_paneArrivalFrom = from;
    activateSelectedFromInput();
}

void Effect::startPaneArrival(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                              const QRectF &from)
{
    if (!window || window->isDeleted() || !window->window() || !output || !from.isValid()
        || window->window()->moveResizeOutput() != output) return;
    const QRectF to(window->window()->moveResizeGeometry());
    if (to.isEmpty() || to == from) return;
    // The window is already on its pane, sized once; only its picture travels,
    // replacing whatever motion the layout gave it. It waits where it was let
    // go for its client to draw the pane's shape, a short while at most, and
    // then moves the whole way (PaneArrival.h).
    for (auto it = m_bentoMotions.begin(); it != m_bentoMotions.end();) {
        if (it->window != window) { ++it; continue; }
        unredirect(window);
        it = m_bentoMotions.erase(it);
    }
    BentoMotion arrival{window, output, from, to, QRectF(output->geometry())};
    arrival.arrival = true;
    arrival.asked.start();
    m_bentoMotions.append(arrival);
    KWin::effects->addRepaintFull();
}

void Effect::toggle()
{
    toggleOwnedPresentation();
}

void Effect::toggleOwnedPresentation(bool growToActive)
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
    // From the desktop, Spread opens whatever is selected in it.
    if (m_cardStage->selectedIsBentoGroup()
        && m_cardStage->presentation() != CardPresentation::Desktop) {
        if (!m_cardStage->resumeSelectedBentoProjection()) {
            qWarning() << "Kadunce" << Revision
                       << "declined to resume the selected Bento group";
        }
        observeCardOwnership();
        return;
    }
    m_cardStage->toggle(growToActive);
    observeCardOwnership();
}

void Effect::release()
{
    // Windows given back here may come back from minimized; that is no pick.
    const bool wasReleasing = std::exchange(m_releasing, true);
    const auto settled = qScopeGuard([this, wasReleasing] { m_releasing = wasReleasing; });
    if (m_carryRuntime) m_carryRuntime->cancel();
    // §13 returns every managed window, whichever desktop holds it.
    bool hadBento = false;
    bool hadCards = false;
    forEachSession([&] {
        if (hasActiveDesktopStage()) {
            hadBento = true;
            m_desktopStage->stopPendingSettle();
            m_desktopStage->restoreAllSessions();
        }
        hadCards = hadCards || m_cardStage->isActive();
        m_cardStage->release();
    });
    returnDependentWindows();
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
    if (m_tablePresenter && m_tablePresenter->visible() && data.screen == m_tablePresenterOutput)
        m_tablePresenter->prePaint(data.frame);
    // A released row, a returning card or a thrown one moves once per frame.
    if (isTabletOutput(data.screen)) m_cardStage->advanceMotion();
    showSleepingCardsInSpread();
    // Presentation changes repaint, so a frame is where a dependent's
    // application is first seen to come to the front or leave it.
    for (const auto &window : std::as_const(m_dependents)) {
        if (!window || window->isDeleted() || !window->window()) continue;
        const bool shown = dependentShown(dependentLead(window));
        if ((!shown && !window->window()->isHidden())
            || (shown && m_heldDependents.contains(window))) {
            scheduleDependentSync();
            break;
        }
    }
    if (!m_heldDependents.isEmpty() || !m_drawnDialogs.empty()) updateDrawnDialogs();
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
        && m_guestNeighborMotion.elapsed() < motion(220)) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        m_continueRepaint = true;
    }
    for (auto it = m_bentoMotions.begin(); it != m_bentoMotions.end();) {
        if (!bentoMotionRect(it->window)) {
            if (it->window && !it->window->isDeleted()) unredirect(it->window);
            it = m_bentoMotions.erase(it);
        } else {
            // An arrival moves once its client draws the pane's shape or has
            // had long enough to; this frame draws it at its start, and its
            // clock runs from when the frame is done (postPaintScreen).
            if (it->arrival && !it->drawnMoving && it->output == data.screen) {
                const auto frame = it->window->frameGeometry();
                it->drawnMoving = !paneArrivalWaits(
                    paneArrivalShaped(frame.width(), frame.height(), it->to.width(), it->to.height()),
                    it->asked.elapsed());
            }
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
    for (auto &m : m_bentoMotions)
        if (m.drawnMoving && !m.timer.isValid()) m.timer.start();
    KWin::effects->postPaintScreen();
    if (continueRepaint) {
        KWin::effects->addRepaintFull();
    }
}

// CARD-LIFECYCLE.md §2: a sleeping card is chosen in Spread to wake it, so
// Spread keeps it drawn, dimmed, from the last frame its window showed. KWin
// paints no minimized window unless an effect holds it visible.
void Effect::showSleepingCardsInSpread()
{
    const bool spread = m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Spread;
    for (auto it = m_sleepingShown.begin(); it != m_sleepingShown.end();) {
        KWin::EffectWindow *window = it.key();
        if (spread && m_cardStage->liveCardIndex(window) >= 0 && window->isMinimized()) ++it;
        else it = m_sleepingShown.erase(it);
    }
    if (!spread) return;
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        if (!window || window->isDeleted() || !window->isMinimized()
            || m_sleepingShown.contains(window) || m_cardStage->liveCardIndex(window) < 0) continue;
        m_sleepingShown.insert(window, KWin::EffectWindowVisibleRef(window, KWin::EffectWindow::PAINT_DISABLED_BY_MINIMIZE));
    }
}

void Effect::prePaintWindow(KWin::RenderView *view,
                            KWin::EffectWindow *window,
                            KWin::WindowPrePaintData &data)
{
    if (window == m_carriedWindow || (window == m_settlingWindow && dropSettleRect()) || bentoMotionRect(window)) {
        data.setTransformed(); data.setTranslucent();
    }
    // Left undrawn, so it must not hide what is under it from the scene.
    if (m_previewDesktop ? hiddenByDesktopPreview(window) : hiddenOnOtherDesktop(window)) data.setTranslucent();
    // Drawn with its card, not where it stands, so it hides nothing there.
    if (m_drawnDialogs.contains(window) || drawnWithCarriedCard(window)) data.setTranslucent();
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

void Effect::drawDestinationOutline(const KWin::RenderTarget &renderTarget,
    const KWin::RenderViewport &viewport, const KWin::Region &deviceRegion,
    KWin::LogicalOutput *screen, const QRectF &box)
{
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
        m_destinationShader->setUniform("outlineRadius", float(CardCornerRadius));
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
}

PaintResult Effect::paintScreen(const KWin::RenderTarget &renderTarget,
                         const KWin::RenderViewport &viewport,
                         int mask,
                         const KWin::Region &deviceRegion,
                         KWin::LogicalOutput *screen)
{
    m_paintingOutput = screen;
    m_projectionBackdropDrawn = false;
    if (screen && screen == tabletOutput()) m_cardLabelTargets.clear();
    if (!painted([&] { return KWin::effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen); })) return paintResult(false);
    // Labels, rails and outlines belong to the current desktop's presentation.
    if (m_previewDesktop) return paintResult(true);
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
            m_destinationShader->setUniform("outlineRadius", pill ? 2.f : float(CardCornerRadius));
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
    const auto cardExit = m_carriedWindow ? m_carryCardExitOutput : QPointer<KWin::LogicalOutput>();
    if (m_destinationShader && screen && preview
        && (m_carriedWindow || m_cardStage->cardGrabActive())
        && (cardEntry == screen || (cardExit && cardExit == screen)
            || (reservation && !cardEntry && reservation->showsPlacementOutline()
                && reservation->destinationOutput() == screen
                && m_desktopStage->cardDropValid(*reservation)))) {
        const QRectF box(*preview);
        drawDestinationOutline(renderTarget, viewport, deviceRegion, screen, box);
        // The outline above is drawn for a Bento reservation, a Card Stage
        // entry or a card leaving at the bottom edge. §10 gives the bottom edge
        // the only detaching release, so a pane leaving its layout and a card
        // leaving its display are labelled, and an entry never is.
        if ((cardExit && cardExit == screen)
            || (reservation && !cardEntry && reservation->detachesToDesktop()))
            m_detachLabel.render(renderTarget, viewport, box);
    }
    // Where a placement request is aimed, as a carried card's destination is.
    if (m_destinationShader && screen && m_placementPreview && m_placementPreviewOutput == screen)
        drawDestinationOutline(renderTarget, viewport, deviceRegion, screen, QRectF(*m_placementPreview));
    if (screen && screen == tabletOutput() && m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Spread) {
        const auto &model = m_cardStage->model();
        const auto &windows = m_cardStage->liveCards();
        // A held card is above the row, so no name of the row shows through it.
        auto *held = m_cardStage->cardGrabActive() ? m_cardStage->selectedWindow() : nullptr;
        const QRectF heldRect = held ? m_cardLabelTargets.value(held) : QRectF();
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
            if (window != held && !heldRect.isEmpty()
                && CardLabelRenderer::labelRect(*target).intersects(heldRect)) continue;
            m_cardLabelRenderer.render(renderTarget, viewport,
                *target,
                {applicationName, stackPositionLabel(
                    m_cardStage->shownStackPosition(cardId), count, bentoGroup)});
        }
    }
    m_paintingOutput = nullptr;
    return paintResult(true);
}

void Effect::observeCardOwnership()
{
    // One ledger over every desktop: a window has one owner across all of
    // them. A display has at most one layout per desktop, so another desktop's
    // layout is named with its desktop.
    std::vector<quintptr> cards;
    std::vector<BentoOwnershipView> sessions;
    // A card two desktops both hold is still one kind of owner to the ledger,
    // so it is caught here.
    std::vector<OwnershipViolation> reported;
    QSet<quintptr> held;
    for (const auto &entry : m_sessions) {
        const DesktopSession *session = entry.second.get();
        for (const auto &window : session->cards->liveCards()) {
            if (!window) continue;
            const auto identity = reinterpret_cast<quintptr>(window.data());
            if (held.contains(identity))
                reported.push_back({identity, OwnershipViolation::Rule::TwoOwners,
                                    QStringLiteral("desktop ") + entry.first});
            held.insert(identity);
            cards.push_back(identity);
        }
        for (auto view : session->layouts->ownershipView()) {
            if (session != m_currentSession) view.output += QStringLiteral(" on desktop ") + entry.first;
            sessions.push_back(std::move(view));
        }
    }

    // Every window whose owner changed is put to the ledger as a transition.
    // The ledger accepts only the six the contract defines, so a change it
    // refuses is a change §14 does not permit, reported from this one place.
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
        if (m_ownershipViolationLog.size() < 64)
            m_ownershipViolationLog.append(describeOwnershipViolation(violation));
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
    syncDesktopPreview();
    if (isDependentWindow(window)) {
        // Anything that activates it before the next turn is the application
        // opening it, not the person asking for it.
        const QPointer<KWin::EffectWindow> fresh(window);
        m_dependents.append(fresh);
        m_freshDependents.append(fresh);
        QTimer::singleShot(0, this, [this, fresh] { m_freshDependents.removeAll(fresh); });
        scheduleDependentSync();
        return;
    }
    connectManagedWindow(window);
    const QPointer<KWin::EffectWindow> candidate(window);
    const auto admitReadyWindow = [this, candidate]() {
        // A window that opens on a desktop not shown is not a card here; that
        // desktop takes it when it is next shown.
        if (!candidate || candidate->isDeleted() || !isCardWindow(candidate)
            || !candidate->window() || !candidate->window()->readyForPainting()) {
            return;
        }
        if (m_desktopStage->handleWindowAdded(candidate)) {
            observeCardOwnership();
            completePlacementForWindow(candidate);
            return;
        }
        // §3: a display that can own cards and holds none takes the arrival
        // as its Active card, with every other window there as a card.
        if (!m_cardStage->isActive() && startTabletInCards(candidate)) {
            completePlacementForWindow(candidate);
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
        completePlacementForWindow(candidate);
    };
    QTimer::singleShot(0, this, admitReadyWindow);
    if (window->window() && !window->window()->readyForPainting())
        connect(window->window(), &KWin::Window::readyForPaintingChanged,
                this, admitReadyWindow, Qt::SingleShotConnection);
}

void Effect::handleWindowClosed(KWin::EffectWindow *window)
{
    if (m_dependents.contains(window)) {
        m_dependents.removeAll(window);
        m_heldDependents.removeAll(window);
        m_freshDependents.removeAll(window);
        scheduleDependentSync();
    }
    // Let go before KWin destroys it; the hold names the window it holds.
    m_drawnDialogs.erase(window);
    if (window == m_dialogsLead) releaseCarriedDialogs();
    m_applicationDisplayNames.remove(window);
    m_cardLabelTargets.remove(window);
    m_previewRefs.remove(window);
    // A destroyed window leaves ownership without a transition: it no longer
    // exists to own, and its identity may be reused by the next allocation.
    m_ownership.forget(reinterpret_cast<quintptr>(window));
    if (window == m_settlingWindow) clearDropSettle();
    if (window == m_carriedWindow && m_carryRuntime) m_carryRuntime->cancel();
    m_activationOrder.remove(windowIdentity(window));
    // The window may be held by a desktop that is not shown.
    forEachSession([&] {
        m_desktopStage->handleWindowClosed(window);
        m_cardStage->handleWindowClosed(window);
    });
    if (m_table.isOpen()) {
        m_tableWorkspaces = tableWorkspaces();
        relayoutTable();
        refreshTable();
    }
    scheduleDissolve();
    Q_EMIT workspaceContextChanged();
}

void Effect::handleWindowActivated(KWin::EffectWindow *window)
{
    if (m_holdingDependents) return;
    if (auto *lead = isDependentWindow(window) ? dependentLead(window) : nullptr) {
        if (!dependentShown(lead)) {
            const bool opening = m_freshDependents.contains(window) || !m_dependents.contains(window);
            // The person asked for it through the dock, the task switcher or
            // a waiting row, so its application comes forward with it on top.
            if (!opening && !admitActivatedCardToLiveBento(lead)) m_cardStage->handleWindowActivated(lead);
        }
        scheduleDependentSync();
        return;
    }
    if (m_settlingWindow && window != m_settlingWindow) clearDropSettle();
    if (m_carriedWindow && window != m_carriedWindow && m_carryRuntime) m_carryRuntime->cancel();
    if (isApplicationWindow(window) && m_guestFocusReturn) {
        const bool restored = window == m_guestFocusReturn;
        m_guestFocusReturn.clear();
        if (restored) {
            qInfo() << "Kadunce" << Revision << "kept Spread as focus returned to"
                    << window->caption();
            return;
        }
    }
    const QPointer<KWin::EffectWindow> focusedBefore = m_lastActiveApplication;
    if (isApplicationWindow(window)) {
        m_lastActiveApplication = window;
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
        // A guest gives up the keyboard as it hides, and KWin hands focus back
        // to the card that held it before. That is the guest going, not a
        // request to open the card.
        if (window == focusedBefore) {
            qInfo() << "Kadunce" << Revision << "kept Spread as focus returned to"
                    << window->caption();
            return;
        }
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
    callLauncherGuestOwner(QStringLiteral("completeGuestLaunch"), {m_launcherGuestLaunchToken});
    QTimer::singleShot(motion(220), this, [this, generation]() {
        if (generation != m_guestGeneration) return;
        endLauncherGuest();
    });
    return true;
}

void Effect::handleActiveGeometryChanged(KWin::EffectWindow *window,
                                         const KWin::RectF &)
{
    DesktopSession *holder = sessionHolding(window);
    SessionScope scope(this, holder ? holder : m_currentSession);
    m_cardStage->handleActiveGeometryChanged(window);
}

void Effect::handleSessionStateChanged()
{
    bool owning = false;
    forEachSession([&] { owning = owning || m_cardStage->isActive() || hasActiveDesktopStage(); });
    if (owning && KWin::effects->sessionState() != KWin::SessionState::Normal) {
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

PaintResult Effect::drawWindow(const KWin::RenderTarget &renderTarget,
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
        if (!painted([&] { return KWin::OffscreenEffect::drawWindow(
            renderTarget, viewport, window, mask, deviceRegion, data); })) return paintResult(false);
        return paintResult(true);
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

    const bool drawn = painted([&] { return KWin::OffscreenEffect::drawWindow(
        renderTarget, viewport, window, mask, deviceRegion, data); });
    KWin::ShaderManager::instance()->popShader();
    if (!drawn) return paintResult(false);
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
                if (!painted([&] { return KWin::OffscreenEffect::drawWindow(renderTarget, viewport, neighbor,
                    PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT,
                    KWin::Region(), preparation); })) return paintResult(false);
            }
        }
    }
    return paintResult(true);
}

PaintResult Effect::paintWindow(const KWin::RenderTarget &renderTarget,
                         const KWin::RenderViewport &viewport,
                         KWin::EffectWindow *window,
                         int mask,
                         const KWin::Region &deviceRegion,
                         KWin::WindowPaintData &data)
{
    // Hidden with its application even before KWin is told, so a dialog that
    // opens behind the card in front is never seen for a frame.
    if (isDependentWindow(window) && !dependentShown(dependentLead(window))) return paintResult(true);
    // Drawn with its card in hand, after the card.
    if (drawnWithCarriedCard(window)) return paintResult(true);
    // Keys are drawn only once they are known to be the person's.
    if (!m_keysForPerson && window == KWin::effects->inputPanel()) return paintResult(true);
    // A card flicked closed stays out of sight while its app closes.
    if (m_cardStage->thrownAway(window)) return paintResult(true);
    // A previewed desktop is drawn as it stands, with nothing of the current
    // desktop's presentation over or under it.
    if (m_previewDesktop) {
        if (!hiddenByDesktopPreview(window))
            if (!painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window, mask, deviceRegion, data); })) return paintResult(false);
        return paintResult(true);
    }
    if (hiddenOnOtherDesktop(window)) return paintResult(true);
    // A sleeping card stands in Spread dimmed, so it can be found and woken.
    if (m_sleepingShown.contains(window)) data.multiplyOpacity(SleepingCardOpacity);
    if (guestNeighborOpacity() <= 0.0 && m_cardStage->launcherGuestActive()
        && m_paintingOutput == tabletOutput() && isCardWindow(window)
        && m_cardStage->paintSlot(window) != 99) return paintResult(true);
    const auto settle = window == m_settlingWindow ? dropSettleRect() : bentoMotionRect(window);
    if (settle && window != m_settlingWindow && m_paintingOutput
        && window->window()->moveResizeOutput() != m_paintingOutput) return paintResult(true);
    if (((window == m_carriedWindow && m_carryRuntime) || settle) && m_paintingOutput) {
        const auto plan = settle
            ? carryPaintPlan(*settle, settle->topLeft(), QRectF(m_paintingOutput->geometry()))
            : carryPaintPlan(m_carryPickup, m_carryRuntime->handoff.carry().position(),
                             QRectF(m_paintingOutput->geometry()));
        if (!plan || plan->clip.isEmpty()) return paintResult(true);
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
        const bool drawn = painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window, mask | PAINT_WINDOW_TRANSFORMED, clip, data); });
        m_fanApertureWindow = nullptr; m_fanPaintSize = {}; m_fanApertureOrigin = {};
        m_fanApertureSize = {}; m_fanApertureRadius = 0;
        if (!drawn) return paintResult(false);
        if (window == m_dialogsLead)
            return paintResult(paintDialogsOn(renderTarget, viewport, window, carriedDialogs(), clip, data));
        return paintResult(true);
    }
    // A Bento pane is the desktop stage's to paint. Card Stage hides what it
    // owns, and a pane is not one of its cards, so it must not be routed here.
    // While the desktop shows, what Card Stage does not own is drawn as it
    // stands, and its cards are held aside where KWin draws nothing.
    if (!m_cardStage->isActive() || !m_paintingOutput
        || !(isCardWindow(window) || m_sleepingShown.contains(window))
        || m_desktopStage->managesWindow(window)
        || (m_cardStage->presentation() == CardPresentation::Desktop
            && m_cardStage->liveCardIndex(window) < 0)) {
        if (!painted([&] { return KWin::effects->paintWindow(
            renderTarget, viewport, window, mask, deviceRegion, data); })) return paintResult(false);
        return paintResult(true);
    }

    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet) {
        if (!painted([&] { return KWin::effects->paintWindow(
            renderTarget, viewport, window, mask, deviceRegion, data); })) return paintResult(false);
        return paintResult(true);
    }

    const int slot = m_cardStage->paintSlot(window);
    const bool grabbedWindow = m_cardStage->cardGrabActive()
        && window == m_cardStage->selectedWindow();
    // Resting on a pane of the Bento group, the held card slides under it.
    const auto heldTuck = grabbedWindow ? m_cardStage->heldTuck() : std::nullopt;
    const auto route = cardPaintRoute(m_paintingOutput == tablet,
        window->screen() == tablet, slot != 99, window == m_nativeCarry,
        grabbedWindow);
    if (route == CardPaintRoute::Hidden) return paintResult(true);
    if (route == CardPaintRoute::Native) {
        // Only the carried item may cross the fence. Passive tablet neighbors
        // remain hidden externally; the carrier is clipped per output.
        const auto outputClip = viewport.mapToDeviceCoordinatesAligned(m_paintingOutput->geometry());
        if (!painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window, mask,
            window == m_nativeCarry ? deviceRegion & KWin::Region(outputClip) : deviceRegion, data); })) return paintResult(false);
        return paintResult(true);
    }

    if (m_cardStage->presentation() == CardPresentation::Active) {
        // While the keys type into it, the card ends a gutter above them on
        // every frame, whatever size its client has drawn yet.
        if (const auto edge = m_cardStage->keyboardRoomEdge(window)) {
            const KWin::RectF output = m_paintingOutput->geometry();
            const KWin::RectF above(output.x(), output.y(), output.width(),
                                    std::max(0.0, *edge - output.y()));
            if (!painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window, mask,
                deviceRegion & KWin::Region(viewport.mapToDeviceCoordinatesAligned(above)), data); })) return paintResult(false);
            return paintResult(true);
        }
        if (!painted([&] { return KWin::effects->paintWindow(
            renderTarget, viewport, window, mask, deviceRegion, data); })) return paintResult(false);
        return paintResult(true);
    }

    // A card held in its own Stack is lifted out of it; the place it would
    // take is an outline at the Stack's seam, just below the held card. Let
    // go, the outline fades under the card then in front.
    if (window == m_cardStage->selectedWindow() && m_paintingOutput == tablet && m_destinationShader) {
        const auto outline = m_cardStage->stackOutline();
        if (!outline.rect.isEmpty() && outline.opacity > 0.0) {
            paintCardSurface(m_destinationShader.get(), renderTarget, viewport,
                deviceRegion & KWin::Region(viewport.mapToDeviceCoordinatesAligned(tablet->geometry())),
                QRectF(outline.rect), 0.6, 0.0f, float(outline.opacity));
        }
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
        // It takes the pane's shape as it goes (CARD-LIFECYCLE.md §5).
        if (heldTuck) {
            const auto blended = heldTuckBlend(
                {double(target.x()), double(target.y()), double(target.width()), double(target.height())},
                {double(heldTuck->tucked.x()), double(heldTuck->tucked.y()),
                 double(heldTuck->tucked.width()), double(heldTuck->tucked.height())},
                heldTuck->progress);
            target = KWin::Rect(qRound(blended.x), qRound(blended.y),
                                qRound(blended.width), qRound(blended.height));
        }
    } else {
        target.translate(qRound(paintPose.x), qRound(paintPose.y));
    }
    const double poseOpacity = m_cardStage->applyPoseTransition(window, target, paintPose);
    data.multiplyOpacity(poseOpacity * m_cardStage->liftOpacity(window));
    if (m_cardStage->launcherGuestActive() && m_paintingOutput == tablet) {
        const double opacity = guestNeighborOpacity();
        data.multiplyOpacity(opacity);
        const int direction = target.center().x() < tablet->geometry().center().x() ? -1 : 1;
        target.translate(qRound(direction * (1.0 - opacity) * target.width() * 0.25), 0);
    }
    paintPose.rotation += launcherGuestRotation;
    if (heldTuck) paintPose.rotation += heldTuck->rotation * heldTuck->progress;
    if (!paintPose.visible) {
        return paintResult(true);
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
        if (!pane) return paintResult(true);
        visualTarget = KWin::Rect(qRound(pane->targetSurface.x),
            qRound(pane->targetSurface.y), qRound(pane->targetSurface.width),
            qRound(pane->targetSurface.height));
        projectionPaneClip = KWin::Rect(qRound(pane->targetClip.x),
            qRound(pane->targetClip.y), qRound(pane->targetClip.width),
            qRound(pane->targetClip.height));
        // The pane a held card would take gives way to a cutout, so the card
        // can slide under the group in its place (CARD-LIFECYCLE.md §5).
        const double recess = m_cardStage->carryPaneRecess(window);
        if (recess > 0.0) data.multiplyOpacity(1.0 - recess);
    } else if (bentoProjection) {
        return paintResult(true); // CARD-LIFECYCLE.md §7: a sleeping group member is owned and
                // minimized, so the group shows its panes and not this window.
    }
    const bool rotatedFanCard = !qFuzzyIsNull(paintPose.rotation);
    // The group's backdrop goes under its panes, so the first pane drawn on
    // this display draws it. KWin draws raised windows last, so that is not
    // always the lowest pane in its stacking order.
    const bool paintProjectionBackdrop = bentoProjection && !m_projectionBackdropDrawn;
    if (paintProjectionBackdrop) m_projectionBackdropDrawn = true;
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

    bool drawn = true;
    if (heldTuck) {
        // Under the group the card shows only through the cutout and past the
        // group's edge; the rest of it fades as it slides beneath.
        const KWin::Region group(viewport.mapToDeviceCoordinatesAligned(heldTuck->group));
        const KWin::Region hole(viewport.mapToDeviceCoordinatesAligned(heldTuck->pane));
        const double under = heldTuckUnder(heldTuck->progress);
        const KWin::Region over = cardClip & group.subtracted(hole);
        if (under < 1.0 && !over.isEmpty()) {
            KWin::WindowPaintData fading(data);
            fading.multiplyOpacity(1.0 - under);
            drawn = painted([&] { return KWin::effects->paintWindow(
                renderTarget, viewport, window, mask | PAINT_WINDOW_TRANSFORMED, over, fading); });
        }
        drawn = drawn && painted([&] { return KWin::effects->paintWindow(
            renderTarget, viewport, window, mask | PAINT_WINDOW_TRANSFORMED,
            cardClip & outputFence.subtracted(group).united(hole), data); });
    } else {
        drawn = painted([&] { return KWin::effects->paintWindow(
            renderTarget, viewport, window, mask | PAINT_WINDOW_TRANSFORMED,
            cardClip, data); });
    }

    m_fanApertureWindow = nullptr;
    m_fanPaintSize = {};
    m_fanApertureOrigin = {};
    m_fanApertureSize = {};
    m_fanApertureRadius = 0.0F;
    if (!drawn) return paintResult(false);

    // A dialog waiting with this application shows on its card.
    return paintResult(paintDialogsOn(renderTarget, viewport, window, drawnDialogsOf(window), cardClip, data));
}

} // namespace Kadunce
