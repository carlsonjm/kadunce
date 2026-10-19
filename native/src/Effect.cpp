/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Effect.h"
#include "EffectShared.h"
#include "LogName.h"
#include "MotionTime.h"
#include <QScopeGuard>
#include <fstream>
#include <string>
#include "LaunchIdentity.h"
#include "PendingLaunch.h"
#include "PlacementAim.h"
#include "SpreadLayout.h"
#include "DisplayHandoffPolicy.h"
#include "CarryPaintPlan.h"
#include "PaneArrival.h"
#include "NativeLanding.h"
#include "MonitorDropIntent.h"
#include "DeliberateEdgeEntry.h"
#include "TouchDisplay.h"
#include "DesktopRemoval.h"
#include "StuckNotesWatcher.h"
#include "SurfaceTone.h"
#include "KWinMatch.h"
#include "DisplayBounds.h"
#include <QCoreApplication>
#include <cursor.h>
#include <cursorsource.h>
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
#include <scene/item.h>
#include <scene/itemrenderer.h>
#include <scene/scene.h>
#include <scene/workspacescene.h>
#include <window.h>
#include <workspace.h>
#include <virtualdesktops.h>
#include <wayland_server.h>
#include <wayland/seat.h>
#include <wayland/surface.h>
#include <wayland/textinput_v3.h>

#include <KConfigGroup>
#include <KSharedConfig>
#include <KService>
#include <KSharedConfig>

#include <QAction>
#include <QKeyEvent>
#include <QLineF>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QEasingCurve>
#include <QJsonArray>
#include <chrono>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLoggingCategory>
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
// Whether a window still stands where a motion takes it. At a fractional
// scale KWin may land a frame part of a pixel off the rect it was asked for,
// and that is still the motion's place.
bool landsAt(const QRectF &geometry, const QRectF &to)
{
    return std::abs(geometry.left() - to.left()) < 1.0 && std::abs(geometry.top() - to.top()) < 1.0
        && std::abs(geometry.right() - to.right()) < 1.0 && std::abs(geometry.bottom() - to.bottom()) < 1.0;
}
}

// The version this build is, as the journal names it (CMakeLists.txt).
#ifndef KADUNCE_REVISION
#define KADUNCE_REVISION "unversioned"
#endif

namespace
{
constexpr auto Revision = KADUNCE_REVISION;
// Off unless asked for (TESTING.md § Confirming the running build): what the
// pointer is over near a display's top edge, read on every motion there.
Q_LOGGING_CATEGORY(KaduncePointer, "kadunce.pointer", QtWarningMsg)
constexpr double CardCornerRadius = 10.0;
constexpr qint64 FrameStallMs = 50;
// How far the Active card must travel under a KWin move before the cards let go.
constexpr double ActiveMoveTravel = 12.0;
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
uniform vec3 outlineInk;
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
    vec4 color = vec4(outlineInk * edgeAlpha, edgeAlpha) + fill * (1.0 - edgeAlpha);
    fragColor = nitsToDestinationEncoding(sourceEncodingToNitsInDestinationColorspace(color));
}
)GLSL";
// The keyboard's window outlined: 2 px of the accent just inside the window's
// own edge, so its contents stay where they are and nothing is left behind as
// it moves. This one follows the paper corner Kadunce trims a window to.
constexpr auto KeyboardOutlineFragment = R"GLSL(#version 140
in vec2 point;
out vec4 fragColor;
uniform vec4 destinationBox;
uniform float outlineRadius;
uniform vec3 outlineInk;
uniform float outlineOpacity;
#include "colormanagement.glsl"
void main() {
    vec2 halfSize = destinationBox.zw * 0.5;
    float radius = min(outlineRadius, min(halfSize.x, halfSize.y));
    vec2 q = abs(point - destinationBox.xy - halfSize) - (halfSize - vec2(radius));
    float d = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
    float feather = max(fwidth(d), 0.5);
    float inside = 1.0 - smoothstep(-feather, 0.0, d);
    float accent = smoothstep(-2.0 - feather, -2.0, d) * inside;
    vec4 color = vec4(outlineInk, 1.0) * accent * outlineOpacity;
    fragColor = nitsToDestinationEncoding(sourceEncodingToNitsInDestinationColorspace(color));
}
)GLSL";
// The same outline on a window that keeps its own corners, or none: drawn
// with the window's own picture, along the edge of what it paints, so it
// follows whatever corner the application draws. A pixel is on the band when
// the picture ends within 2 px of it; translucency the window has throughout
// is not an edge.
constexpr auto KeyboardEdgeVertex = R"GLSL(#version 140
in vec4 position;
in vec4 texcoord;
uniform mat4 modelViewProjectionMatrix;
out vec2 texcoord0;
void main() {
    gl_Position = modelViewProjectionMatrix * position;
    texcoord0 = texcoord.st;
}
)GLSL";
constexpr auto KeyboardEdgeFragment = R"GLSL(#version 140
in vec2 texcoord0;
out vec4 fragColor;
#include "colormanagement.glsl"
#include "saturation.glsl"
uniform sampler2D sampler;
uniform vec4 modulation;
uniform vec3 outlineInk;
uniform float outlineOpacity;
uniform float bandTexels;
uniform float frameMargin;
uniform float cornerReach;
float outsideAt(vec2 uv, float centre) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return 1.0;
    float a = texture(sampler, uv).a;
    return 1.0 - smoothstep(0.25, 0.75, a / max(centre, 0.001));
}
void main() {
    vec4 tex = texture(sampler, texcoord0);
    vec2 size = vec2(textureSize(sampler, 0));
    vec2 texel = 1.0 / size;
    // The edge lies along the frame, and only bends away from it in a corner,
    // so the rest of the window is not searched.
    vec2 p = texcoord0 * size;
    vec2 fromFrame = min(p, size - p) - vec2(frameMargin);
    bool nearEdge = min(fromFrame.x, fromFrame.y) <= bandTexels * 1.4 + 1.0
        || max(fromFrame.x, fromFrame.y) <= cornerReach;
    float band = 0.0;
    for (int i = 0; i < 12 && nearEdge; ++i) {
        float angle = float(i) * 0.5235988;
        vec2 step = vec2(cos(angle), sin(angle)) * texel * bandTexels;
        band = max(band, max(outsideAt(texcoord0 + step * 0.5, tex.a), outsideAt(texcoord0 + step, tex.a)));
        band = max(band, 0.5 * outsideAt(texcoord0 + step * 1.35, tex.a));
    }
    band *= smoothstep(0.3, 0.6, tex.a) * outlineOpacity;
    tex = sourceEncodingToNitsInDestinationColorspace(tex);
    tex = adjustSaturation(tex);
    vec4 ink = sourceEncodingToNitsInDestinationColorspace(vec4(outlineInk, 1.0)) * tex.a;
    tex = mix(tex, ink, band);
    tex *= modulation;
    fragColor = nitsToDestinationEncoding(tex);
}
)GLSL";
// The keyboard outline fades in over the Visual sheet's micro transition.
constexpr int KeyboardOutlineFadeMs = 140;
// The shape a window's border asks the pointer for. KWin 6.8 no longer
// exposes it, and there it reads as none.
template<typename Window>
QByteArray borderCursorName(Window *window)
{
    if constexpr (requires { window->cursor().name(); }) return window->cursor().name();
    else return QByteArray();
}

// The renderer that draws a view's scene. From KWin 6.8 each render device
// has its own; earlier KWin has one.
template<typename SceneT, typename View>
KWin::ItemRenderer *sceneRenderer(SceneT *scene, View *view)
{
    if constexpr (requires { scene->renderer(view->renderDevice()); }) return scene->renderer(view->renderDevice());
    else return scene->renderer();
}

// Draws the pointer again over what Kadunce painted after the scene, so no
// label, note or outline ever covers it. KWin draws the pointer inside the
// scene when it has no cursor plane, such as with a software cursor, and then
// anything an effect paints after the scene lands on top of it. The pointer
// is drawn again only where Kadunce painted, `overlaid` in logical pixels, and
// only when KWin composited it into this display's scene, so a pointer on its
// own hardware plane, or one nothing covered, is never doubled. False when
// drawing failed and painting is to stop.
bool paintPointerAbove(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                       const KWin::Region &deviceRegion, KWin::LogicalOutput *screen,
                       const QList<QRectF> &overlaid)
{
    if (overlaid.isEmpty()) return true;
    auto *scene = KWin::kwinApp() ? KWin::kwinApp()->scene() : nullptr;
    KWin::Item *pointer = scene ? scene->cursorItem() : nullptr;
    if (!screen || !pointer || !pointer->isVisible()) return true;
    const KWin::RectF bounds = pointer->mapToScene(pointer->boundingRect());
    if (bounds.isEmpty() || !QRectF(bounds).intersects(QRectF(screen->geometry()))) return true;
    KWin::Region covered;
    for (const QRectF &rect : overlaid) {
        const QRectF under = rect.intersected(QRectF(bounds));
        if (!under.isEmpty()) covered |= KWin::Region(viewport.mapToDeviceCoordinatesAligned(KWin::RectF(under)));
    }
    if (covered.isEmpty()) return true;
    KWin::SceneView *drawnIn = nullptr;
    for (KWin::RenderView *view : scene->views()) {
        auto *sceneView = qobject_cast<KWin::SceneView *>(view);
        if (!sceneView || sceneView->logicalOutput() != screen) continue;
        // On its own plane for any view of this display: not in the scene.
        if (!sceneView->shouldRenderItem(pointer)) return true;
        if (!drawnIn) drawnIn = sceneView;
    }
    if (!drawnIn) return true;
    KWin::ItemRenderer *renderer = sceneRenderer(scene, drawnIn);
    if (!renderer) return true;
    const KWin::Region region = deviceRegion & covered;
    if (region.isEmpty()) return true;
    return painted([&] {
        return renderer->renderItem(renderTarget, viewport, pointer, KWin::Scene::PAINT_SCREEN_TRANSFORMED,
            region, KWin::WindowPaintData{},
            [drawnIn](KWin::Item *item) { return !drawnIn->shouldRenderItem(item); },
            [](KWin::Item *) { return false; });
    });
}

// Card backing and vacant seam share one rigid transform and neutral material.
void paintCardSurface(KWin::GLShader *shader, const KWin::RenderTarget &renderTarget,
    const KWin::RenderViewport &viewport, const KWin::Region &clip,
    const QRectF &box, double angle, float opacity, float outline,
    const QVector3D &fillColor = SurfaceTone::current().cardBacking(),
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
    // Card surfaces draw no ring; this keeps the shared shader's ink defined.
    shader->setUniform("outlineInk", QVector3D(0.88F, 0.88F, 0.88F));
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
// The separately installed Rounded Corners effect trims windows itself.
const QString RoundedCornersEffect = QStringLiteral("kwin4_effect_shapecorners");
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

namespace {
// The display KWin lists under this name; none when it lists none.
KWin::LogicalOutput *screenNamed(const QString &name)
{
    if (name.isEmpty()) return nullptr;
    KWin::LogicalOutput *output = KWin::effects->findScreen(name);
    return output && output->name() == name ? output : nullptr;
}

// The cards behind a card on its stage: its Spread Stack, or the Stack behind
// it as a pane.
QList<KWin::EffectWindow *> cardsBehind(const CardStageController &stage, KWin::EffectWindow *card)
{
    QList<KWin::EffectWindow *> behind = stage.paneStack(card);
    for (auto *member : stage.stackFrontToBack(card))
        if (member != card && !behind.contains(member)) behind.append(member);
    return behind;
}
}

// One display's card stage on one desktop, its host declared first so it
// outlives the stage.
struct Effect::DisplayStage {
    std::unique_ptr<StageHost> host;
    std::unique_ptr<CardStageController> cards;
};

// One virtual desktop's ownership session. Its host is declared first so it
// outlives the stages that call it.
struct Effect::DesktopSession {
    QString desktopId;
    QPointer<KWin::VirtualDesktop> desktop;
    std::unique_ptr<SessionHost> host;
    std::unique_ptr<DesktopStageController> layouts;
    // CARD-LIFECYCLE.md §11: each display's own card stage on this desktop,
    // by stage key (Effect::stageKeyFor).
    std::map<QString, std::unique_ptr<DisplayStage>> displays;

    // The stage holding the window as one of its cards, if any does.
    [[nodiscard]] CardStageController *stageHolding(const KWin::EffectWindow *window) const
    {
        if (!window) return nullptr;
        for (const auto &display : displays)
            if (display.second->cards->liveCardIndex(window) >= 0) return display.second->cards.get();
        return nullptr;
    }
    [[nodiscard]] std::optional<QString> keyHolding(const KWin::EffectWindow *window) const
    {
        if (!window) return std::nullopt;
        for (const auto &display : displays)
            if (display.second->cards->liveCardIndex(window) >= 0) return display.first;
        return std::nullopt;
    }
};

// Names one session's stage on one display for the length of a call. Leaving
// hands back the session and display named before, or the current desktop's
// when that was the one named, since a desktop switch inside the call makes
// another desktop current. Without a display, the one named now is kept.
class Effect::SessionScope
{
public:
    SessionScope(Effect *effect, DesktopSession *session, std::optional<QString> key = std::nullopt)
        : m_effect(effect)
        , m_previous(effect->scopedToCurrent() ? nullptr : effect->m_scopedSession)
        , m_previousKey(effect->m_scopedKey)
    {
        ++effect->m_scopeDepth;
        effect->scopeTo(session, key ? *key : effect->m_scopedKey);
    }
    ~SessionScope()
    {
        --m_effect->m_scopeDepth;
        m_effect->scopeTo(m_previous ? m_previous : m_effect->m_currentSession, m_previousKey);
    }
    SessionScope(const SessionScope &) = delete;
    SessionScope &operator=(const SessionScope &) = delete;

private:
    Effect *m_effect;
    DesktopSession *m_previous;
    QString m_previousKey;
};

// Every host call a card stage makes is answered for that stage's own desktop
// and display, so a stage never reads another desktop's windows, or another
// display's, as its own: inside the call, the effect's m_cardStage is this
// stage and tabletOutput() is its display. The effect keeps the answers.
class Effect::StageHost final : public CardStageHost
{
public:
    StageHost(Effect *effect, DesktopSession *session, const QString &key)
        : m_effect(effect)
        , m_session(session)
        , m_key(key)
    {
    }

    KWin::LogicalOutput *tabletOutputForCardStage() const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().tabletOutputForCardStage();
    }
    bool isTabletOutputForCardStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().isTabletOutputForCardStage(output);
    }
    bool isManagedWindowForCardStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().isManagedWindowForCardStage(window);
    }
    bool mayHoldWindowForCardStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().mayHoldWindowForCardStage(window);
    }
    std::optional<double> inputPanelTopForCardStage(KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().inputPanelTopForCardStage(output);
    }
    bool keyboardTypesIntoForCardStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().keyboardTypesIntoForCardStage(window);
    }
    KWin::RectF workAreaForCardStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().workAreaForCardStage(output);
    }
    void setPagingShortcutsForCardStage(bool active) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        cards().setPagingShortcutsForCardStage(active);
    }
    void cancelInputForCardStage() override
    {
        SessionScope scope(m_effect, m_session, m_key);
        cards().cancelInputForCardStage();
    }
    // A card whose application asked a question as it was flicked closed
    // opens with it. The opening waits a turn of the event loop and then acts
    // only if the shown desktop's stage still has that card selected.
    void presentSelectedForCardStage() override
    {
        SessionScope scope(m_effect, m_session, m_key);
        cards().presentSelectedForCardStage();
    }
    // Each stage hears the gutter change and lays out its own.
    void cardGutterChangedForCardStage() override
    {
        SessionScope scope(m_effect, m_session, m_key);
        cards().cardGutterChangedForCardStage();
    }
    void connectManagedWindowForCardStage(KWin::EffectWindow *window) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        cards().connectManagedWindowForCardStage(window);
    }
    void unredirectForCardStage(KWin::EffectWindow *window) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        cards().unredirectForCardStage(window);
    }
    bool holdPictureBeforeResizeForCardStage(KWin::EffectWindow *window,
        const std::function<void()> &resize) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().holdPictureBeforeResizeForCardStage(window, resize);
    }
    void retireBentoProjectionForCardStage(const QList<QPointer<KWin::EffectWindow>> &windows) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        cards().retireBentoProjectionForCardStage(windows);
    }
    bool connectedForCardStage() const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().connectedForCardStage();
    }
    bool layoutLeavesRoomForCardStage() const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().layoutLeavesRoomForCardStage();
    }
    bool holdsOwnCardsForCardStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().holdsOwnCardsForCardStage(output);
    }
    bool moveCardToDisplayForCardStage(KWin::EffectWindow *card, KWin::LogicalOutput *to) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().moveCardToDisplayForCardStage(card, to);
    }
    KWin::LogicalOutput *releaseDisplayForCardStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().releaseDisplayForCardStage(window);
    }
    bool admitCardToDesktopStage(KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const KWin::RectF &geometry, const std::function<bool()> &commitSource,
        const std::function<void()> &releaseSource) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().admitCardToDesktopStage(window, output, geometry, commitSource, releaseSource);
    }
    bool resumeBentoProjectionForCardStage(const BentoProjectionSession &projection,
        const std::function<bool()> &commitSource,
        const std::function<void()> &releaseSource) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().resumeBentoProjectionForCardStage(projection, commitSource, releaseSource);
    }

private:
    CardStageHost &cards() const { return *m_effect; }

    Effect *m_effect;
    DesktopSession *m_session;
    QString m_key;
};

// The layouts of one desktop span every display, so each host call is
// answered for the display it names: there, that display's card stage is the
// one holding cards beside its layout. A call naming none is answered for the
// display named now.
class Effect::SessionHost final : public DesktopStageHost
{
public:
    SessionHost(Effect *effect, DesktopSession *session)
        : m_effect(effect)
        , m_session(session)
    {
    }

    bool isTabletOutputForDesktopStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().isTabletOutputForDesktopStage(output);
    }
    bool usesDrawnZonesForDesktopStage() const override
    {
        return layouts().usesDrawnZonesForDesktopStage();
    }
    bool allowsDesktopStageOnOutput(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
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
    bool connectedOutputForDesktopStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().connectedOutputForDesktopStage(output);
    }
    long long lastUsedForDesktopStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().lastUsedForDesktopStage(window);
    }
    bool holdsOwnCardsForDesktopStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().holdsOwnCardsForDesktopStage(output);
    }
    bool showsOneCardForDesktopStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().showsOneCardForDesktopStage(output);
    }
    bool releasesOnOwnDisplayForDesktopStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().releasesOnOwnDisplayForDesktopStage(output);
    }
    bool outputCanOwnCards(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().outputCanOwnCards(output);
    }
    KWin::Rect activeTargetForDesktopStage(KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().activeTargetForDesktopStage(output);
    }
    void prepareOutputForDesktopStage(KWin::LogicalOutput *output) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        layouts().prepareOutputForDesktopStage(output);
    }
    void retireOutputFromDesktopStage(KWin::LogicalOutput *output) override
    {
        SessionScope scope(m_effect, m_session, on(output));
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
        SessionScope scope(m_effect, m_session, on(output));
        layouts().animateBentoLayout(output, windows, from, to);
    }
    void paneYieldedForDesktopStage(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                                    const QRectF &drawn) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        layouts().paneYieldedForDesktopStage(window, output, drawn);
    }
    void lastPaneLeftLayoutForDesktopStage(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                                           const QRectF &drawn) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        layouts().lastPaneLeftLayoutForDesktopStage(window, output, drawn);
    }
    void layoutLeftForOtherDisplayForDesktopStage(KWin::LogicalOutput *output) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        layouts().layoutLeftForOtherDisplayForDesktopStage(output);
    }
    void displacedCardMayReturnForDesktopStage(KWin::EffectWindow *window,
                                               KWin::LogicalOutput *output) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        layouts().displacedCardMayReturnForDesktopStage(window, output);
    }
    // The record is the one the stage holding the window keeps.
    std::optional<NativeMoveSnapshot> activeRestoreForDesktopStage(KWin::EffectWindow *window) const override
    {
        const auto held = m_session->keyHolding(window);
        SessionScope scope(m_effect, m_session,
            held ? held : std::optional<QString>(on(window ? window->screen() : nullptr)));
        return layouts().activeRestoreForDesktopStage(window);
    }
    bool admitDisplacedPaneToTablet(KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore = nullptr) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().admitDisplacedPaneToTablet(window, output, commitSource, restore);
    }
    bool admitSleepingPaneToTablet(KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore = nullptr) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().admitSleepingPaneToTablet(window, output, commitSource, restore);
    }
    bool admitTransferredWindowToTablet(KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore = nullptr) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().admitTransferredWindowToTablet(window, output, commitSource, restore);
    }

private:
    DesktopStageHost &layouts() const { return *m_effect; }
    // The stage key of the display named, or of the display named now.
    QString on(const KWin::LogicalOutput *output) const
    {
        return output ? m_effect->stageKeyFor(output) : m_effect->m_scopedKey;
    }

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

template<typename F>
void Effect::forEachStage(F &&f)
{
    // Every desktop's stage on every display, each named in turn.
    std::vector<std::pair<DesktopSession *, QString>> stages;
    for (const auto &entry : m_sessions)
        for (const auto &display : entry.second->displays) stages.emplace_back(entry.second.get(), display.first);
    for (const auto &[session, key] : stages) {
        SessionScope scope(this, session, key);
        f();
    }
}

template<typename F>
void Effect::forEachDisplayStage(F &&f)
{
    // The scoped desktop's stage on every display, each named in turn.
    if (!m_scopedSession) return;
    DesktopSession *session = m_scopedSession;
    QStringList keys;
    for (const auto &display : session->displays) keys.append(display.first);
    for (const QString &key : std::as_const(keys)) {
        SessionScope scope(this, session, key);
        f(*m_cardStage);
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
        slot->layouts = std::make_unique<DesktopStageController>(
            static_cast<DesktopStageHost *>(slot->host.get()));
    }
    return slot.get();
}

CardStageController &Effect::stageOn(DesktopSession *session, const QString &key)
{
    auto &slot = session->displays[key];
    if (!slot) {
        slot = std::make_unique<DisplayStage>();
        slot->host = std::make_unique<StageHost>(this, session, key);
        slot->cards = std::make_unique<CardStageController>(static_cast<CardStageHost *>(slot->host.get()));
    }
    return *slot->cards;
}

Effect::DesktopSession *Effect::sessionHolding(const KWin::EffectWindow *window) const
{
    if (!window) return nullptr;
    auto *held = const_cast<KWin::EffectWindow *>(window);
    for (const auto &entry : m_sessions)
        if (entry.second->stageHolding(window) || entry.second->layouts->managesWindow(held))
            return entry.second.get();
    return nullptr;
}

CardStageController *Effect::stageHolding(const KWin::EffectWindow *window) const
{
    return m_scopedSession ? m_scopedSession->stageHolding(window) : nullptr;
}

CardStageController *Effect::stageKeepingStackOf(const KWin::EffectWindow *window) const
{
    // The stage holding the window as a card, else the one keeping the Stack
    // behind it as a pane.
    if (!m_scopedSession || !window) return nullptr;
    if (CardStageController *held = m_scopedSession->stageHolding(window)) return held;
    for (const auto &display : m_scopedSession->displays)
        if (!display.second->cards->paneStack(window).isEmpty()) return display.second->cards.get();
    return nullptr;
}

bool Effect::anySpreadShown() const
{
    if (!m_scopedSession) return false;
    for (const auto &display : m_scopedSession->displays)
        if (display.second->cards->isActive()
            && display.second->cards->presentation() == CardPresentation::Spread) return true;
    return false;
}

bool Effect::anyCardStageActive() const
{
    if (m_cardStage && m_cardStage->isActive()) return true;
    if (!m_currentSession) return false;
    for (const auto &display : m_currentSession->displays)
        if (display.second->cards->isActive()) return true;
    return false;
}

bool Effect::roundsCorners() const
{
    return m_fanApertureShader && m_fanPaintSizeLocation >= 0 && m_fanApertureOriginLocation >= 0
        && m_fanApertureSizeLocation >= 0 && m_fanApertureRadiusLocation >= 0
        && !KWin::effects->isEffectLoaded(RoundedCornersEffect);
}

QString Effect::stageKeyFor(const KWin::LogicalOutput *output) const
{
    // One display holding cards keeps one stage, which moves with it. A
    // display holding none has no stage of its own.
    if (!m_cardsOnEveryDisplay) return QString();
    return output && isCardDisplay(output) ? output->name() : m_scopedKey;
}

QString Effect::stageKeyOf(const KWin::EffectWindow *window) const
{
    // The stage holding it, else the one of the display it stands on.
    if (m_scopedSession)
        if (const auto held = m_scopedSession->keyHolding(window)) return *held;
    return stageKeyFor(window ? window->screen() : nullptr);
}

void Effect::scopeTo(DesktopSession *session, const QString &key)
{
    // Every display holding its own cards keeps its stage under its name;
    // none is kept twice, once by name and once as the primary display.
    const QString named = !m_cardsOnEveryDisplay ? QString() : key.isEmpty() ? m_cardOutputName : key;
    m_scopedSession = session;
    m_scopedKey = named;
    m_cardStage = session ? &stageOn(session, named) : nullptr;
    m_desktopStage = session ? session->layouts.get() : nullptr;
}

void Effect::noteDisplayInUse(const KWin::LogicalOutput *output)
{
    // What names no display of its own, a key or a timer, acts on the display
    // in use: the one last touched, or holding the window last focused. It is
    // taken up on the next turn, so a call under way keeps the display it began on.
    if (!output || !m_cardsOnEveryDisplay || !isCardDisplay(output)) return;
    const QString key = stageKeyFor(output);
    if (key == m_inUseKey) return;
    m_inUseKey = key;
    QTimer::singleShot(0, this, [this] { restInUse(); });
}

void Effect::enterInputScope(const QPointF &position, InputReach reach, qint64 contact)
{
    // A contact KWin no longer holds lifted where the router did not see it.
    if (reach == InputReach::Press && contact >= 0)
        if (const auto down = touchesDownForInput())
            m_strokeContacts.removeIf([&](qint64 held) {
                return held >= 0 && held != contact && !down->contains(qint32(held));
            });
    const bool stroking = !m_strokeContacts.isEmpty();
    QString key = stroking ? m_strokeKey : m_scopedKey;
    if (!stroking && (reach == InputReach::Press || reach == InputReach::Within))
        if (KWin::LogicalOutput *at = KWin::effects->screenAt(position.toPoint()); isCardDisplay(at))
            key = stageKeyFor(at);
    if (reach == InputReach::Press) {
        if (!stroking) m_strokeKey = key;
        m_strokeContacts.insert(contact);
        noteDisplayInUse(screenNamed(key));
    }
    m_inputScopes.push_back(std::make_unique<SessionScope>(this, m_currentSession, key));
}

void Effect::leaveInputScope(InputReach reach, qint64 contact)
{
    if (!m_inputScopes.empty()) m_inputScopes.pop_back();
    if (reach == InputReach::Release) m_strokeContacts.remove(contact);
    if (reach == InputReach::Cancel) m_strokeContacts.removeIf([](qint64 held) { return held >= 0; });
}

QString Effect::inUseKey() const
{
    if (!m_cardsOnEveryDisplay) return QString();
    return isCardDisplay(screenNamed(m_inUseKey)) ? m_inUseKey : m_cardOutputName;
}

bool Effect::isInUseDisplay(const KWin::LogicalOutput *output) const
{
    return isCardDisplay(output) && stageKeyFor(output) == inUseKey();
}

void Effect::restInUse()
{
    if (m_scopeDepth > 0) return;
    const QString key = inUseKey();
    if (key != m_scopedKey || m_scopedSession != m_currentSession) scopeTo(m_currentSession, key);
}


Effect::Effect()
{
    connect(KWin::effects, &KWin::EffectsHandler::windowDeleted, this,
        [this](KWin::EffectWindow *window) {
            m_previewSourceBounds.remove(window);
            m_previewRefs.remove(window);
            m_drawnAs.remove(window);
            releaseHeldPicture(window);
            m_offerPictures.erase(window);
            if (m_capturingHeld == window) m_capturingHeld = nullptr;
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
    {
        const KConfigGroup spread = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Spread"));
        m_spreadChoice.display = spread.readEntry(QStringLiteral("Display"), QString());
        m_spreadChoice.identity = spread.readEntry(QStringLiteral("DisplayIdentity"), QString());
        m_spreadChoice.withoutTouchscreen = spread.readEntry(QStringLiteral("WithoutTouchscreen"), true);
        // Every display holds its own cards; "windows" keeps the displays
        // other than Spread's to ordinary windows and layouts.
        m_cardsOnEveryDisplay = spread.readEntry(QStringLiteral("OtherDisplays"), QStringLiteral("cards"))
            .trimmed().toLower() != QStringLiteral("windows");
        const KConfigGroup sizing = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Sizing"));
        for (const QString &output : sizing.keyList()) {
            const QString chosen = sizing.readEntry(output, QString());
            if (parseSizing(chosen)) m_sizingChoices.insert(output, chosen);
        }
        const KConfigGroup sizingDisplays = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))
            ->group(QStringLiteral("SizingDisplays"));
        for (const QString &output : sizingDisplays.keyList()) {
            const QString identity = sizingDisplays.readEntry(output, QString());
            if (!identity.isEmpty()) m_sizingIdentities.insert(output, identity);
        }
    }
    m_usesDrawnZones = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Bento"))
        .readEntry(QStringLiteral("UseDrawnZones"), false);
    m_showsKeyboardOutline = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Windows"))
        .readEntry(QStringLiteral("KeyboardOutline"), true);
    // A window coming to the keyboard repaints, so its outline moves with it.
    connect(KWin::effects, &KWin::EffectsHandler::windowActivated, this, [this] {
        if (m_showsKeyboardOutline) KWin::effects->addRepaintFull();
    });
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
        m_keyboardOutlineShader = KWin::ShaderManager::instance()->generateCustomShader(
            KWin::ShaderTrait::UniformColor, QByteArray(DestinationVertex), QByteArray(KeyboardOutlineFragment));
        m_keyboardEdgeShader = KWin::ShaderManager::instance()->generateCustomShader(
            KWin::ShaderTrait::MapTexture, QByteArray(KeyboardEdgeVertex), QByteArray(KeyboardEdgeFragment));
        m_frostedBox.create();
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
        },
        [this](int key) { keyReleased(key); });

    // Three fingers down on the touchscreen, or four on a touchpad, open Spread
    // under the fingers. KDE's Overview, which claims the same moves, is set
    // aside while Kadunce runs.
    m_spreadGestureAction = new QAction(this);
    KWin::effects->registerTouchscreenSwipeShortcut(
        KWin::SwipeDirection::Down, 3, m_spreadGestureAction,
        [this](qreal progress) { followSpreadGesture(progress); });
    KWin::effects->registerTouchpadSwipeShortcut(
        KWin::SwipeDirection::Down, 4, m_spreadGestureAction, [this](qreal progress) {
            followSpreadGestureOn(progress, KWin::effects->screenAt(KWin::effects->cursorPos().toPoint()));
        });
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
        [this](qreal progress) {
            KWin::LogicalOutput *touched = screenNamed(m_touchOutputName);
            followTableGesture(progress, touched ? touched : tabletOutput());
        });
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

    // A display driven by a touchscreen selects Kadunce's richer direct
    // four-edge router, which alone pulls Table down from the top edge.
    // Without one, top and bottom gesture recognition is delegated to
    // Plasma/KWin and only the resulting semantic action arrives here. The
    // router is told which backend owns those edges so one swipe cannot be
    // consumed or toggled twice.
    m_usesDirectSystemEdges = !m_touchOutputName.isEmpty();
    if (!m_usesDirectSystemEdges) {
        // Plasma's bottom touch edge opens Spread, as the bezel does on the
        // tablet, and so do three fingers on any touchscreen.
        m_showSpreadAction = new QAction(tr("Show Spread"), this);
        m_showSpreadAction->setObjectName(
            QStringLiteral("Kadunce Show Spread"));
        connect(m_showSpreadAction, &QAction::triggered,
                this, &Effect::showCardLine);
        KWin::effects->registerTouchBorder(
            KWin::ElectricBottom, m_showSpreadAction);

        m_showActiveAction = new QAction(tr("Show Active card"), this);
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
    connect(KWin::VirtualDesktopManager::self(), &KWin::VirtualDesktopManager::desktopAdded, this,
            [this](KWin::VirtualDesktop *desktop) {
                if (desktop) m_unenteredDesktops.insert(desktop->id());
            });
    connect(KWin::effects, &KWin::EffectsHandler::sessionStateChanged,
            this, &Effect::handleSessionStateChanged);
    connect(KWin::effects, &KWin::EffectsHandler::screenRemoved,
            this, &Effect::handleScreenRemoved);
    connect(KWin::effects, &KWin::EffectsHandler::screenAdded, this,
            [this](KWin::LogicalOutput *output) {
                m_displaysChanged.start();
                forEachSession([&] { m_desktopStage->handleScreenAdded(output); });
                refreshCardOutput();
                scheduleCardDisplaySettle();
            });
    // A display that moves, as the tablet does when a monitor is arranged
    // beside it, is a layout KWin may put windows back into as well.
    connect(KWin::effects, &KWin::EffectsHandler::virtualScreenGeometryChanged, this, [this] {
        m_displaysChanged.start();
        scheduleCardDisplaySettle();
    });
    // A panel took its room, gave it up or took it back. KWin announces the
    // rearrangement before making it, so the card is placed on the next turn.
    m_keysWorkAreaRelease.setSingleShot(true);
    m_keysWorkAreaRelease.setInterval(1000);
    connect(&m_keysWorkAreaRelease, &QTimer::timeout, this, &Effect::releaseKeysWorkArea);
    // A card let go lands, and settles, within this; its dialogs follow it
    // there and are then left where they stand.
    m_paneStackDwell.setSingleShot(true);
    connect(&m_paneStackDwell, &QTimer::timeout, this, [this] {
        if (m_carriedWindow) updateNativeCarryDestination(m_lastCarryContact);
    });
    m_cardBookSettle.setSingleShot(true);
    m_cardBookSettle.setInterval(250);
    connect(&m_cardBookSettle, &QTimer::timeout, this, &Effect::readCardBook);
    m_carriedDialogsRelease.setSingleShot(true);
    m_carriedDialogsRelease.setInterval(1000);
    // A notice said only in the workspace context has no frame to end it.
    m_contextNoticeEnd.setSingleShot(true);
    m_contextNoticeEnd.setInterval(GestureNoteTiming::TotalMs + 20);
    connect(&m_contextNoticeEnd, &QTimer::timeout, this, [this] {
        if (m_zoneNotice && m_zoneNotice->shown.elapsed() >= GestureNoteTiming::TotalMs) m_zoneNotice.reset();
        if (m_gestureNote && m_gestureNote->shown.elapsed() >= GestureNoteTiming::TotalMs) m_gestureNote.reset();
        Q_EMIT workspaceContextChanged();
        KWin::effects->addRepaintFull();
    });
    connect(&m_carriedDialogsRelease, &QTimer::timeout, this, &Effect::releaseCarriedDialogs);
    connect(KWin::workspace(), &KWin::Workspace::aboutToRearrange, this, [this]() {
        if (m_workAreaCheckQueued) return;
        m_workAreaCheckQueued = true;
        QTimer::singleShot(0, this, [this]() {
            m_workAreaCheckQueued = false;
            followKeysWorkArea();
            forEachDisplayStage([](CardStageController &stage) { stage.followWorkArea(); });
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
                    forEachDisplayStage([&](CardStageController &stage) {
                        stage.repaintKeyboardEdge(old, panel->frameGeometry());
                        stage.refreshKeyboardRoom();
                    });
                });
        }
        forEachDisplayStage([](CardStageController &stage) { stage.refreshKeyboardRoom(); });
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
                [this]() {
                    forEachDisplayStage([](CardStageController &stage) { stage.keepKeyboardRoomPlacement(); });
                });
        for (const auto signal : {&KWin::InputMethod::visibleChanged,
                                  &KWin::InputMethod::activeWindowChanged}) {
            connect(method, signal, this,
                    [this]() {
                        forEachDisplayStage([](CardStageController &stage) { stage.refreshKeyboardRoom(); });
                    });
        }
    }

    m_carryRuntime = std::make_unique<NativeCarryRuntime>();
    m_roomOfferInput = std::make_unique<RoomOfferInput>();
    m_roomOfferInput->pictureAt = [this](QPointF p) { return roomOfferPictureAt(p); };
    m_roomOfferInput->hovered = [this](int at) {
        if (!m_roomOffer || m_roomOffer->hovered == at) return;
        m_roomOffer->hovered = at;
        KWin::effects->addRepaintFull();
    };
    m_roomOfferInput->picked = [this](int at) {
        qInfo() << "Kadunce" << Revision << "picked offered card" << at;
        pickFromRoomOffer(at);
    };
    m_roomOfferInput->dismissed = [this] {
        qInfo() << "Kadunce" << Revision << "leaves the offer: Escape or a press away from its pictures";
        leaveRoomOffer();
    };
    m_carryRuntime->observed = [this](KWin::Window *w, const char *event) {
        traceNativeMove(w ? w->effectWindow() : m_carriedWindow.data(), event);
    };
    m_carryRuntime->adopted = [this](KWin::Window *w) {
        leaveRoomOffer();
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
        // Carried again, it goes where this carry puts it; the top edge marks it
        // alone again when it lets go there.
        m_aloneOnPurpose.removeAll(m_carriedWindow);
        m_carryOrigin = w->output() ? w->output()->name() : QString();
        m_carryDrawn = carriedPose();
        m_carryLanded = false;
        traceNativeMove(m_carriedWindow, "adopted");
        syncNotesPause();
        takeCarriedDialogs(m_carriedWindow);
        KWin::effects->setElevatedWindow(m_carriedWindow, true);
        // §11, Connected: a pane holding a Stack, held before it is drawn up,
        // reorders its Stack instead of being carried.
        endStackScrub();
        m_stackScrubArmed = m_desktopStage->managesWindow(m_carriedWindow)
            && !m_cardStage->paneStack(m_carriedWindow).isEmpty();
        if (m_stackScrubArmed) m_stackScrubPane = m_carriedWindow;
        // A title bar held still until KWin's own drag time shows the ladder
        // at once, so the hold is seen before the hand moves.
        if (m_stackScrubArmed) {
            const auto held = m_carryRuntime->observer.contacts().held(m_carryRuntime->route.owner());
            if (held && std::hypot(held->travel.x(), held->travel.y()) < StackScrubSlop
                && std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - held->pressedAt).count() >= StackScrubHoldMs) {
                beginStackScrub(held->pressedPosition.y());
                m_stackScrubHeldStill = m_stackScrub.active();
                refreshStackScrubShown();
            }
        }
        KWin::effects->addRepaintFull();
    };
    m_carryRuntime->moved = [this](QPointF p) {
        m_carryDrawn = carriedPose();
        if (followStackScrub(p)) return;
        updateNativeCarryDestination(p);
    };
    m_carryRuntime->letGo = [this] { m_stackScrubLetGo = m_stackScrub.active(); };
    m_carryRuntime->wheeled = [this](int notches) {
        if (!m_stackScrub.active() && m_carryWidthEdge != CarryWidthEdge::None)
            return turnCarryWidth(notches);
        if (!m_stackScrub.active() && !m_stackScrubArmed && holdCarryWidth())
            return turnCarryWidth(notches);
        if (!m_stackScrub.active()) {
            if (!m_stackScrubArmed || !m_stackScrubPane) return false;
            beginStackScrub(stackScrubHand(KWin::effects->cursorPos().y()));
        }
        m_stackScrubHeldStill = false;
        m_stackScrub.wheel(notches, stackScrubHand(KWin::effects->cursorPos().y()));
        refreshStackScrubShown();
        KWin::effects->addRepaintFull();
        return true;
    };
    m_carryRuntime->deferred = [this](KWin::Window *w) {
        // Native-drag visibility exception only: no controller start, which
        // would invalidate the saved reservation or take geometry ownership.
        m_nativeCarry = w->effectWindow();
        m_nativeCarrySource = w->output()->name();
        m_nativeCarryFromBento = false;
        takeCarriedDialogs(m_nativeCarry);
        KWin::effects->setElevatedWindow(m_nativeCarry, true);
    };
    // §11, Connected: held anywhere on a monitor, a turned wheel carries the
    // window, as the top edge would.
    m_carryRuntime->wheelEntryRequested = [this](const PreparedCarrySource &, QPointF p) {
        for (auto *output : KWin::effects->screens())
            if (QRectF(output->geometry()).contains(p))
                return connectedOutputForDesktopStage(output) && !carryEdgeAt(output, p) && !isPanelPoint(p);
        return false;
    };
    m_carryRuntime->entryRequested = [this](const PreparedCarrySource &source, QPointF p) {
        for (auto *output : KWin::effects->screens()) {
            if (!QRectF(output->geometry()).contains(p)) continue;
            if (carryEdgeAt(output, p)) return true;
            if (isPanelPoint(p)) return false;
            return output->name() != source.origin().output
                && (isTabletOutput(output) || m_desktopStage->hasSessionOnOutput(output->name()));
        }
        return false;
    };
    m_carryRuntime->released = [this](bool committed) {
        m_carryLanded = committed;
        // §11, Connected: a side snap that leaves room open offers the other
        // cards there, once the snapped card has grown into its place.
        KWin::LogicalOutput *landed = nullptr;
        if (committed && m_carryWidthEdge == CarryWidthEdge::Side)
            for (auto *o : KWin::effects->screens())
                if (QRectF(o->geometry()).contains(m_lastCarryContact)) landed = o;
        if (landed) askRoomOffer(landed);
        const auto refused = std::exchange(m_carryRefusal, std::nullopt);
        if (!committed && refused && refused->output)
            showGestureNote(refused->output, refused->contact, refused->reason);
    };
    m_carryRuntime->ended = [this] {
        m_carryRefusal.reset();
        // §10 Cancel: a carry cancelled, refused or interrupted puts the
        // window back where it was, and it glides there from under the hand.
        const QPointer<KWin::EffectWindow> carried = m_carriedWindow;
        const QRectF drawn = std::exchange(m_carryDrawn, QRectF());
        const bool landed = std::exchange(m_carryLanded, false);
        const QString origin = std::exchange(m_carryOrigin, QString());
        // A Stack scrubbed goes in where it was let go, and nowhere when the
        // scrub was cancelled. The pane never left its place.
        const QPointer<KWin::EffectWindow> scrubbed = m_stackScrub.active() ? m_stackScrubPane : nullptr;
        const int place = m_stackScrub.place;
        const bool letGo = std::exchange(m_stackScrubLetGo, false);
        endStackScrub();
        endNativeCarryPresentation();
        if (m_heldActiveMove == carried) m_heldActiveMove.clear();
        if (scrubbed) {
            if (letGo && place > 0)
                QTimer::singleShot(0, this, [this, scrubbed, place] {
                    if (scrubbed) (void)reorderPaneStack(scrubbed, place);
                });
            return;
        }
        // §11, Every display: wherever the carry left it, a window loose on a
        // display holding its own cards becomes a card there.
        if (carried)
            QTimer::singleShot(0, this, [this, carried, origin] { claimWindowForItsDisplay(carried, origin); });
        if (landed || !carried || carried->isDeleted() || !carried->window()) return;
        auto *output = carried->window()->moveResizeOutput();
        startDropSettle(carried, output, drawn, QRectF(carried->window()->moveResizeGeometry()),
                        output && isTabletOutput(output));
    };
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
    m_carryRuntime->interrupted = [this] {
        if (m_motionOutlivesInterrupt) return;
        clearDropSettle();
        clearBentoMotions();
        clearYieldingPanes();
    };
    connect(KWin::effects, &KWin::EffectsHandler::screenAboutToLock, this,
            [this] {
                if (m_carryRuntime) m_carryRuntime->cancel();
                // The lock screen takes the releases from here on.
                if (m_touchWitness) (void)m_touchWitness->touchCancel();
            });
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        connectManagedWindow(window);
    }

    if (!QDBusConnection::sessionBus().registerObject(
            QStringLiteral("/Kadunce"),
            QStringLiteral("co.goodinput.Kadunce"), this,
            QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals)) {
        qWarning() << "Kadunce" << Revision
                   << "could not publish the workspace context interface";
    }

    // Gooseberry's notes, while it runs; without it Spread draws none.
    m_stuckNotesWatcher = new StuckNotesWatcher(this);
    connect(m_stuckNotesWatcher, &StuckNotesWatcher::windowsChanged, this, [this](const QVariantList &windows) {
        const auto entries = stuckNotesEntries(windows);
        const auto shown = std::count_if(entries.cbegin(), entries.cend(),
                                         [](const StuckNotesEntry &entry) { return entry.shown; });
        qInfo() << "Kadunce reads notes on" << entries.size() << "windows from Gooseberry," << shown << "shown";
        m_stuckNotes.setEntries(entries);
        syncNotesPause();
        Q_EMIT workspaceContextChanged();
        if (m_cardStage && m_cardStage->isActive()) KWin::effects->addRepaintFull();
    });

    if (KWin::input()) {
        m_inputRouter = std::make_unique<WorkspaceInputRouter>(
            static_cast<WorkspaceInputTarget *>(this),
            m_usesDirectSystemEdges);
        KWin::input()->installInputEventFilter(m_inputRouter.get());
        // After the carry filter, so that it sees every contact first.
        m_touchWitness = std::make_unique<TouchWitness>();
        m_touchWitness->inBottomBezel = [this](const QPointF &position) {
            return m_inputRouter && m_inputRouter->inBottomBezel(position);
        };
        m_touchWitness->describeTitleTouch = [this](const QPointF &position) {
            return describeTitleTouch(position);
        };
        // A title bar can be left holding a contact whose lift it never saw,
        // as when the tap that chose a card in Spread was Kadunce's. KWin's
        // title bar handling then ignores every press until a contact with
        // that number lifts, so the next drag of a title does nothing. With
        // only this contact down, whatever it holds is stale.
        m_touchWitness->beforeTouchDown = [](qint32 id) {
            KWin::TouchInputRedirection *touch = KWin::input() ? KWin::input()->touch() : nullptr;
            if (!touch || touch->decorationPressId() == -1 || touch->touchPointCount() > 1) return;
            qInfo() << "Kadunce clears a title bar's hold on contact" << touch->decorationPressId()
                    << "that lifted unseen, so contact" << id << "can move its window";
            touch->setDecorationPressId(-1);
        };
        // A carry Kadunce takes from a title bar keeps the contact's lift, so
        // KWin's title bar handling never hears it and holds the contact on.
        // KWin aims each new contact before any filter sees it, and while
        // that hold stands it aims at nothing: the next touch, a swipe up
        // say, would be spent before the hold above is cleared. Once a lift
        // has passed every filter, a hold on it with no contact down is let
        // go there and then.
        m_touchWitness->onTouchUp = [this](qint32 id) {
            QTimer::singleShot(0, this, [id] {
                KWin::TouchInputRedirection *touch = KWin::input() ? KWin::input()->touch() : nullptr;
                if (!touch || touch->decorationPressId() != id || touch->touchPointCount() > 0) return;
                qInfo() << "Kadunce lets go of a title bar's hold on contact" << id
                        << "whose lift KWin's title bar never saw";
                touch->setDecorationPressId(-1);
            });
        };
        m_touchWitness->describeTouchState = [] {
            const KWin::TouchInputRedirection *touch = KWin::input() ? KWin::input()->touch() : nullptr;
            if (!touch) return QStringLiteral("has no touch input");
            const KWin::Window *focus = touch->focus();
            auto *seat = KWin::waylandServer() ? KWin::waylandServer()->seat() : nullptr;
            return QStringLiteral("focuses %1, its title bar holds contact %2, %3 contacts down, client touch sequence %4")
                .arg(focus ? focus->resourceClass() : QStringLiteral("nothing"))
                .arg(touch->decorationPressId())
                .arg(touch->touchPointCount())
                .arg(seat && seat->isTouchSequence() ? QStringLiteral("open") : QStringLiteral("none"));
        };
        if (m_carryRuntime) m_carryRuntime->touchStillDown = [this](qint64 id) {
            return !m_touchWitness || m_touchWitness->down().contains(qint32(id));
        };
    }

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
                    ? "direct system edges"
                    : "Plasma-native system edges")
            << "and output-local Bento ready; fan aperture"
            << (m_fanApertureShader ? "enabled" : "r20 fallback");
    // Switching Kadunce on puts the display that can own cards in cards. The
    // turn lets KWin finish loading the effect before the first placement.
    QTimer::singleShot(0, this, [this] { (void)holdDisplaysInCards(nullptr); });
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
    if (arrival) m_letGo = false;
    if (m_letGo) return false;
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
    delete std::exchange(m_stuckNotesWatcher, nullptr);
    if (m_gapHeld) KWin::effects->stopMouseInterception(this);
    m_table.close();
    if (m_tablePresenter) m_tablePresenter->hide();
    m_spokenLine.reset();
    if (m_tableKeyboard) KWin::effects->ungrabKeyboard();
    giveTableCornerBack();
    endDesktopPreview();
    if (m_nativeCarry) KWin::effects->setElevatedWindow(m_nativeCarry, false);
    forEachSession([this] { m_desktopStage->cancelRestoredMinimizations(); });
    // Roll back input while its target/controllers are alive, then unregister
    // the filter before restoration can reenter KWin or destroy those owners.
    cancelInputForCardStage();
    m_touchWitness.reset();
    leaveRoomOffer();
    m_roomOfferInput.reset();
    m_carryRuntime.reset();
    m_inputRouter.reset();
    Q_EMIT bridgeUnavailable();
    closeLauncherGuest();
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
    forEachSession([&] {
        owning = owning || hasActiveDesktopStage();
        forEachDisplayStage([&](CardStageController &stage) { owning = owning || stage.isActive(); });
    });
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
    if (!callerMayDrive()) return;
    if (presentationForInput() != WorkspacePresentation::Spread) {
        toggle();
    }
}

void Effect::showActive()
{
    if (!callerMayDrive()) return;
    if (presentationForInput() == WorkspacePresentation::Spread) {
        toggle();
    }
}

void Effect::showCardLineOn(const QString &outputName)
{
    // §11: the dock on a display opens that display's own Spread.
    KWin::LogicalOutput *output = screenNamed(outputName);
    SessionScope scope(this, m_currentSession, isCardDisplay(output) ? stageKeyFor(output) : m_scopedKey);
    noteDisplayInUse(output);
    showCardLine();
}

void Effect::showActiveOn(const QString &outputName)
{
    KWin::LogicalOutput *output = screenNamed(outputName);
    SessionScope scope(this, m_currentSession, isCardDisplay(output) ? stageKeyFor(output) : m_scopedKey);
    noteDisplayInUse(output);
    showActive();
}

bool Effect::supported()
{
    const QString built = QStringLiteral(KADUNCE_BUILT_KWIN);
    const QString running = QCoreApplication::applicationVersion();
    if (kwinMatches(built, running)) return true;
    qWarning() << "Kadunce" << Revision << "built for KWin" << built
               << "but KWin" << running << "is running; Cards stay off until a rebuild";
    return false;
}

void Effect::adoptDirectSystemEdges()
{
    if (m_usesDirectSystemEdges || m_touchOutputName.isEmpty()) return;
    m_usesDirectSystemEdges = true;
    // Hand each edge back to Plasma before the router claims it, so one swipe
    // cannot reach both backends. Adoption is one-way: a touchscreen that
    // later goes away leaves Kadunce's own recognition in place rather than
    // churning the backend mid-session.
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
            << "adopted direct system edges after a touchscreen appeared";
}

bool Effect::isTabletOutput(const KWin::LogicalOutput *output) const
{
    // The display of the stage named now: the one display holding cards, or
    // while every display holds its own, the one the stage is kept for.
    if (!output || m_cardOutputName.isEmpty()) return false;
    return output->name() == (m_scopedKey.isEmpty() ? m_cardOutputName : m_scopedKey);
}

bool Effect::isCardDisplay(const KWin::LogicalOutput *output) const
{
    return output && !m_cardOutputName.isEmpty()
        && (m_cardsOnEveryDisplay || output->name() == m_cardOutputName)
        && KWin::effects->screens().contains(const_cast<KWin::LogicalOutput *>(output));
}

void Effect::refreshCardOutput()
{
    QList<TouchDisplayOutput> outputs;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (output)
            outputs.append({output->name(), output->isInternal(), QSizeF(output->physicalSize()),
                QSizeF(output->geometry().width(), output->geometry().height()), output->uuid()});
    }
    // §11: the chosen display is kept by its lasting identity as well as its
    // name. A choice that has none yet takes its display's; one whose display
    // came back under another name keeps that name from now on.
    {
        const QString chosen = chosenOutputName(outputs, m_spreadChoice);
        QString identity = m_spreadChoice.identity;
        if (identity.isEmpty() && !chosen.isEmpty()) {
            for (const auto &output : outputs)
                if (output.name == chosen) identity = output.identity;
        }
        const QString display = chosen.isEmpty() ? m_spreadChoice.display : chosen;
        if (display != m_spreadChoice.display || identity != m_spreadChoice.identity) {
            m_spreadChoice.display = display;
            m_spreadChoice.identity = identity;
            writeSpreadChoice();
        }
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
    m_touchOutputName = touchOutputName(outputs, devices);
    // A touchscreen that arrives after KWin, or is plugged in later, gives
    // the edges to Kadunce's own router.
    adoptDirectSystemEdges();
    const QString name = cardOutputName(outputs, devices, m_spreadChoice);
    if (name == m_cardOutputName) return;
    const QString previous = m_cardOutputName;
    m_cardOutputName = name;
    qInfo() << "Kadunce" << Revision << (m_cardsOnEveryDisplay ? "the primary display is" : "cards belong to")
            << (name.isEmpty() ? QStringLiteral("no display") : name)
            << (previous.isEmpty() ? QString() : QStringLiteral("after ") + previous);
    if (m_cardsOnEveryDisplay) {
        // §11: every display keeps its own Spread and cards. The primary
        // display moves nothing; it is where a window or Search goes when
        // nothing else says where. A display that holds cards for the first
        // time holds its windows as cards, as switching Kadunce on does.
        if (m_scopeDepth == 0) restInUse();
        m_letGo = false;
        QTimer::singleShot(0, this, [this] { (void)holdDisplaysInCards(nullptr); });
        scheduleCardDisplaySettle();
        Q_EMIT workspaceContextChanged();
        return;
    }
    if (m_cardStage->isActive()) {
        cancelInputForCardStage();
        // With no display left to hold cards, they return to the desktop.
        // Otherwise Spread moves whole: every card goes to the new display in
        // the settle that follows, and that display's windows join it there.
        if (name.isEmpty()) m_cardStage->release();
        else m_cardDisplayMoved = true;
    } else if (!name.isEmpty()) {
        // §3: a display that takes cards over holds its windows as cards, as
        // switching Kadunce on does. The turn lets KWin finish moving windows.
        m_letGo = false;
        QTimer::singleShot(0, this, [this] { (void)startTabletInCards(nullptr); });
    }
    scheduleCardDisplaySettle();
    Q_EMIT workspaceContextChanged();
}

void Effect::setSpreadDisplay(const QString &outputName)
{
    KWin::LogicalOutput *output = outputName.isEmpty() ? nullptr : KWin::effects->findScreen(outputName);
    // A display away keeps the identity it was chosen with. Choosing the same
    // name again for another monitor that has it now is a new choice.
    const QString identity = output ? output->uuid()
        : outputName == m_spreadChoice.display ? m_spreadChoice.identity : QString();
    if (outputName == m_spreadChoice.display && identity == m_spreadChoice.identity) return;
    m_spreadChoice.display = outputName;
    m_spreadChoice.identity = identity;
    writeSpreadChoice();
    refreshCardOutput();
}

void Effect::writeSpreadChoice()
{
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Spread"));
    if (m_spreadChoice.display.isEmpty()) group.deleteEntry(QStringLiteral("Display"));
    else group.writeEntry(QStringLiteral("Display"), m_spreadChoice.display);
    if (m_spreadChoice.identity.isEmpty()) group.deleteEntry(QStringLiteral("DisplayIdentity"));
    else group.writeEntry(QStringLiteral("DisplayIdentity"), m_spreadChoice.identity);
    group.sync();
}

QString Effect::chosenSizingFor(const KWin::LogicalOutput *output) const
{
    if (!output) return QString();
    return chosenSizing(m_sizingChoices, m_sizingIdentities, output->name(), output->uuid());
}

DisplaySizing Effect::sizingFor(const KWin::LogicalOutput *output) const
{
    if (!output) return DisplaySizing::Solo;
    const auto geometry = output->geometry();
    return Kadunce::displaySizing(chosenSizingFor(output),
        geometry.width(), geometry.height());
}

QString Effect::displaySizing(const QString &outputName) const
{
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (output && output->name() == outputName) return sizingName(sizingFor(output));
    }
    return QString();
}

void Effect::setDisplaySizing(const QString &outputName, const QString &sizing)
{
    // An empty or unknown choice returns the display to sizing by room.
    const std::optional<DisplaySizing> parsed = parseSizing(sizing);
    const QString chosen = parsed ? sizingName(*parsed) : QString();
    if (outputName.isEmpty()) return;
    KWin::LogicalOutput *output = KWin::effects->findScreen(outputName);
    if (output && output->name() != outputName) output = nullptr;
    const QString identity = output ? output->uuid() : QString();
    const QString current = output ? chosenSizingFor(output) : m_sizingChoices.value(outputName);
    if (current == chosen && (chosen.isEmpty() || m_sizingChoices.contains(outputName))) return;
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Sizing"));
    KConfigGroup displays = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("SizingDisplays"));
    // A choice is kept once, under the display's name now: one this display
    // made under an earlier name goes, as does whatever another display left
    // under this name.
    const QStringList names = m_sizingIdentities.keys();
    for (const QString &name : names) {
        if (name == outputName || identity.isEmpty() || m_sizingIdentities.value(name) != identity) continue;
        m_sizingChoices.remove(name);
        m_sizingIdentities.remove(name);
        group.deleteEntry(name);
        displays.deleteEntry(name);
    }
    if (chosen.isEmpty()) {
        m_sizingChoices.remove(outputName);
        m_sizingIdentities.remove(outputName);
        group.deleteEntry(outputName);
        displays.deleteEntry(outputName);
    } else {
        m_sizingChoices.insert(outputName, chosen);
        group.writeEntry(outputName, chosen);
        if (identity.isEmpty()) {
            m_sizingIdentities.remove(outputName);
            displays.deleteEntry(outputName);
        } else {
            m_sizingIdentities.insert(outputName, identity);
            displays.writeEntry(outputName, identity);
        }
    }
    group.sync();
    QTimer::singleShot(0, this, [this] { (void)holdDisplaysInCards(nullptr); });
    Q_EMIT workspaceContextChanged();
    KWin::effects->addRepaintFull();
}

bool Effect::connectedForCardStage() const
{
    KWin::LogicalOutput *output = tabletOutput();
    return output && sizingFor(output) == DisplaySizing::Connected;
}

bool Effect::layoutLeavesRoomForCardStage() const
{
    KWin::LogicalOutput *output = tabletOutput();
    return output && m_desktopStage->presentsPlacedLayoutOn(output->name());
}

bool Effect::isCardWindow(const KWin::EffectWindow *window) const
{
    // A card held aside while the desktop shows is hidden by Card Stage, and
    // is still a card.
    return isApplicationWindow(window)
        && !window->isMinimized()
        && (!window->isHidden() || window->data(CardAsideRole).toBool());
}

bool Effect::floatsAtItsOwnSize(const KWin::EffectWindow *window) const
{
    if (!window || window->isDeleted() || !window->window() || !window->isNormalWindow()) return false;
    // Shuffle's own surfaces, Settings and Welcome, are drawn frameless at
    // one size on every display, as Search is.
    const QString identity = applicationIdentity(window);
    if (identity.startsWith(QStringLiteral("co.goodinput.Shuffle."), Qt::CaseInsensitive)
        && !window->hasDecoration()) return true;
    // A window that cannot be resized fills no pane and no Active card.
    const QSizeF least = window->window()->minSize();
    const QSizeF most = window->window()->maxSize();
    return least.width() > 0.0 && least.height() > 0.0 && least == most;
}

void Effect::centreFloatingWindow(KWin::EffectWindow *window, KWin::LogicalOutput *on)
{
    if (!floatsAtItsOwnSize(window) || window->isFullScreen() || window->isUserMove()) return;
    KWin::LogicalOutput *output = on ? on : screenNamed(inUseKey());
    if (!output) output = tabletOutput();
    if (!output) output = window->screen();
    if (!output) return;
    const QRectF area = KWin::effects->clientArea(KWin::MaximizeArea, output);
    QSizeF size = QRectF(window->frameGeometry()).size();
    // Shuffle Settings opens at the tablet's Active card size on every
    // display, so it reads as the same card wherever it opens; with no tablet,
    // at this display's own.
    const bool settings = applicationIdentity(window)
        .compare(QStringLiteral("co.goodinput.Shuffle.Settings"), Qt::CaseInsensitive) == 0;
    if (settings) {
        KWin::LogicalOutput *tablet = tabletOutput();
        if (!tablet) tablet = output;
        const QSizeF card = QRectF(m_cardStage->activeTarget(tablet)).size();
        const QSizeF least = window->window()->minSize();
        if (!card.isEmpty()) {
            size = QSizeF(std::clamp(card.width(), least.width(), std::max(least.width(), area.width())),
                          std::clamp(card.height(), least.height(), std::max(least.height(), area.height())));
        }
    }
    const QPointF at(area.x() + std::max(0.0, (area.width() - size.width()) / 2.0),
                     area.y() + std::max(0.0, (area.height() - size.height()) / 2.0));
    window->window()->moveResize(KWin::RectF(QRectF(at, size)));
    qInfo() << "Kadunce" << Revision << "floats" << logNameOf(window) << "centred on" << output->name()
            << "at" << size;
    if (!settings) return;
    window->setProperty("kaduncePlacedSize", size);
    // Keys raised as Settings opens by touch make KWin keep the size it had
    // before it was centred, and put that back once they go: it is centred
    // at its card size again then, on its own display.
    if (window->property("kaduncePutsBack").toBool()) return;
    window->setProperty("kaduncePutsBack", true);
    const QPointer<KWin::EffectWindow> watched = window;
    const auto puts = std::make_shared<int>(0);
    connect(window, &KWin::EffectWindow::windowFrameGeometryChanged, this, [this, watched, puts] {
        QTimer::singleShot(0, this, [this, watched, puts] {
            if (!watched || watched->isDeleted() || !watched->window() || watched->isUserMove()
                || watched->isUserResize() || *puts >= 20) return;
            const QSizeF wanted = watched->property("kaduncePlacedSize").toSizeF();
            const QSizeF asked = QRectF(watched->window()->moveResizeGeometry()).size();
            if (!wanted.isValid() || (std::abs(asked.width() - wanted.width()) < 1.0
                                      && std::abs(asked.height() - wanted.height()) < 1.0)) return;
            // While the keys are up, the room KWin makes for them stands.
            if (inputPanelTopForCardStage(watched->screen())) return;
            ++*puts;
            qInfo() << "Kadunce" << Revision << "puts" << logNameOf(watched) << "back at its card size from" << asked;
            centreFloatingWindow(watched, watched->screen());
        });
    });
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
    // The screenshot tool's capture floats over cards as a guest: it asks to
    // fill the screen, frameless and above the rest, and would otherwise join
    // a layout half drawn. The window showing the result is a card
    // (CARD-LIFECYCLE.md §4).
    if ((identity.compare(QStringLiteral("org.kde.spectacle"), Qt::CaseInsensitive) == 0
            || identity.compare(QStringLiteral("spectacle"), Qt::CaseInsensitive) == 0)
        && (window->isFullScreen() || window->keepAbove() || !window->hasDecoration())) {
        return false;
    }
    if (floatsAtItsOwnSize(window)) return false;
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
    if (current) m_unenteredDesktops.remove(current->id());
    // Table's own switches close it first; any other ends it.
    if (m_table.isOpen()) closeTable();
    // A switch made some other way ends a preview of a different desktop. A
    // commit keeps it, and the screen, until the switch has been announced.
    if (m_previewDesktop && current != m_previewDesktop) endDesktopPreview();
    // Nothing started on the desktop being left may still be in hand. Its
    // session keeps its cards and layouts exactly as they are.
    cancelInputForCardStage();
    m_currentSession = sessionFor(current);
    scopeTo(m_currentSession, m_scopedKey);
    bool holding = false;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (!isCardDisplay(output)) continue;
        SessionScope scope(this, m_currentSession, stageKeyFor(output));
        setPagingShortcutsActive(m_cardStage->isActive());
        // Coming back finds the cards presented, not a Spread left open. It
        // cannot close on the way out: its cards are not on the desktop being shown.
        if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread)
            toggleOwnedPresentation();
        if (m_cardStage->isActive()) holding = true;
    }
    // Windows that opened or arrived here while it was not shown become this
    // desktop's cards, as windows arriving on a display do.
    if (holding) scheduleCardDisplaySettle();
    // §3 for a display with no cards yet on this desktop: its windows become
    // cards, the one in use Active, as when Kadunce is switched on. The turn
    // lets KWin finish activating a window on the desktop it switched to. A
    // display presenting its layout starts no cards; what arrived there while
    // it was not shown waits behind the layout instead.
    QTimer::singleShot(0, this, [this, holding] {
        if (!holdDisplaysInCards(nullptr) && !holding) scheduleCardDisplaySettle();
    });
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
    if (desktop) m_unenteredDesktops.remove(desktop->id());
    // A stack carried to it and not yet dealt goes with it.
    if (desktop) m_pendingDecks.remove(desktop->id());
    // KWin has already moved the desktop's windows to a neighbour. Its session
    // lets them go as a release would, and the desktop they stand on now takes
    // them as its own (DesktopRemoval.h). Removing the desktop shown can come
    // before the switch away from it, or with none, so the desktop KWin shows
    // now is made current first.
    const QString id = desktop ? desktop->id() : QString();
    auto removed = m_sessions.find(id);
    KWin::VirtualDesktop *shown = KWin::effects->currentDesktop();
    const DesktopRemovalPlan plan = planDesktopRemoval(removed != m_sessions.end(), id,
        m_currentSession ? m_currentSession->desktopId : QString(), shown ? shown->id() : QString());
    if (!plan.release) return;
    if (plan.switchFirst) handleDesktopChanged(desktop, shown);
    {
        SessionScope scope(this, removed->second.get());
        if (hasActiveDesktopStage()) {
            m_desktopStage->stopPendingSettle();
            m_desktopStage->restoreAllSessions();
        }
        forEachDisplayStage([](CardStageController &stage) { stage.release(); });
    }
    if (plan.forget) m_sessions.erase(removed);
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
        if (CardStageController *own = stageHolding(window)) (void)own->releaseCard(window);
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
    const CardStageController *cards = holder ? holder->stageHolding(window) : nullptr;
    // A Spread left open there comes back as its Active card, so that card is
    // the one shown; a desktop presenting its layout shows only its panes.
    return holder && holder != m_currentSession && cards
        && !(cards->presentation() != CardPresentation::Bento && cards->selectedWindow() == window);
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
    const CardStageController *cards = holder ? holder->stageHolding(window) : nullptr;
    if (!cards) return false;
    if (m_previewCard && cards->liveCardIndex(m_previewCard) >= 0) return window != m_previewCard;
    return !(cards->presentation() != CardPresentation::Bento && cards->selectedWindow() == window);
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
    if (!callerMayDrive()) return false;
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
    if (!callerMayDrive()) return false;
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
    // What hangs in a workspace's row: an application's own window, not a
    // dialog, not one on every desktop, and not one kept out of the taskbar,
    // the pager or the task switcher, even when it is a card.
    const auto application = [](KWin::EffectWindow *window) {
        if (!window || window->isDeleted() || !window->window()) return false;
        const KWin::Window *client = window->window();
        return tableListsWindow({
            .normal = window->isNormalWindow(),
            .onAllDesktops = window->isOnAllDesktops(),
            .dependent = dependentLead(window) != nullptr,
            .skipSwitcher = client->skipSwitcher(),
            .skipTaskbar = client->skipTaskbar(),
            .skipPager = client->skipPager(),
        });
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
            // Each display's cards, the primary display's first: its front
            // card first, then the rest in Spread's order.
            std::vector<const CardStageController *> stages;
            for (const auto &display : session->displays)
                if (display.first == m_cardOutputName || display.first.isEmpty())
                    stages.insert(stages.begin(), display.second->cards.get());
                else stages.push_back(display.second->cards.get());
            for (const CardStageController *cards : stages) {
                KWin::EffectWindow *front = cards->isActive() ? cards->selectedWindow() : nullptr;
                QList<KWin::EffectWindow *> ordered;
                if (front) ordered.append(front);
                for (const auto &window : cards->liveCards())
                    if (window && window != front) ordered.append(window);
                for (KWin::EffectWindow *window : std::as_const(ordered)) {
                    if (!window || window->isDeleted() || taken.contains(window)) continue;
                    // A stack is one card, its face in front, of the members
                    // Table lists.
                    QList<KWin::EffectWindow *> deck = cards->stackFrontToBack(window);
                    if (deck.isEmpty()) deck.append(window);
                    for (KWin::EffectWindow *member : std::as_const(deck)) taken.insert(member);
                    deck.removeIf([&application](KWin::EffectWindow *member) { return !application(member); });
                    if (deck.isEmpty()) continue;
                    TableCard card = cardOf(deck.first());
                    for (KWin::EffectWindow *member : std::as_const(deck))
                        if (member != card.windows.first()) card.windows.append(member);
                    card.stacked = int(card.windows.size()) - 1;
                    workspace.cards.append(card);
                }
            }
            // A layout's panes are cards of their own.
            for (const auto &view : session->layouts->ownershipView()) {
                for (const quintptr pane : view.panes) {
                    auto *window = reinterpret_cast<KWin::EffectWindow *>(pane);
                    if (!window || window->isDeleted() || taken.contains(window) || !application(window)) continue;
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
        // A workspace nothing has named shows the name of the application it
        // holds first, as one made by carrying a card to + does.
        if (unnamedDesktop(workspace.name) && !workspace.cards.isEmpty())
            workspace.name = workspace.cards.first().application;
        if (unnamedDesktop(workspace.name)) workspace.name.clear();
        workspaces.append(workspace);
    }
    return workspaces;
}

void Effect::readTableWorkspaces()
{
    m_tableWorkspaces = tableWorkspaces();
    // The name each unnamed workspace shows becomes its own, saved once.
    bool renamed = false;
    for (const TableWorkspace &workspace : std::as_const(m_tableWorkspaces)) {
        if (!workspace.desktop || workspace.name.isEmpty()
            || !unnamedDesktop(workspace.desktop->name())) continue;
        workspace.desktop->setName(workspace.name);
        renamed = true;
    }
    if (renamed) KWin::VirtualDesktopManager::self()->save();
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
    m_tableLine = tableLine();
    syncSpokenLine();
}

void Effect::openTable(bool sticky, KWin::LogicalOutput *output)
{
    if (m_table.isOpen()) return;
    m_tableOutput = output ? output : tabletOutput();
    if (!m_tableOutput) return;
    // Nothing begun elsewhere may still be in hand.
    if (m_carryRuntime) m_carryRuntime->cancel();
    readTableWorkspaces();
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
    // A Table button elsewhere, the dock's among them, shows Table open.
    Q_EMIT workspaceContextChanged();
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
    Q_EMIT workspaceContextChanged();
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
    // Table comes down where it is pulled, so the finger stays on its tabs:
    // from a touchscreen's own top edge, on that touchscreen's display.
    openTable(false, KWin::effects->screenAt(position.toPoint()));
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
    m_pointerResting = false;
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
    // Nothing is read for a log no one asked for.
    if (!KaduncePointer().isInfoEnabled()) return;
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
    qCInfo(KaduncePointer).nospace() << "Kadunce pointer near the top at " << position << " is over "
                      << (under ? logNameOf(under) : QStringLiteral("nothing"))
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
    if (!callerMayDrive()) return;
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
    readTableWorkspaces();
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
        // Table's own workspace dissolves as any left one does.
        m_unenteredDesktops.remove(created->id());
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
    if (m_spokenLine) m_spokenLine->hide();
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
    readTableWorkspaces();
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

void Effect::setShowsKeyboardOutline(bool shows)
{
    if (shows == m_showsKeyboardOutline) return;
    m_showsKeyboardOutline = shows;
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Windows"));
    group.writeEntry(QStringLiteral("KeyboardOutline"), shows);
    group.sync();
    KWin::effects->addRepaintFull();
}

// Meta+Shift+B says which arrangement the monitors now take, as Plasma's own
// switches do.
void Effect::switchZoneMode()
{
    setUsesDrawnZones(!m_usesDrawnZones);
    const QString text = m_usesDrawnZones ? QStringLiteral("Monitors: my zones") : QStringLiteral("Monitors: auto fill");
    // A surface showing the context says it on its line instead of a popup
    // over the work.
    if (!m_contextLineClaims.isEmpty()) {
        m_zoneNotice = ZoneNotice{text, {}};
        m_zoneNotice->shown.start();
        m_contextNoticeEnd.start();
        Q_EMIT workspaceContextChanged();
        return;
    }
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/org/kde/osdService"), QStringLiteral("org.kde.osdService"), QStringLiteral("showText"));
    message << QStringLiteral("view-grid") << text;
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
        // The workspace the person is in stays until they leave it, and one
        // made elsewhere stays until it has been entered and left.
        if (desktop == KWin::effects->currentDesktop() || m_namedDesktops.contains(desktop->id())
            || m_unenteredDesktops.contains(desktop->id())) continue;
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
    const CardStageController *cards = holder ? holder->stageHolding(lead) : nullptr;
    if (!lead || !cards) return true;
    // A card on a desktop that is not shown is not in front.
    if (holder != m_currentSession) return false;
    return cards->presentation() == CardPresentation::Active && cards->selectedWindow() == lead;
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
    // The window learns how large it may grow on the display it is on now, and
    // a pane it fell short of there is asked for again within those bounds.
    if (sendDisplayBounds(client, client->moveResizeOutput()) && m_desktopStage->managesWindow(window))
        QTimer::singleShot(0, this, [this, window] {
            if (window && !window->isDeleted()) (void)m_desktopStage->replacePaneShortOfPlace(window);
        });
    // A window KWin sends to another display some other way than a display
    // change, such as its window-to-screen shortcut, is answered as one that
    // opened there. KWin reports the change inside the move that makes it, so
    // the turn after sees where the window stands. A card, a pane or a window
    // being carried already has an owner deciding where it goes.
    QTimer::singleShot(0, this, [this, window] {
        // §11, Every display: a card sent to another display that holds its
        // own cards leaves its stage for that display's, which shows it.
        KWin::LogicalOutput *there = window && window->window() ? window->window()->moveResizeOutput() : nullptr;
        if (window && !m_releasing && window->window() && !window->isUserMove()
            && window != m_carriedWindow && window != m_nativeCarry && stageHolding(window)
            && !displaysJustChanged() && isCardDisplay(there) && stageKeyOf(window) != stageKeyFor(there)) {
            (void)moveCardToDisplay(window, there, true);
            return;
        }
        // A pane sent to another display leaves its layout there as one
        // carried there by its title bar does. Spread moves no pane.
        CardStageController *keeper = window ? stageKeepingStackOf(window) : nullptr;
        const QList<KWin::EffectWindow *> behind = keeper ? keeper->paneStack(window)
                                                          : QList<KWin::EffectWindow *>{};
        if (window && !m_releasing && window->window() && !window->isUserMove()
            && !window->isUserResize() && window != m_carriedWindow && window != m_nativeCarry
            && !stageHolding(window) && !anySpreadShown()
            && m_desktopStage->handoffPaneToOutput(window, there)) {
            // §11, Every display: on a display holding its own cards and no
            // layout it is the card shown there, and its Stack goes with it.
            there = window->screen();
            qInfo() << "Kadunce" << Revision << "handed" << logNameOf(window) << "to"
                    << (there ? there->name() : QString()) << "with" << behind.size()
                    << "behind it; a pane there" << m_desktopStage->managesWindow(window);
            if (isCardDisplay(there)) {
                SessionScope scope(this, m_scopedSession, stageKeyFor(there));
                if (!m_desktopStage->managesWindow(window)) claimWindowForItsDisplay(window);
                rejoinStack(window, behind);
            }
            KWin::effects->activateWindow(window);
            observeCardOwnership();
            Q_EMIT workspaceContextChanged();
            return;
        }
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
        m_cardFaceRects.remove(window);
        observeCardOwnership();
        qInfo() << "Kadunce" << logNameOf(window) << "belongs to" << logNameOf(lead)
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
    if (m_cardOutputName.isEmpty()) return nullptr;
    return screenNamed(m_scopedKey.isEmpty() ? m_cardOutputName : m_scopedKey);
}

KWin::LogicalOutput *Effect::primaryOutput() const
{
    return screenNamed(m_cardOutputName);
}

bool Effect::isTabletOutputForDesktopStage(
    const KWin::LogicalOutput *output) const
{
    // Asked for its own display (SessionHost), every display holding cards is
    // its layouts' card display: its own stage presents them, and takes the
    // windows they shed.
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

bool Effect::connectedOutputForDesktopStage(
    const KWin::LogicalOutput *output) const
{
    return isCardDisplay(output) && sizingFor(output) == DisplaySizing::Connected;
}

bool Effect::turnCarryWidth(int notches)
{
    // A part of a notch still belongs to the carry.
    if (notches == 0 || !m_carriedWindow || !m_carriedWindow->window()) return true;
    KWin::LogicalOutput *target = nullptr;
    for (auto *o : KWin::effects->screens())
        if (QRectF(o->geometry()).contains(m_lastCarryContact)) { target = o; break; }
    if (!target) return true;
    const bool top = m_carryWidthEdge == CarryWidthEdge::Top;
    const double span = workAreaForCardStage(target).width();
    if (span <= 0.0) return true;
    const std::span<const double> stops = top ? std::span<const double>(WheelWidthTopStops)
                                              : std::span<const double>(WheelWidthSideStops);
    // The first notch starts from the width the box shows, taken to the
    // stop it stands at.
    if (m_carryWidth <= 0.0) {
        // Held away from the edges, or with no box shown, it starts from the
        // width the window was picked up at.
        const bool fromPickup = !m_carryPreview || (top && m_carryWidthHeld);
        double width = std::clamp((fromPickup ? m_carryPickup.width() : m_carryPreview->width()) / span, 0.0, 1.0);
        if (!top && !m_carryPreview) width = 0.5;
        for (const double stop : stops)
            if (std::abs(stop - width) < .03) width = stop;
        m_carryKnob = WheelWidthKnob{width};
    }
    const double least = std::max(WheelWidthLeast,
        m_carriedWindow->window()->minSize().width() / span);
    const bool crank = m_carryWidthClock.isValid() && m_carryWidthClock.elapsed() < WheelWidthCrankMs;
    m_carryWidthClock.restart();
    m_carryKnob.turn(notches, crank, least, top ? 1.0 : WheelWidthSideMost, stops);
    m_carryWidth = m_carryKnob.width;
    updateNativeCarryDestination(m_lastCarryContact);
    KWin::effects->addRepaintFull();
    return true;
}

bool Effect::holdCarryWidth()
{
    // Pointer carries only: a touchscreen has no wheel.
    if (!m_carriedWindow) return false;
    const QPointF contact = KWin::effects->cursorPos();
    for (auto *o : KWin::effects->screens()) {
        if (!QRectF(o->geometry()).contains(contact)) continue;
        if (!connectedOutputForDesktopStage(o) || carryEdgeAt(o, contact) || isPanelPoint(contact)) return false;
        m_carryWidthHeld = true;
        m_carryWidthEdge = CarryWidthEdge::Top;
        m_carryWidth = 0.0;
        m_carryWidthClock.invalidate();
        m_lastCarryContact = contact;
        return true;
    }
    return false;
}

void Effect::fitArrivalWidth(QPointer<KWin::EffectWindow> window, QString output, double width, int tries)
{
    if (!window || window->isDeleted() || m_releasing) return;
    KWin::LogicalOutput *there = screenNamed(output);
    if (!there) return;
    {
        SessionScope scope(this, m_scopedSession, stageKeyFor(there));
        if (m_cardStage->activeCardIdentity() == window) {
            m_cardStage->setActiveWidth(window, width);
            if (m_cardStage->refitActive(window)) return;
        }
    }
    if (tries > 0)
        QTimer::singleShot(100, this, [this, window, output, width, tries] {
            fitArrivalWidth(window, output, width, tries - 1);
        });
}

BentoSidePlacement Effect::sideChoiceOn(const KWin::LogicalOutput *output, bool right,
    double y, std::optional<BentoSidePlacement> previous) const
{
    const QRectF geometry(output->geometry());
    if (connectedOutputForDesktopStage(output))
        return bentoConnectedSideChoice(right, y, geometry.top(), geometry.height(), previous);
    return bentoSideChoice(right, y, geometry.center().y(), previous);
}

bool Effect::holdsOwnCardsForDesktopStage(const KWin::LogicalOutput *output) const
{
    // Asked for its own display, no display is another's: each holds its
    // cards as the one display holding cards did.
    return isOtherCardDisplay(output);
}

KWin::LogicalOutput *Effect::releaseDisplayForCardStage(const KWin::EffectWindow *) const
{
    // §11, Every display: a card is given back on the display whose stage
    // holds it.
    if (!m_cardsOnEveryDisplay || KWin::effects->screens().size() < 2) return nullptr;
    return tabletOutput();
}

bool Effect::releasesOnOwnDisplayForDesktopStage(const KWin::LogicalOutput *output) const
{
    return m_cardsOnEveryDisplay && isCardDisplay(output);
}

bool Effect::showsOneCardForDesktopStage(const KWin::LogicalOutput *) const
{
    // A display sized Solo shows its Active card on its own stage, never as
    // a layout of one.
    return false;
}

bool Effect::isOtherCardDisplay(const KWin::LogicalOutput *output) const
{
    return isCardDisplay(output) && !isTabletOutput(output);
}

bool Effect::holdDisplaysInCards(KWin::EffectWindow *arrival)
{
    // CARD-LIFECYCLE.md §3, on every display holding cards. A window opening
    // starts only its own display's cards; after letting go only one does.
    bool held = false;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (!isCardDisplay(output) || (arrival && arrival->screen() != output)) continue;
        SessionScope scope(this, m_currentSession, stageKeyFor(output));
        if (startTabletInCards(arrival)) held = true;
    }
    return held;
}

bool Effect::moveCardToDisplay(KWin::EffectWindow *card, KWin::LogicalOutput *to, bool show)
{
    // §11: a card leaving for another display holding its own cards is taken
    // by that display's stage from the one holding it, in one step, and its
    // Stack goes with it. Sent there, it is shown there: in the layout that
    // display shows, or as its Active card. Otherwise it waits in that
    // display's Spread, at the end of its row.
    DesktopSession *session = card ? sessionHolding(card) : nullptr;
    if (!session || card->isDeleted() || !card->window() || !isCardDisplay(to)) return false;
    SessionScope here(this, session, stageKeyOf(card));
    CardStageController *from = m_cardStage;
    const QString toKey = stageKeyFor(to);
    if (from->liveCardIndex(card) < 0 || toKey == m_scopedKey) return false;
    const QList<KWin::EffectWindow *> behind = cardsBehind(*from, card);
    const auto record = [from](KWin::EffectWindow *window) { return from->managedRestore(window); };
    const auto restore = record(card);
    CardStageController &into = stageOn(session, toKey);
    const QPointer<KWin::EffectWindow> moving = card;
    {
        SessionScope there(this, session, toKey);
        if (!into.admitCardFromOtherDisplay(card,
                [from, moving] { return moving && from->releaseCardToStage(moving); },
                restore ? &*restore : nullptr)) {
            qInfo() << "Kadunce" << Revision << "could not move" << logNameOf(card) << "to" << to->name();
            return false;
        }
    }
    from->forgetPaneStack(card);
    QList<KWin::EffectWindow *> carried;
    for (auto *member : std::as_const(behind)) {
        const auto memberRestore = record(member);
        const QPointer<KWin::EffectWindow> follower = member;
        SessionScope there(this, session, toKey);
        if (into.admitCardFromOtherDisplay(member,
                [from, follower] { return follower && from->releaseCardToStage(follower); },
                memberRestore ? &*memberRestore : nullptr)) carried.append(member);
    }
    {
        SessionScope there(this, session, toKey);
        if (!carried.isEmpty() && !into.stackBehind(card, carried))
            qInfo() << "Kadunce" << Revision << "could not stack" << carried.size() << "cards behind"
                    << logNameOf(card) << "on" << to->name();
        // Shown there once its window stands there: KWin moves it to that
        // display after this turn, and a layout takes only what stands on it.
        if (show && moving && !moving->isMinimized()) {
            QList<QPointer<KWin::EffectWindow>> behindIt;
            for (auto *member : std::as_const(carried)) behindIt.append(member);
            onThisStageLater(0, [this, moving, behindIt] { showMovedCard(moving, behindIt); });
        }
    }
    qInfo() << "Kadunce" << Revision << "moved" << logNameOf(card) << "to" << to->name()
            << "with" << carried.size() << "behind it" << (show ? "and shows it there" : "waiting in its Spread");
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
    return true;
}

void Effect::showMovedCard(KWin::EffectWindow *card, const QList<QPointer<KWin::EffectWindow>> &behind, int tries)
{
    // §11: a card sent to this display is shown here: a pane the keys carried
    // away takes back the side it left, else it joins the layout shown here,
    // else it is the Active card.
    if (!card || card->isDeleted() || card->isMinimized() || m_releasing) return;
    // A layout here that took it in already holds its Stack with it.
    if (m_cardStage->liveCardIndex(card) < 0) {
        keepStackBehind(card, behind, 10);
        return;
    }
    KWin::LogicalOutput *here = tabletOutput();
    // Its application answers the move in its own time, and until it has the
    // window stands on the display it left.
    if (here && card->screen() != here && tries > 0) {
        onThisStageLater(50, [this, card = QPointer(card), behind, tries] {
            if (card) showMovedCard(card, behind, tries - 1);
        });
        return;
    }
    QList<KWin::EffectWindow *> stack;
    for (const auto &member : behind)
        if (member && !member->isDeleted()) stack.append(member.data());
    bool shown = restoreLeftLayout(card, stack);
    if (!shown && here && m_cardStage->presentsBento() && m_desktopStage->hasSessionOnOutput(here->name()))
        shown = m_desktopStage->admitCardToLiveBento(card,
            [this, card = QPointer(card)] { return card && m_cardStage->releaseCardToLiveBento(card); });
    if (shown) keepStackBehind(card, behind, 10);
    else (void)m_cardStage->presentCardActive(card, true);
    KWin::effects->activateWindow(card);
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
}

void Effect::onThisStageLater(int ms, const std::function<void()> &f)
{
    const QString desktop = m_scopedSession ? m_scopedSession->desktopId : QString();
    const QString key = m_scopedKey;
    QTimer::singleShot(ms, this, [this, desktop, key, f] {
        const auto found = m_sessions.find(desktop);
        if (found == m_sessions.end()) return;
        SessionScope scope(this, found->second.get(), key);
        f();
    });
}

long long Effect::lastUsedForDesktopStage(const KWin::EffectWindow *window) const
{
    return window ? static_cast<long long>(m_activationOrder.value(windowIdentity(window))) : 0;
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

void Effect::layoutLeftForOtherDisplayForDesktopStage(KWin::LogicalOutput *output)
{
    // §11, Every display: a display whose layout's last pane was carried to
    // another shows its desktop, its cards waiting in its Spread, so the pane
    // carried back is shown, or takes its side again, as on any display.
    if (!m_cardsOnEveryDisplay || !output || !isTabletOutput(output)
        || m_cardStage->presentation() != CardPresentation::Bento) return;
    m_cardStage->showDesktop();
}

void Effect::displacedCardMayReturnForDesktopStage(KWin::EffectWindow *card, KWin::LogicalOutput *output)
{
    // §5: the card a pane became when an arrival took its slot comes back to
    // the layout once that arrival leaves, as long as it still waits behind
    // the layout as it was left there: not called forward, not asleep, not
    // sent to another display and not behind another card. The window in use
    // stays in use, as the arrival may have gone elsewhere with the person.
    if (!card || card->isDeleted() || card->isMinimized() || !output || m_releasing
        || card->screen() != output || !m_cardStage->presentsBento()
        || m_cardStage->liveCardIndex(card) < 0 || m_cardStage->paneHolding(card)
        || !m_desktopStage->hasSessionOnOutput(output->name())) return;
    const QList<KWin::EffectWindow *> stack = m_cardStage->stackFrontToBack(card);
    if (!stack.isEmpty() && stack.first() != card) return;
    const QPointer<KWin::EffectWindow> back = card;
    if (!m_desktopStage->admitCardToLiveBento(card,
            [this, back] { return back && m_cardStage->releaseCardToLiveBento(back); }, true, false)) {
        qInfo() << "Kadunce" << Revision << "kept" << logNameOf(card) << "waiting behind the layout on"
                << output->name() << "with no room for it there";
        return;
    }
    // It was lowered under the panes as a hidden card. A pane again, it stands
    // above the cards still hidden there, or a press on it would reach one.
    if (back && back->window()) KWin::workspace()->raiseWindow(back->window());
    if (stack.size() > 1) m_cardStage->notePaneStack(card, stack.mid(1));
    m_cardStage->settlePaneStacks();
    qInfo() << "Kadunce" << Revision << "brought" << logNameOf(card) << "back to the layout on"
            << output->name() << "with" << qMax<qsizetype>(stack.size() - 1, 0)
            << "cards behind it, as the pane that took its place leaves";
    Q_EMIT workspaceContextChanged();
}

void Effect::prepareOutputForDesktopStage(KWin::LogicalOutput *output)
{
    if (output && isTabletOutput(output) && m_cardStage->isActive()) {
        qInfo() << "Kadunce" << Revision << "lets the cards go for a layout across" << output->name();
        m_cardStage->release(); // Do not release Bento sessions on other displays.
    }
}

std::optional<NativeMoveSnapshot> Effect::activeRestoreForDesktopStage(KWin::EffectWindow *window) const
{
    return m_cardStage->managedRestore(window);
}

bool Effect::admitDisplacedPaneToTablet(KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore)
{
    // §5: while the display presents its layout, the pane that yielded becomes
    // a nonselected card behind it. Anywhere else there is no layout in front
    // of it, and an ordinary transfer is the right arrival. A layout that holds
    // every window there owns no card yet, so the pane that yields starts card
    // ownership behind it, as any arrival behind a layout does.
    std::optional<SessionScope> scope;
    if (output) scope.emplace(this, m_scopedSession, stageKeyFor(output));
    KWin::LogicalOutput *tablet = tabletOutput();
    const bool behindLayout = !m_cardStage->isActive() && tablet
        && m_desktopStage->hasSessionOnOutput(tablet->name()) && m_cardStage->startBehindLayout();
    if (m_cardStage->admitDisplacedPaneAsHiddenCard(window, commitSource, restore))
        return true;
    if (behindLayout) m_cardStage->stopBehindLayoutIfEmpty();
    return admitTransferredWindowToTablet(window, output, commitSource, restore);
}

void Effect::settleStacksOfArrival()
{
    // A pane that is a card again stands in front of its Stack in Spread's
    // row. The transfer that made it one is still under way, so the Stack is
    // settled the turn after, once the card is in the row.
    onThisStageLater(0, [this] {
        if (m_releasing || !m_cardStage) return;
        m_cardStage->settlePaneStacks();
        observeCardOwnership();
    });
}

CardStageController *Effect::carrySourceStage(const PreparedCarrySource &source) const
{
    if (m_cardStage && m_cardStage->nativeCarrySourceValid(source)) return m_cardStage;
    if (!m_scopedSession) return nullptr;
    for (const auto &display : m_scopedSession->displays)
        if (display.second->cards->nativeCarrySourceValid(source)) return display.second->cards.get();
    return nullptr;
}

bool Effect::cardCarrySourceValid(const PreparedCarrySource &source) const
{
    return carrySourceStage(source) != nullptr;
}

bool Effect::admitSleepingPaneToTablet(KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore)
{
    // Unlike the awake door, this one cannot find the record for itself: it
    // would prepare a carry, and a carry refuses a minimized window. The caller
    // still holds the session that has it, so it supplies it, and without it
    // the state §13 restores the window to would be the one the user just asked
    // for -- release would re-minimize a window it had woken.
    if (!restore) return false;
    std::optional<SessionScope> scope;
    if (output) scope.emplace(this, m_scopedSession, stageKeyFor(output));
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
    m_cardStage->followWorkArea(true);
    forEachDisplayStage([](CardStageController &stage) { stage.followWorkArea(true); });
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
    if (!callerMayDrive()) return;
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
    forEachDisplayStage([&](CardStageController &stage) {
        stage.keyboardHeading(keys.y() + keys.height() - height, durationMs);
    });
}

void Effect::raiseKeyboard()
{
    if (!callerMayDrive()) return;
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

void Effect::tapEndedForInput(const KWin::Window *window)
{
    if (!window || !m_inputRouter) return;
    // An application that asks for the keys itself does so as the finger
    // lifts; this waits long enough for it to have done so.
    constexpr int OwnAskMs = 250;
    QTimer::singleShot(OwnAskMs, this,
                       [this, tapped = QPointer<KWin::Window>(const_cast<KWin::Window *>(window)),
                        events = m_inputRouter->touchEvents()] { raiseKeysForTerminalTap(tapped, events); });
}

void Effect::raiseKeysForTerminalTap(QPointer<KWin::Window> window, quint64 touchEvents)
{
    // Another touch since belongs to whatever that touch does.
    if (!window || !m_inputRouter || m_inputRouter->touchEvents() != touchEvents
        || m_inputRouter->latestTouchKept()) return;
    KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
    if (!method || !method->isEnabled() || method->isVisible()) return;
    KWin::TextInputV3Interface *input = KWin::waylandServer()->seat()->textInputV3();
    if (!input || !input->isEnabled()
        || input->contentPurpose() != KWin::TextInputContentPurpose::Terminal) return;
    const KWin::Window *target = method->activeWindow();
    if (!target || (target != window.data() && !window->hasTransient(target, true))) return;
    raiseKeyboard();
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
    // A field that asks within a moment of a tap in its own window, made while
    // the keys were up, is the one tapped. The cursor a browser reports then
    // cannot be read: moving into a box in a frame from another site, it gives
    // the last box's, or this one's from before the window last changed size.
    // With the keys down the tap may have been on a link or a list, and a
    // page that opens focuses its own box; only the cursor can tell.
    constexpr qint64 TapJustNowMs = 1000;
    if (m_keysUpAtTouch && m_inputRouter && m_inputRouter->msSinceLatestTouch() < TapJustNowMs
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
                      << (target ? logNameOf(target) : QStringLiteral("no window"))
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
    forEachDisplayStage([](CardStageController &stage) { stage.refreshKeyboardRoom(); });
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
    m_lineDestination.reset(); m_lineDestinationWindow.clear();
    if (m_carryRuntime) {
        // Motion under way carries on: each glide checks on every frame that
        // its window is still presented where it draws it, and stops when
        // not. A carry this ends goes back with a presentation changing under
        // it, so it does not glide home.
        const bool carrying = bool(m_carriedWindow);
        {
            QScopedValueRollback<bool> outlives(m_motionOutlivesInterrupt, !carrying);
            m_carryRuntime->cancel();
        }
        if (carrying) clearDropSettle();
    }
    m_inputActivationGuard.invalidate();
    if (m_inputRouter) m_inputRouter->cancelWorkspaceInteraction();
}

void Effect::connectManagedWindowForCardStage(KWin::EffectWindow *window)
{
    connectManagedWindow(window);
}

namespace {
// The longest a resized card keeps its earlier picture, should its app be
// slow to draw at the new size.
constexpr int HeldPictureLimitMs = 300;
}

namespace {
// A rectangle as the journal and workspace context write it.
QString rectText(const QRectF &rect)
{
    return QStringLiteral("%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

// Where an offered card's picture stands in its tile: in its own shape, as
// large as the tile allows, centred.
QRectF offerPicture(const QRectF &tile, const QSizeF &card)
{
    const QSizeF fitted = card.scaled(tile.size(), Qt::KeepAspectRatio);
    return QRectF(tile.center() - QPointF(fitted.width() / 2, fitted.height() / 2), fitted);
}
}

bool Effect::holdPictureBeforeResizeForCardStage(KWin::EffectWindow *window,
                                                 const std::function<void()> &resize)
{
    if (!window || !resize || !KWin::effects->isOpenGLCompositing()
        || m_heldPictures.count(window)) return false;
    auto &held = m_heldPictures[window];
    held.resize = resize;
    held.damaged = connect(window, &KWin::EffectWindow::windowDamaged, this, [this](KWin::EffectWindow *damaged) {
        const auto it = m_heldPictures.find(damaged);
        if (it == m_heldPictures.end() || it->second.resize || !it->second.picture.taken()) return;
        if (QSizeF(damaged->bufferGeometry().size()) != it->second.bufferAtCapture) ++it->second.drawnSince;
    });
    KWin::effects->addRepaintFull();
    // Whatever the paints do, the card is resized and its picture let go.
    const QPointer<KWin::EffectWindow> guard(window);
    QTimer::singleShot(HeldPictureLimitMs, this, [this, guard] {
        if (!guard) return;
        const auto it = m_heldPictures.find(guard.data());
        if (it == m_heldPictures.end()) return;
        auto resize = std::move(it->second.resize);
        releaseHeldPicture(guard.data());
        if (resize) resize();
        KWin::effects->addRepaintFull();
    });
    return true;
}

void Effect::releaseHeldPicture(KWin::EffectWindow *window)
{
    const auto it = m_heldPictures.find(window);
    if (it == m_heldPictures.end()) return;
    disconnect(it->second.damaged);
    m_heldPictures.erase(it);
}

void Effect::captureOfferPictures()
{
    for (const auto &card : std::as_const(m_roomOffer->cards)) {
        if (!card || card->isDeleted() || m_offerPictures.contains(card.data())) continue;
        KWin::EffectWindow *window = card.data();
        m_capturingHeld = window;
        KeptPicture picture;
        const bool taken = picture.take(window,
            [&](const KWin::RenderTarget &target, const KWin::RenderViewport &viewport) {
                KWin::WindowPaintData data;
                data.setOpacity(1.0);
                (void)painted([&] { return KWin::effects->drawWindow(target, viewport, window,
                    PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT, KWin::Region::infinite(), data); });
            }, true);
        m_capturingHeld = nullptr;
        if (!taken) continue;
        m_offerPictures.emplace(window, std::move(picture));
        // The next frame draws it.
        KWin::effects->addRepaintFull();
    }
}

void Effect::captureHeldPictures()
{
    for (auto &[window, held] : m_heldPictures) {
        if (!held.resize || held.picture.taken() || window->isDeleted()) continue;
        m_capturingHeld = window;
        const bool taken = held.picture.take(window,
            [&](const KWin::RenderTarget &target, const KWin::RenderViewport &viewport) {
                KWin::WindowPaintData data;
                data.setOpacity(1.0);
                (void)painted([&] { return KWin::effects->drawWindow(target, viewport, window,
                    PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT, KWin::Region::infinite(), data); });
            });
        m_capturingHeld = nullptr;
        if (!taken) continue;
        held.bufferAtCapture = window->bufferGeometry().size();
        // Resized once the picture is kept, outside the paint.
        QTimer::singleShot(0, this, [this, guard = QPointer<KWin::EffectWindow>(window)] {
            if (!guard) return;
            const auto it = m_heldPictures.find(guard.data());
            if (it == m_heldPictures.end() || !it->second.resize) return;
            auto resize = std::move(it->second.resize);
            it->second.resize = nullptr;
            resize();
        });
    }
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
    // §11, Every display: a card leaving for another display holding its own
    // cards and showing no layout is that display's card, shown in front.
    if (isOtherCardDisplay(output) && !m_desktopStage->hasSessionOnOutput(output->name())) {
        const auto restore = m_cardStage->managedRestore(window);
        const QList<KWin::EffectWindow *> behind = cardsBehind(*m_cardStage, window);
        SessionScope there(this, m_scopedSession, stageKeyFor(output));
        if (!admitTransferredWindowToTablet(window, output, commitSource, restore ? &*restore : nullptr))
            return false;
        releaseSource();
        rejoinStack(window, behind);
        return true;
    }
    if (m_carryDestination && window == m_carriedWindow) {
        // §11: carried into the layout another display shows, its Stack
        // follows it there.
        const QList<KWin::EffectWindow *> behind = isOtherCardDisplay(output)
            ? cardsBehind(*m_cardStage, window) : QList<KWin::EffectWindow *>{};
        if (!m_desktopStage->transferPreparedCard(*m_carryDestination, commitSource, releaseSource)) return false;
        if (!behind.isEmpty()) {
            SessionScope there(this, m_scopedSession, stageKeyFor(output));
            rejoinStack(window, behind);
        }
        return true;
    }
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

void Effect::settleChosenCardForInput()
{
    if (m_cardStage->finishGrowToActive()) observeCardOwnership();
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
    // A touchscreen driving another display has that display's own edges to
    // start from, whichever mode the display is in.
    KWin::LogicalOutput *touched = KWin::effects->findScreen(m_touchOutputName);
    WorkspaceInputGeometry geometry{
        QRectF(tabletRect),
        QRectF(centerCard),
        double(tabletRect.bottom()),
        double(centerCard.right()),
    };
    if (touched) geometry.touchscreen = {QRectF(touched->geometry()), double(touched->geometry().bottom())};
    return geometry;
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
    // The swipe opens the Spread of the display it rises on.
    m_spreadGesture = {};
    m_spreadGesture.key = m_scopedKey;
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
    closeLauncherGuest();
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
    return pairCardsIntoBento(output, active, m_cardStage->partnerForSideSnap(active, walkLeft), side);
}

bool Effect::placeActiveCardAlone(KWin::LogicalOutput *output, BentoSidePlacement side)
{
    auto *active = m_cardStage->activeCardIdentity();
    if (!output || !active || !isTabletOutput(output)) return false;
    const auto reserved = m_desktopStage->prepareCardDrop(active, output,
        KWin::RectF(active->frameGeometry()),
        DesktopStageController::CardDropIntent::ActivateBento, side, nullptr, true);
    if (!reserved) return false;
    // Its Stack waits behind the pane it becomes, so the keys carry it whole.
    QList<KWin::EffectWindow *> behind = m_cardStage->stackFrontToBack(active);
    behind.removeAll(active);
    QPointer<KWin::EffectWindow> carried = active;
    const bool placed = m_desktopStage->activatePreparedTabletDrop(*reserved, nullptr,
        [this, carried] { return m_cardStage->releasePairToBento(carried, nullptr); });
    if (placed && carried && !behind.isEmpty()) {
        m_cardStage->notePaneStack(carried, behind);
        m_cardStage->settlePaneStacks();
        qInfo() << "Kadunce" << Revision << "placed" << logNameOf(carried) << "alone with"
                << behind.size() << "behind it";
    }
    // The room left open shows the desktop, not the cards.
    if (placed) m_cardStage->syncSelectedElevation();
    return placed;
}

void Effect::layOutHeldLayoutBeside(KWin::EffectWindow *card, KWin::LogicalOutput *output,
    BentoSidePlacement side, const QRectF &drawn)
{
    if (!card || card->isDeleted() || !output || !isTabletOutput(output)) return;
    m_aloneOnPurpose.removeAll(card);
    const QPointer<KWin::EffectWindow> placed = card;
    if (m_cardStage->activeCardIdentity() == card && m_cardStage->stepActiveBackToLayout()
        && playPickedCardIntoLayout(card) && placed && m_desktopStage->managesWindow(card)) {
        (void)m_desktopStage->placePaneBySide(card, output, side);
        startPaneArrival(card, output, drawn);
        qInfo() << "Kadunce" << Revision << "laid the layout on" << output->name() << "out again with"
                << logNameOf(card) << "at the" << (side.right ? "right" : "left") << "edge";
    } else if (placed && m_cardStage->activeCardIdentity() == card && !m_cardStage->hasBentoGroup()
               && placeActiveCardAlone(output, side)) {
        // The layout had no room for it and stays in Spread.
        startPaneArrival(card, output, drawn);
    } else {
        (void)m_cardStage->presentSelectedNow();
        qInfo() << "Kadunce" << Revision << "could not lay the layout on" << output->name()
                << "out again with" << (placed ? logNameOf(card) : QStringLiteral("a closed card"));
    }
    observeCardOwnership();
}

bool Effect::pairCardsIntoBento(KWin::LogicalOutput *output, KWin::EffectWindow *active,
    KWin::EffectWindow *partner, BentoSidePlacement side)
{
    if (!output || !active || !partner || partner == active) return false;
    const auto reserved = m_desktopStage->prepareCardDrop(active, output,
        KWin::RectF(active->frameGeometry()),
        DesktopStageController::CardDropIntent::ActivateBento, side, partner);
    if (!reserved) return false;
    QPointer<KWin::EffectWindow> carried = active;
    QPointer<KWin::EffectWindow> named = reserved->namedPartner();
    // Each card's Stack waits behind the pane it becomes, as a card placed
    // alone keeps its own (placeActiveCardAlone), so no card of it is laid
    // out as a pane of its own after.
    const auto behindOf = [this](KWin::EffectWindow *card) {
        QList<KWin::EffectWindow *> behind = card ? m_cardStage->stackFrontToBack(card) : QList<KWin::EffectWindow *>{};
        behind.removeAll(card);
        return behind;
    };
    const QList<KWin::EffectWindow *> activeBehind = behindOf(active);
    const QList<KWin::EffectWindow *> partnerBehind = behindOf(named);
    if (!m_desktopStage->activatePreparedTabletDrop(*reserved, nullptr,
            [this, carried, named] { return m_cardStage->releasePairToBento(carried, named); }))
        return false;
    if (carried && !activeBehind.isEmpty()) m_cardStage->notePaneStack(carried, activeBehind);
    if (named && !partnerBehind.isEmpty()) m_cardStage->notePaneStack(named, partnerBehind);
    if (!activeBehind.isEmpty() || !partnerBehind.isEmpty()) m_cardStage->settlePaneStacks();
    return true;
}

void Effect::toggleBento()
{
    m_letGo = false;
    // §10: Meta+B lays a card the top edge left alone out with the rest.
    m_aloneOnPurpose.clear();
    if (m_carryRuntime) m_carryRuntime->cancel();
    // CARD-LIFECYCLE.md §3: external Bento while docked, tablet Bento while
    // undocked. This read the pointer before, which on a touch tablet is
    // wherever the pointer was last left rather than where the user is working.
    // With cards on every display, the other displays show one card each, so
    // the key belongs to Spread's display.
    KWin::LogicalOutput *target = m_cardsOnEveryDisplay ? nullptr : externalDesktopOutput();
    if (!target) target = tabletOutput();
    if (!target) return;
    // §3 names a pair on a display that can own cards, so a Bento action there
    // composes two named windows rather than sweeping the display. A display
    // that cannot own cards reaches neither pairing rule and composes across
    // itself, which is what an external desktop stage is for.
    if (!m_desktopStage->hasSessionOnOutput(target->name())
        && m_cardStage->canOwnCards(target)) {
        // §11, Every display: windows given back there are cards again first.
        if (m_cardsOnEveryDisplay && !m_cardStage->isActive()) (void)startTabletInCards(nullptr);
        qInfo() << "Kadunce" << Revision << "Meta+B on" << target->name() << "with no layout";
        const QPointer<KWin::EffectWindow> active = m_cardStage->activeCardIdentity();
        if (!pairActiveCardIntoBento(target)) {
            // §11, Connected: the layout Spread holds, standing behind the
            // Active card, is laid out again with that card in it, as picking
            // the card from Spread would; its panes are no partners for it.
            if (active && connectedOutputForDesktopStage(target)
                && m_cardStage->stepActiveBackToLayout()) {
                if (playPickedCardIntoLayout(active)) {
                    qInfo() << "Kadunce" << Revision << "laid the layout on" << target->name()
                            << "out again with" << logNameOf(active) << "in it";
                } else {
                    (void)m_cardStage->presentSelectedNow();
                    qInfo() << "Kadunce" << Revision << "could not lay the layout on" << target->name()
                            << "out again with" << logNameOf(active) << "in it";
                }
                observeCardOwnership();
                return;
            }
            qInfo() << "Kadunce" << Revision << "found no pair for Bento on" << target->name()
                    << "with cards" << (m_cardStage->isActive() ? "shown" : "not shown")
                    << "and" << (active ? "an" : "no") << "Active card";
        } else if (connectedOutputForDesktopStage(target)) {
            // §11, Connected: Bento auto-fill is the layout. The pair grows
            // to hold every open card of the display, as far as it has room.
            QTimer::singleShot(0, this, [this, target = QPointer(target)] { fillLayoutWithCards(target); });
        }
        observeCardOwnership();
        return;
    }
    // The panes a layout the key ends had, and the one in use.
    QList<QPointer<KWin::EffectWindow>> former = m_desktopStage->panesOn(target->name());
    QPointer<KWin::EffectWindow> inUse = KWin::effects->activeWindow();
    if (!former.contains(inUse)) inUse = former.isEmpty() ? nullptr : former.first();
    // §11, Connected: a layout standing behind the Active card, as one does
    // until Spread can hold what the top edge left, is not what the display
    // shows. The key lays it out again with that card in it, at
    // one press, rather than ending what was not shown.
    const QPointer<KWin::EffectWindow> inFront = m_cardsOnEveryDisplay && m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Active && connectedOutputForDesktopStage(target)
        ? m_cardStage->activeCardIdentity() : nullptr;
    qInfo() << "Kadunce" << Revision << "Meta+B on" << target->name() << "with a layout of"
            << former.size() << (inFront ? QString(QStringLiteral("behind ") + logNameOf(inFront)) : QStringLiteral("shown"));
    (void)m_desktopStage->toggleOnOutput(target->name());
    observeCardOwnership();
    // §11, Every display: a layout ended by the key leaves its windows as
    // cards, the one in use Active, so the key pairs them again. With cards
    // already held behind the layout, its panes join them.
    if (!m_cardsOnEveryDisplay || !m_cardStage->canOwnCards(target)) return;
    // Each pane's Stack is a Stack in the row again, whole, so the key lays
    // out only its face.
    onThisStageLater(0, [this, former, inUse, inFront, target = QPointer(target)] {
        if (!m_cardStage->isActive()) {
            (void)startTabletInCards(inUse);
            m_cardStage->settlePaneStacks();
            return;
        }
        if (!target || m_desktopStage->hasSessionOnOutput(target->name())) return;
        for (const auto &window : former)
            if (window && !window->isDeleted() && window->screen() == target
                && m_cardStage->liveCardIndex(window) < 0)
                (void)m_cardStage->admitArrivalAsCard(window);
        m_cardStage->settlePaneStacks();
        KWin::EffectWindow *front = inFront && !inFront->isDeleted()
            && m_cardStage->liveCardIndex(inFront) >= 0 ? inFront.data() : nullptr;
        KWin::EffectWindow *active = front ? front : inUse.data();
        if (active && m_cardStage->liveCardIndex(active) >= 0) (void)m_cardStage->promoteToActive(active);
        qInfo() << "Kadunce" << Revision << "ended the layout on" << target->name()
                << "into cards," << (active ? logNameOf(active) : QString()) << "Active";
        if (front && pairActiveCardIntoBento(target)) {
            qInfo() << "Kadunce" << Revision << "laid the layout on" << target->name()
                    << "out again with" << logNameOf(front) << "in it";
            QTimer::singleShot(0, this, [this, target] { fillLayoutWithCards(target); });
        }
        observeCardOwnership();
        Q_EMIT workspaceContextChanged();
    });
}

void Effect::fillLayoutWithCards(KWin::LogicalOutput *output)
{
    if (!output || m_releasing || !m_cardStage->presentsBento()
        || !m_desktopStage->hasSessionOnOutput(output->name())) return;
    // The front of each Stack takes a pane; the cards behind it wait there.
    QList<KWin::EffectWindow *> fronts;
    for (const auto &card : m_cardStage->liveCards()) {
        if (!card || card->isDeleted() || card->isMinimized()
            || card->screen() != output || m_cardStage->paneHolding(card)) continue;
        const auto stack = m_cardStage->stackFrontToBack(card);
        if (!stack.isEmpty() && stack.first() != card) continue;
        fronts.append(card);
    }
    int filled = 0;
    for (KWin::EffectWindow *card : std::as_const(fronts)) {
        const QPointer<KWin::EffectWindow> front = card;
        if (!front || front->isDeleted() || m_cardStage->liveCardIndex(front) < 0) continue;
        const QList<KWin::EffectWindow *> behind = m_cardStage->stackFrontToBack(front).mid(1);
        if (!m_desktopStage->admitCardToLiveBento(front,
                [this, front] { return m_cardStage->releaseCardToLiveBento(front); }, true)) break;
        if (!behind.isEmpty()) m_cardStage->notePaneStack(front, behind);
        ++filled;
    }
    if (filled == 0) return;
    m_cardStage->settlePaneStacks();
    qInfo() << "Kadunce" << Revision << "filled the layout on" << output->name() << "with"
            << filled << "more cards";
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
}

void Effect::handleManagedWindowMinimizedChanged()
{
    auto *client = qobject_cast<KWin::Window *>(sender());
    const QPointer<KWin::EffectWindow> guarded = client ? client->effectWindow() : nullptr;
    if (!guarded) return;
    DesktopSession *holder = sessionHolding(guarded);
    SessionScope scope(this, holder ? holder : m_currentSession);
    m_desktopStage->handleWindowMinimizedChanged(guarded);
    // §3: a display that can own cards, holding none, takes a window picked
    // back from minimized as its Active card, as it takes a window that opens
    // there. Only such a display does: one holding no cards keeps its windows
    // native until a snap or a key asks.
    KWin::LogicalOutput *there = guarded->screen();
    if (m_releasing || guarded->isMinimized() || !isCardDisplay(there)) return;
    SessionScope own(this, m_scopedSession, stageKeyFor(there));
    if (!m_cardStage->isActive()) (void)startTabletInCards(guarded);
}

void Effect::connectManagedWindow(KWin::EffectWindow *window)
{
    if (!window) {
        return;
    }
    if (window->window()) {
        // A window is connected again each time a stage admits it; a unique
        // connection keeps one handler however often that is.
        connect(window->window(), &KWin::Window::minimizedChanged, this,
            &Effect::handleManagedWindowMinimizedChanged, Qt::UniqueConnection);
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
        if (const QPointer<KWin::XdgToplevelInterface> toplevel = nativeMoveProtocol(window->window()->surface()))
            connect(toplevel.data(), &KWin::XdgToplevelInterface::minimumSizeChanged,
                    this, &Effect::handleMinimumSizeChanged, Qt::UniqueConnection);
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

QStringList Effect::namesBehindInSpread(int cardId) const
{
    QStringList names;
    const auto &cards = m_cardStage->liveCards();
    const auto order = m_cardStage->model().stackPaintOrderForId(cardId);
    // Painted farthest first, the face last.
    for (auto id = order.crbegin(); id != order.crend(); ++id) {
        if (id == order.crbegin()) continue;
        KWin::EffectWindow *card = *id >= 1 && *id <= cards.size() ? cards.at(*id - 1).data() : nullptr;
        if (card && !card->isDeleted()) names.append(applicationDisplayName(card));
    }
    return names;
}

QString Effect::spreadCardName(KWin::EffectWindow *window, int cardId) const
{
    const QStringList behind = namesBehindInSpread(cardId);
    if (!behind.isEmpty()) return stackedApplicationName(applicationDisplayName(window), behind);
    QStringList partners;
    for (auto *pane : m_cardStage->layoutPartners(window)) partners.append(applicationDisplayName(pane));
    return pairedApplicationName(applicationDisplayName(window), partners);
}

QString Effect::applicationDisplayName(KWin::EffectWindow *window) const
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
    // Answered by the stage holding the window, else its display's.
    SessionScope scope(this, m_scopedSession, stageKeyOf(window));
    noteDisplayInUse(window ? window->screen() : nullptr);
    if (window && window == m_letGoOnDisplay) m_letGoOnDisplay.clear();
    if (window && window->window()) {
        qInfo() << "Kadunce sees KWin begin a" << (window->isUserResize() ? "resize" : "move")
                << "of" << window->window()->resourceClass()
                << (window->isUserMove() || window->isUserResize() ? "by hand" : "by itself")
                << "at" << window->window()->frameGeometry();
        if (m_touchWitness) m_touchWitness->moveStarted();
    }
    // A card does not become an ordinary window through a resize grip.
    // Defer cancellation until KWin has finished publishing native-start; never
    // tear down its transaction recursively inside that signal.
    if (window && window->window() && window->isUserResize()
        && m_cardStage->managedRestore(window)) {
        QPointer<KWin::EffectWindow> guarded = window;
        QTimer::singleShot(0, this, [this, guarded] {
            CardStageController *cards = guarded ? stageHolding(guarded) : nullptr;
            if (!guarded || !guarded->window() || !guarded->isUserResize()
                || !cards || !cards->managedRestore(guarded)) return;
            const auto *touch = KWin::input() ? KWin::input()->touch() : nullptr;
            StaleTouchRelease::cancel(guarded->window(), touch && touch->touchPointCount() > 0);
        });
        return;
    }
    traceNativeMove(window, "native-start");
    if (window == m_settlingWindow) clearDropSettle();
    if (m_carryRuntime && m_carryRuntime->route.busy()) m_carryRuntime->cancel();
    if (m_carryRuntime && window->window() && window->isUserMove()
        && !m_carryRuntime->route.busy()) {
        CardStageController *cards = m_cardStage;
        auto source = cards->prepareNativeCarrySource(window);
        const bool fromBento = !source;
        if (!source) source = m_desktopStage->prepareNativeCarrySource(window);
        if (source) {
            m_carryPickup = QRectF(window->frameGeometry());
            QPointer<KWin::EffectWindow> guarded = window;
            if (m_carryRuntime->handoff.stage(window->window(), *source,
                [this, saved = *source, fromBento] {
                    return fromBento ? m_desktopStage->nativeCarrySourceValid(saved)
                                     : cardCarrySourceValid(saved);
                }, [this, guarded] { if (guarded) beginLegacyNativeMove(guarded); })) {
                traceNativeMove(window, source->isDesktopWindow() ? "staged-ordinary" : "staged-card");
                qInfo() << "Kadunce" << Revision << "carries" << applicationIdentity(window)
                        << "by its title bar as"
                        << (!fromBento ? "the Active card"
                            : source->isDesktopWindow() ? "an ordinary window" : "a Bento pane");
                return;
            }
            traceNativeMove(window, "stage-refused");
            qInfo() << "Kadunce" << Revision << "leaves the drag of" << applicationIdentity(window)
                    << "to KWin: the carry could not be staged";
        } else {
            traceNativeMove(window, "no-carry-source");
            // A drag Kadunce does not carry is KWin's own move, and edges do
            // nothing Kadunce's. Say why, so a drag that did nothing is legible.
            qInfo() << "Kadunce" << Revision << "leaves the drag of" << applicationIdentity(window)
                    << "to KWin: as a card," << cards->nativeCarryRefusal(window)
                    << "; as a pane," << m_desktopStage->nativeCarryRefusal(window);
        }
    }
    beginLegacyNativeMove(window);
}

void Effect::beginLegacyNativeMove(KWin::EffectWindow *window)
{
    traceNativeMove(window, "native-fallback");
    SessionScope scope(this, m_scopedSession, stageKeyOf(window));
    if (isApplicationWindow(window) && window->isUserMove()) {
        m_nativeCarry = window;
        m_nativeCarrySource = window->screen() ? window->screen()->name() : QString();
        m_nativeCarryFromBento = m_desktopStage->managesWindow(window);
    }
    // An application may start a move on a tap in its own title area, which
    // then ends where it began. The cards are let go only once the Active card
    // travels, so a tap leaves them as they were.
    if (window->isUserMove() && !window->isUserResize()
        && m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Active
        && m_cardStage->selectedWindow() == window) {
        m_heldActiveMove = window;
        m_heldActiveMoveOrigin = QRectF(window->frameGeometry()).topLeft();
        qInfo() << "Kadunce" << Revision << "keeps the cards while" << applicationIdentity(window)
                << "has not moved yet";
    } else {
        m_heldActiveMove.clear();
        m_cardStage->handleManualWindowChange(window);
    }
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
    m_lastEdgeEntryLog.clear();
    if (m_carriedWindow) {
        KWin::effects->setElevatedWindow(m_carriedWindow, false);
        unredirect(m_carriedWindow);
    }
    m_carriedWindow.clear(); m_carryDestination.reset(); m_carryPreview.reset();
    syncNotesPause();
    m_stackLeanTo = false; m_stackLeanFrom = 0.0; m_stackLeanClock.invalidate();
    m_carryWidthEdge = CarryWidthEdge::None; m_carryWidth = 0.0; m_carryWidthClock.invalidate();
    m_carryWidthHeld = false;
    m_carryCardEntryOutput.clear(); m_carryCardExitOutput.clear();
    m_carryStackOnto.clear(); m_paneStackAim.clear(); m_paneStackDwell.stop();
    endStackScrub();
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
    QJsonArray yieldingPanes;
    for (const auto &yielding : m_yieldingPanes) {
        const auto opacity = yieldingPaneOpacity(yielding.window);
        if (opacity) yieldingPanes.append(QJsonObject{
            {QStringLiteral("window"), windowIdentity(yielding.window)},
            {QStringLiteral("rect"), geometryContext(KWin::RectF(yielding.rect).toRect())},
            {QStringLiteral("opacity"), *opacity}});
    }
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
        {QStringLiteral("dropWindow"), settling ? windowIdentity(m_settlingWindow) : QString()},
        {QStringLiteral("dropTarget"), settling ? geometryContext(KWin::RectF(m_settleTo).toRect()) : QJsonObject{}},
        {QStringLiteral("dropStarted"), settling && m_dropSettleTimer.isValid()},
        {QStringLiteral("yieldingPanes"), yieldingPanes},
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
        // §11, Connected: the pane a carried pane would join in a Stack.
        {QStringLiteral("stackOnto"), m_carryStackOnto ? windowIdentity(m_carryStackOnto) : QString()},
        // How far the carried window leans into that Stack, 0 to 1.
        {QStringLiteral("stackLean"), m_carryStackOnto ? stackLeanNow() : 0.0},
        {QStringLiteral("carryWidth"), m_carryWidth},
        // §11, Connected: the cards the room a side snap left open offers,
        // in order, and where each picture stands.
        {QStringLiteral("roomOffer"), [this] {
            QJsonObject offer;
            if (!m_roomOffer) return offer;
            QJsonArray cards, tiles;
            for (const auto &card : m_roomOffer->cards) cards.append(windowIdentity(card));
            for (const auto &tile : m_roomOffer->tiles) tiles.append(geometryContext(KWin::RectF(tile).toRect()));
            offer.insert(QStringLiteral("output"), m_roomOffer->output ? m_roomOffer->output->name() : QString());
            offer.insert(QStringLiteral("room"), geometryContext(KWin::RectF(m_roomOffer->room).toRect()));
            offer.insert(QStringLiteral("cards"), cards);
            offer.insert(QStringLiteral("tiles"), tiles);
            return offer;
        }()},
        // §11, Connected: the place a scrubbed face would go in at, and the
        // cards its ladder shows, front first.
        {QStringLiteral("scrubPlace"), m_stackScrub.active() ? m_stackScrub.place : -1},
        {QStringLiteral("scrubLadder"), [this] {
            QJsonArray ladder;
            if (!m_stackScrub.active() || !m_stackScrubPane) return ladder;
            const auto order = stackScrubOrder(m_stackScrubPane.data(),
                m_cardStage->paneStack(m_stackScrubPane), m_stackScrub.place);
            for (int i = m_stackScrub.ladderStart();
                 i < std::min<int>(order.size(), m_stackScrub.ladderStart() + m_stackScrub.ladderCount()); ++i)
                ladder.append(windowIdentity(order.at(i)));
            return ladder;
        }()},
        {QStringLiteral("titleBarHold"), KWin::input() && KWin::input()->touch()
            ? KWin::input()->touch()->decorationPressId() : -1},
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
        {QStringLiteral("carryScale"), m_cardStage->carryScale()}
    }).toJson(QJsonDocument::Compact));
}

void Effect::updateNativeCarryDestination(QPointF contact)
{
    // A Stack being scrubbed has no destination: it goes back in its pane.
    if (m_stackScrub.active()) return;
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
    m_carryRefusal.reset();
    m_carryStackOnto.clear();
    m_lastCarryContact = contact;
    // The aim is read again below only where a pane can be stacked on.
    bool aimingPane = false;
    const auto forgetPaneAim = qScopeGuard([this, &aimingPane] {
        if (aimingPane) return;
        m_paneStackAim.clear();
        m_paneStackDwell.stop();
    });
    if (!m_carriedWindow || !m_carryRuntime->handoff.source()) return;
    auto &handoff = m_carryRuntime->handoff;
    handoff.withdrawDrop(); // Leaving an exit/edge must retire its commit callback too.
    KWin::LogicalOutput *target = nullptr;
    for (auto *o : KWin::effects->screens())
        if (QRectF(o->geometry()).contains(contact)) { target = o; break; }
    KWin::effects->addRepaintFull();
    if (!target) return;
    // §11, Every display: what the carry would do there is asked of the
    // stage of the display under the contact.
    SessionScope scope(this, m_scopedSession, stageKeyFor(target));
    // The drop is checked and committed on the same display's stage.
    const auto previewDrop = [this, &handoff, desktop = m_scopedSession->desktopId, key = m_scopedKey](
        CarryDestination destination, std::function<bool()> valid,
        std::function<bool(const PreparedCarrySource &)> commit) {
        return handoff.previewDrop(std::move(destination),
            [this, desktop, key, valid = std::move(valid)] {
                const auto found = m_sessions.find(desktop);
                if (found == m_sessions.end()) return false;
                SessionScope there(this, found->second.get(), key);
                return valid();
            },
            [this, desktop, key, commit = std::move(commit)](const PreparedCarrySource &source) {
                const auto found = m_sessions.find(desktop);
                if (found == m_sessions.end()) return false;
                SessionScope there(this, found->second.get(), key);
                return commit(source);
            });
    };
    const bool bento = m_desktopStage->nativeCarrySourceValid(*handoff.source());
    const bool desktopWindow = handoff.source()->isDesktopWindow();
    const bool local = target->name() == handoff.source()->origin().output;
    const bool tablet = isTabletOutput(target);
    auto edge = carryEdgeAt(target, contact);
    // §11, Connected: a wheel turned while held away from the edges asks what
    // the top edge would, until the hand reaches a side or leaves the
    // monitors; on another display it is asked of the arrival below.
    if (m_carryWidthHeld && (!connectedOutputForDesktopStage(target) || (edge && *edge != CarryEdge::Top)))
        m_carryWidthHeld = false;
    if (m_carryWidthHeld && !edge && local) edge = CarryEdge::Top;
    // Every return below that leaves an edge without a drop names why, and a
    // drop that is previewed but then refused at release cannot go there.
    std::optional<GestureRefusal> refusal;
    const auto noteRefusal = qScopeGuard([&] {
        if (!edge) return;
        if (handoff.hasDrop()) refusal = GestureRefusal::Unplaced;
        if (refusal) m_carryRefusal = CarryRefusal{target, contact, *refusal};
    });
    std::optional<BentoSidePlacement> side;
    if (edge && (*edge == CarryEdge::Left || *edge == CarryEdge::Right))
        side = sideChoiceOn(target, *edge == CarryEdge::Right, contact.y(), previousDestination
                && previousDestination->destinationOutput() == target
                ? previousDestination->sidePlacement() : std::nullopt);
    // §11, Connected: the contact names a place, over, beside or under.
    if (side) side->connected = connectedOutputForDesktopStage(target);
    // A wheel turned at the edge names its width too, until the hand moves to
    // another edge. Only a monitor's top and side edges take it.
    const CarryWidthEdge widthEdge = !connectedOutputForDesktopStage(target) ? CarryWidthEdge::None
        : m_carryWidthHeld ? CarryWidthEdge::Top
        : !edge ? CarryWidthEdge::None
        : *edge == CarryEdge::Top ? CarryWidthEdge::Top
        : side ? CarryWidthEdge::Side : CarryWidthEdge::None;
    if (widthEdge != m_carryWidthEdge) {
        m_carryWidthEdge = widthEdge;
        m_carryWidth = 0.0;
        m_carryWidthClock.invalidate();
    }
    if (side && widthEdge == CarryWidthEdge::Side && m_carryWidth > 0.0) side->share = m_carryWidth;
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
        previewDrop({CarryDestinationKind::NativeDesktop, target->name(), target->name(), 0, 0},
            [carried, output] {
                return carried && !carried->isDeleted() && output
                    && KWin::effects->screens().contains(output.data()) && carried->screen() == output;
            },
            [this, output, box, carried](const PreparedCarrySource &source) {
                CardStageController *cards = carrySourceStage(source);
                const bool released = cards && cards->releaseNativeCarryToDesktop(source, output, box);
                if (released) m_letGoOnDisplay = carried;
                return released;
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
        previewDrop({CarryDestinationKind::NativeDesktop, target->name(), target->name(), 0, 0},
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
        QString partnerReason;
        auto *partner = sideEdge
            ? m_cardStage->partnerForSideSnap(m_carriedWindow, leftEdge, &partnerReason) : nullptr;
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
            .placesAlone = tablet && connectedForCardStage(),
        });
        QPointer<KWin::EffectWindow> carried = m_carriedWindow;
        QPointer<KWin::LogicalOutput> output = target;
        const auto destination = monitorDropIntent(target->name(), 0, std::nullopt, edge);
        refusal = edgeEntryRefusal(outcome, isCardWindow(m_carriedWindow));
        {
            // Once per edge and outcome in a carry: what this edge would do
            // with the window, and for a side, which partner and why.
            const char *meaning = outcome == EdgeEntryOutcome::Refuse ? "nothing (refused)"
                : outcome == EdgeEntryOutcome::Unchanged ? "nothing: no partner"
                : outcome == EdgeEntryOutcome::ComposeDisplayBento ? "a layout across the display"
                : outcome == EdgeEntryOutcome::PairIntoBento ? "a Bento pair"
                : outcome == EdgeEntryOutcome::PlaceAlone
                    ? (side && side->large ? "the card alone, beside"
                       : side && side->over ? "the card alone, over" : "the card alone, under")
                : outcome == EdgeEntryOutcome::AdoptDisplay ? "cards, starting with this window"
                : "the Active card";
            const QString line = QStringLiteral("%1 at the %2 edge would make %3%4")
                .arg(applicationIdentity(m_carriedWindow),
                     *edge == CarryEdge::Top ? QStringLiteral("top")
                         : *edge == CarryEdge::Bottom ? QStringLiteral("bottom")
                         : leftEdge ? QStringLiteral("left") : QStringLiteral("right"),
                     QString::fromLatin1(meaning),
                     sideEdge ? QStringLiteral("; partner: ") + partnerReason : QString());
            if (line != m_lastEdgeEntryLog) {
                m_lastEdgeEntryLog = line;
                qInfo() << "Kadunce" << Revision << qPrintable(line);
            }
        }
        switch (outcome) {
        case EdgeEntryOutcome::Refuse:
            return;
        case EdgeEntryOutcome::Unchanged: {
            // §11, Solo: no awake card to pair, but sleeping ones: the card
            // stays Active and the half beside its side offers them.
            if (!sideEdge || !side || !destination || !tablet || liveLayout
                || m_cardStage->activeCardIdentity() != m_carriedWindow
                || sleepingCardsOn(target).isEmpty()) return;
            refusal.reset();
            m_carryCardEntryOutput = target;
            const BentoSidePlacement place = *side;
            // The place it names: what the room leaves, across the gutter.
            const QRectF beside = pairOfferRoom(target, place);
            const QRectF whole(m_cardStage->activeTarget(target));
            const double gap = 10.0;
            const QRectF named = place.right
                ? QRectF(QPointF(beside.right() + gap, whole.top()), whole.bottomRight())
                : QRectF(whole.topLeft(), QPointF(beside.left() - gap, whole.bottom()));
            m_carryPreview = KWin::RectF(named);
            previewDrop(*destination,
                [this, carried, output] {
                    return carried && !carried->isDeleted() && output && carried->screen() == output
                        && m_cardStage->activeCardIdentity() == carried;
                },
                [this, carried, output, bento, place, named](const PreparedCarrySource &source) {
                    if (!(bento ? m_desktopStage->nativeCarrySourceValid(source) : cardCarrySourceValid(source)))
                        return false;
                    const QRectF drawn = carriedPose();
                    // It glides into the place its snap named first, and the
                    // offer opens in the rest.
                    m_cardStage->holdActivePlace(carried, KWin::RectF(named).toRect());
                    if (!m_cardStage->refitActive(carried)) {
                        m_cardStage->holdActivePlace(carried, std::nullopt);
                        return false;
                    }
                    glideIntoActive(carried, output, drawn);
                    askPairOffer(output, carried, place);
                    return true;
                });
            return;
        }
        case EdgeEntryOutcome::ComposeDisplayBento:
            break; // A display that cannot own cards reserves below as before.
        case EdgeEntryOutcome::PairIntoBento: {
            refusal = GestureRefusal::Unplaced;
            if (!side || !destination) return;
            refusal = GestureRefusal::NoRoom;
            const auto reserved = m_desktopStage->prepareCardDrop(m_carriedWindow, target,
                KWin::RectF(QRectF(handoff.carry().position(), m_carryPickup.size())),
                DesktopStageController::CardDropIntent::ActivateBento, side, partner);
            if (!reserved) return;
            m_carryPreview = m_desktopStage->cardDropPreview(*reserved);
            if (!m_carryPreview) return;
            m_carryDestination = reserved;
            previewDrop(*destination,
                [this, reserved] { return m_desktopStage->cardDropValid(*reserved); },
                [this, reserved, carried, output, bento](const PreparedCarrySource &source) {
                    // Commit against the identity the reservation named, never
                    // a partner re-read after preparation.
                    QPointer<KWin::EffectWindow> named = reserved->namedPartner();
                    // Read before commit, which can retire the carry's pose.
                    const QRectF drawn = carriedPose();
                    const bool committed = (bento ? m_desktopStage->nativeCarrySourceValid(source)
                                  : cardCarrySourceValid(source))
                        && m_desktopStage->activatePreparedTabletDrop(*reserved,
                            &source.restoreSnapshot(),
                            [this, carried, named] {
                                return m_cardStage->releasePairToBento(carried, named);
                            });
                    // It grows into its pane from where it was let go.
                    if (committed) startPaneArrival(carried, output, drawn);
                    return committed;
                });
            return;
        }
        case EdgeEntryOutcome::PlaceAlone: {
            // §11, Connected: the Active card takes the place its contact
            // names, alone. Nothing pairs, and the rest of the display stays
            // open for the next card.
            refusal = GestureRefusal::Unplaced;
            if (!side || !destination) return;
            refusal = GestureRefusal::NoRoom;
            const auto reserved = m_desktopStage->prepareCardDrop(m_carriedWindow, target,
                KWin::RectF(QRectF(handoff.carry().position(), m_carryPickup.size())),
                DesktopStageController::CardDropIntent::ActivateBento, side, nullptr, true);
            if (!reserved) return;
            m_carryPreview = m_desktopStage->cardDropPreview(*reserved);
            if (!m_carryPreview) return;
            m_carryDestination = reserved;
            // §11, Connected: the layout the card left waits in Spread, and a
            // side snap is a purposeful act, so the layout comes back with the
            // card in the place its contact names and the others beside it.
            if (m_cardStage->hasBentoGroup() && m_cardStage->activeCardIdentity() == m_carriedWindow) {
                const BentoSidePlacement place = *side;
                previewDrop(*destination,
                    [this, reserved] { return m_desktopStage->cardDropValid(*reserved); },
                    [this, carried, output, bento, place](const PreparedCarrySource &source) {
                        if (!carried || !output || !(bento ? m_desktopStage->nativeCarrySourceValid(source)
                                                           : cardCarrySourceValid(source)))
                            return false;
                        const QRectF drawn = carriedPose();
                        // After the carry has let the card go, as Meta+B would.
                        QTimer::singleShot(0, this, [this, carried, output, place, drawn] {
                            if (!carried || !output) return;
                            layOutHeldLayoutBeside(carried, output, place, drawn);
                            askRoomOffer(output);
                        });
                        return true;
                    });
                return;
            }
            previewDrop(*destination,
                [this, reserved] { return m_desktopStage->cardDropValid(*reserved); },
                [this, reserved, carried, output, bento](const PreparedCarrySource &source) {
                    // Read before commit, which can retire the carry's pose.
                    const QRectF drawn = carriedPose();
                    const bool committed = (bento ? m_desktopStage->nativeCarrySourceValid(source)
                                  : cardCarrySourceValid(source))
                        && m_desktopStage->activatePreparedTabletDrop(*reserved,
                            &source.restoreSnapshot(),
                            [this, carried] {
                                return m_cardStage->releasePairToBento(carried, nullptr);
                            });
                    if (committed) {
                        startPaneArrival(carried, output, drawn);
                        // The room left open shows the desktop, not the cards.
                        m_cardStage->syncSelectedElevation();
                        // Asked here: leaving Active can end the carry's
                        // presentation before the release is reported.
                        askRoomOffer(output);
                    }
                    return committed;
                });
            return;
        }
        case EdgeEntryOutcome::AdoptDisplay:
        case EdgeEntryOutcome::MakeActive: {
            // §10: a card that is already owned and not Active cannot be carried
            // natively, so only adoption and a native arrival reach here.
            refusal = GestureRefusal::Unplaced;
            if (!destination) return;
            const bool adopt = outcome == EdgeEntryOutcome::AdoptDisplay;
            if (!adopt && m_cardStage->liveCardIndex(carried) >= 0) {
                // §11, Connected: the Active card itself, with the wheel
                // turned, stays the Active card at its new width.
                const double width = m_carryWidthEdge == CarryWidthEdge::Top ? m_carryWidth : 0.0;
                if (width <= 0.0 || m_cardStage->activeCardIdentity() != carried
                    || !m_carriedWindow->window()) return;
                refusal.reset();
                m_carryCardEntryOutput = target;
                const KWin::Rect whole = m_cardStage->activeTarget(target);
                m_carryPreview = KWin::RectF(width < 1.0
                    ? CardStageController::narrowedActive(whole, width, m_carriedWindow->window()->minSize().width())
                    : whole);
                previewDrop(*destination,
                    [this, carried, output] {
                        return carried && !carried->isDeleted() && output && carried->screen() == output
                            && m_cardStage->activeCardIdentity() == carried;
                    },
                    [this, carried, output, bento, width](const PreparedCarrySource &source) {
                        if (!(bento ? m_desktopStage->nativeCarrySourceValid(source) : cardCarrySourceValid(source)))
                            return false;
                        // Read before the card moves, which retires the carry's pose.
                        const QRectF drawn = carriedPose();
                        m_cardStage->setActiveWidth(carried, width);
                        if (!m_cardStage->refitActive(carried)) return false;
                        glideIntoActive(carried, output, drawn);
                        return true;
                    });
                return;
            }
            // The destination is Card Stage: the carried window lands as the
            // Active card, and adoption makes every other eligible window a
            // nonselected individual card.
            m_carryCardEntryOutput = target;
            // §11, Connected: narrowed by the wheel, kept centred.
            const double width = m_carryWidthEdge == CarryWidthEdge::Top ? m_carryWidth : 0.0;
            const KWin::Rect whole = m_cardStage->activeTarget(target);
            m_carryPreview = KWin::RectF(width > 0.0 && width < 1.0 && m_carriedWindow->window()
                ? CardStageController::narrowedActive(whole, width,
                      m_carriedWindow->window()->minSize().width())
                : whole);
            previewDrop(*destination,
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
                [this, carried, output, adopt, bento, extractingPane, width](const PreparedCarrySource &source) {
                    const auto sourceValid = [this, &source, bento] {
                        return bento ? m_desktopStage->nativeCarrySourceValid(source)
                                     : cardCarrySourceValid(source);
                    };
                    // Read before commit, which can retire the carry's pose.
                    const QRectF drawn = carriedPose();
                    // Let go with no wheel turned, it stands at the whole width.
                    m_cardStage->setActiveWidth(carried, width > 0.0 ? width : 1.0);
                    if (adopt) {
                        if (!m_cardStage->adoptDisplayWithActive(carried, sourceValid)) return false;
                        glideIntoActive(carried, output, drawn);
                        return true;
                    }
                    // §5: a pane gives up Bento ownership in the same published
                    // step that makes it a card, so the layout it left is never
                    // observed still naming it. Everything else arrives without
                    // a layout to leave.
                    if (extractingPane
                        ? !m_desktopStage->extractPaneToCards(carried, sourceValid)
                        : !admitTransferredWindowToTablet(carried, output, sourceValid)) return false;
                    (void)m_cardStage->promoteToActive(carried);
                    glideIntoActive(carried, output, drawn);
                    // §10: taken out of a layout to stand alone, it is called
                    // back alone too, until Meta+B or an edge lays it out.
                    if (extractingPane && output && isTabletOutput(output)) markAloneOnPurpose(carried);
                    if (extractingPane && output && isTabletOutput(output) && connectedForCardStage()
                        && m_desktopStage->hasSessionOnOutput(output->name())) {
                        // After the glide, so the card is drawn where it lands.
                        QTimer::singleShot(motion(260), this, [this, carried, output] {
                            holdLayoutLeftBehind(carried, output);
                        });
                    }
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
    // §11, Connected: a pane carried over another pane and rested there joins
    // that pane's Stack in front; let go sooner, the two swap below.
    KWin::EffectWindow *over = local && bento && !desktopWindow && !edge
        && connectedOutputForDesktopStage(target)
        ? m_desktopStage->paneAt(target, contact, m_carriedWindow) : nullptr;
    // Carried in from another display and rested on Spread's Active card, or
    // on a pane there however that display is sized, as the card another
    // display shows alone is one, it lands as it would and then joins that
    // Stack.
    if (!over && !local && !desktopWindow && !edge) {
        if (tablet && m_cardStage->isActive()
            && m_cardStage->presentation() == CardPresentation::Active) {
            KWin::EffectWindow *card = m_cardStage->selectedWindow();
            if (card && card != m_carriedWindow && !card->isDeleted()
                && QRectF(card->frameGeometry()).contains(contact)) over = card;
        }
        if (!over && m_desktopStage->hasSessionOnOutput(target->name()))
            over = m_desktopStage->paneAt(target, contact, m_carriedWindow);
    }
    QPointer<KWin::EffectWindow> stackAfter;
    if (over) {
        aimingPane = true;
        if (!m_paneStackClock.isValid()) m_paneStackClock.start();
        const long long now = m_paneStackClock.elapsed();
        if (!m_paneStackAim.aim(QPointer<KWin::EffectWindow>(over), now)) {
            m_paneStackDwell.start(int(std::max(1LL, m_paneStackAim.remaining(now))));
        } else if (!local) {
            m_paneStackDwell.stop();
            m_carryStackOnto = over;
            stackAfter = over;
        } else {
            m_paneStackDwell.stop();
            m_carryStackOnto = over;
            m_carryPreview = KWin::RectF(over->frameGeometry());
            QPointer<KWin::EffectWindow> carried = m_carriedWindow;
            QPointer<KWin::EffectWindow> onto = over;
            const auto destination = monitorDropIntent(target->name(), 0,
                std::optional<MonitorLayoutTarget>{{target->name(), 0, 0}}, std::nullopt);
            if (!destination) return;
            previewDrop(*destination,
                [this, carried, onto] {
                    return carried && onto && !carried->isDeleted() && !onto->isDeleted()
                        && m_desktopStage->managesWindow(carried) && m_desktopStage->managesWindow(onto);
                },
                [this, carried, onto](const PreparedCarrySource &) {
                    return carried && onto && stackPaneOnto(carried, onto);
                });
            return;
        }
    }
    // From here an edge asks the layout solve for room, which is the one
    // reason left for it to refuse.
    if (edge) refusal = GestureRefusal::NoRoom;
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
    if (stackAfter) m_carryPreview = KWin::RectF(stackAfter->frameGeometry());
    // §11, Connected: carried in from another display with the wheel turned,
    // it stands as this display's Active card at that width, kept centred.
    const double arrivalWidth = !local && !existing && !stackAfter && !stayNative && isCardDisplay(target)
        && m_carryWidthHeld && m_carryWidth > 0.0 && m_carriedWindow->window() ? m_carryWidth : 0.0;
    if (arrivalWidth > 0.0) {
        const KWin::Rect whole = m_cardStage->activeTarget(target);
        m_carryPreview = KWin::RectF(arrivalWidth < 1.0
            ? CardStageController::narrowedActive(whole, arrivalWidth, m_carriedWindow->window()->minSize().width())
            : whole);
    }
    previewDrop(*destination,
        [this, reserved, stackAfter, aimed = bool(stackAfter)] {
            return m_desktopStage->cardDropValid(*reserved) && (!aimed || (stackAfter && !stackAfter->isDeleted()));
        },
        [this, reserved, bento, output, geometry, stackAfter, targetRect = stackAfter ? std::optional<KWin::RectF>{} : m_carryPreview,
         arrival = m_carriedWindow, arrivalWidth](const PreparedCarrySource &source) {
            if (arrivalWidth > 0.0 && arrival) m_cardStage->setActiveWidth(arrival, arrivalWidth);
            const bool committed = bento
                ? m_desktopStage->transferNativeCarryToDesktop(source, *reserved)
                : carrySourceStage(source) && carrySourceStage(source)->transferNativeCarryToDesktop(source, output, geometry);
            if (committed && targetRect) startDropSettle(arrival, output, QRectF(geometry), QRectF(*targetRect));
            if (committed && arrivalWidth > 0.0 && output) fitArrivalWidth(arrival, output->name(), arrivalWidth, 10);
            if (committed && stackAfter) {
                QPointer<KWin::EffectWindow> carried = arrival;
                QPointer<KWin::EffectWindow> onto = stackAfter;
                QTimer::singleShot(0, this, [this, carried, onto] { stackWhenLanded(carried, onto); });
            }
            return committed;
        });
}

void Effect::stackWhenLanded(QPointer<KWin::EffectWindow> carried, QPointer<KWin::EffectWindow> onto, int tries)
{
    if (!carried || !onto || carried->isDeleted() || onto->isDeleted() || m_releasing) return;
    // Asked of the stage of the card it rested on.
    SessionScope scope(this, m_scopedSession, stageKeyOf(onto));
    // CARD-LIFECYCLE.md §11: once it stands where it was carried, as a pane
    // of that layout or as the display's card, it joins the Stack it rested on.
    bool stacked = false;
    if (m_desktopStage->managesWindow(carried) && m_desktopStage->managesWindow(onto)) {
        stacked = stackPaneOnto(carried, onto);
    } else if (KWin::LogicalOutput *there = carried->screen();
               m_desktopStage->managesWindow(carried) && m_cardStage->liveCardIndex(onto) >= 0
               && isTabletOutput(there)) {
        // §11: the card the display showed gave its place to the arrival, a
        // pane now; it waits behind it, as a card given there with its Stack does.
        QList<KWin::EffectWindow *> behind = m_cardStage->stackFrontToBack(onto);
        if (behind.isEmpty()) behind.append(onto);
        behind.append(m_cardStage->paneStack(carried));
        m_cardStage->notePaneStack(carried, behind);
        m_cardStage->settlePaneStacks();
        stacked = true;
        qInfo() << "Kadunce" << Revision << "stacked" << logNameOf(carried) << "on" << logNameOf(onto)
                << "on" << there->name();
        observeCardOwnership();
        Q_EMIT workspaceContextChanged();
    } else if (m_cardStage->liveCardIndex(carried) >= 0 && m_cardStage->liveCardIndex(onto) >= 0) {
        stacked = m_cardStage->stackCardOnto(carried, onto);
        if (stacked) {
            qInfo() << "Kadunce" << Revision << "stacked" << logNameOf(carried) << "on" << logNameOf(onto);
            observeCardOwnership();
            Q_EMIT workspaceContextChanged();
        }
    }
    if (stacked) return;
    if (tries <= 0) {
        qInfo() << "Kadunce" << Revision << "could not stack" << logNameOf(carried) << "on"
                << logNameOf(onto) << "after it landed";
        return;
    }
    QTimer::singleShot(100, this, [this, carried, onto, tries] { stackWhenLanded(carried, onto, tries - 1); });
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
    m_settleAsked.invalidate();
    m_settleOnCards = false;
    m_settleDrawnMoving = false;
    if (repaint) KWin::effects->addRepaintFull();
}

QRectF Effect::carriedPose() const
{
    if (!m_carriedWindow || !m_carryRuntime) return {};
    // Where it is drawn, leaning into a Stack included, so a drop grows from there.
    return leanedIntoStack(QRectF(m_carryRuntime->handoff.carry().position(), m_carryPickup.size()),
                           m_carryStackOnto ? stackLeanNow() : 0.0);
}

bool Effect::cardDisplayPresents(KWin::EffectWindow *window) const
{
    // What the card display shows: its layout's panes while it presents the
    // layout, the Active card alone among the cards, and anything it does not
    // own as it stands.
    if (!window || !m_cardStage->isActive()) return true;
    if (m_desktopStage->managesWindow(window)) return m_cardStage->presentsBento();
    if (m_cardStage->liveCardIndex(window) >= 0)
        return m_cardStage->presentation() == CardPresentation::Active
            && m_cardStage->selectedWindow() == window;
    return true;
}

void Effect::glideIntoActive(KWin::EffectWindow *window, KWin::LogicalOutput *output, const QRectF &from)
{
    if (!window || window->isDeleted() || !window->window() || !output || !isTabletOutput(output)
        || !m_cardStage->isActive() || m_cardStage->presentation() != CardPresentation::Active
        || m_cardStage->selectedWindow() != window) return;
    startDropSettle(window, output, from, QRectF(window->window()->moveResizeGeometry()), true);
}

void Effect::lastPaneLeftLayoutForDesktopStage(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                                               const QRectF &drawn)
{
    if (!scopedToCurrent()) return;
    glideIntoActive(window, output, drawn);
}

void Effect::paneYieldedForDesktopStage(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                                        const QRectF &drawn)
{
    // A pane leaving another display is a card on Spread's at once; its fade
    // would be drawn where it no longer stands.
    if (isOtherCardDisplay(output)) return;
    if (!scopedToCurrent() || !window || window->isDeleted() || !output || !drawn.isValid()
        || motion(PaneArrivalDuration) <= 1) return;
    m_yieldingPanes.removeIf([window](const auto &yielding) { return yielding.window == window; });
    YieldingPane yielding{window, output, drawn, QRectF(output->geometry()), {}};
    yielding.timer.start();
    m_yieldingPanes.append(yielding);
    KWin::effects->addRepaintFull();
}

std::optional<double> Effect::yieldingPaneOpacity(KWin::EffectWindow *window) const
{
    for (const auto &yielding : m_yieldingPanes) {
        if (yielding.window != window) continue;
        const int duration = motion(PaneArrivalDuration);
        // It fades only while it is a hidden card: shown again, as a pane or
        // as a card, it is drawn as that.
        if (!window || window->isDeleted() || window->isMinimized() || !yielding.output
            || QRectF(yielding.output->geometry()) != yielding.outputGeometry
            || yielding.timer.elapsed() >= duration
            || m_desktopStage->managesWindow(window)
            || (m_cardStage->isActive() && m_cardStage->presentation() != CardPresentation::Bento
                && isTabletOutput(yielding.output))
            || (m_cardStage->presentation() == CardPresentation::Active
                && m_cardStage->selectedWindow() == window)) return std::nullopt;
        return 1.0 - QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
            double(yielding.timer.elapsed()) / duration);
    }
    return std::nullopt;
}

void Effect::clearYieldingPanes()
{
    if (m_yieldingPanes.isEmpty()) return;
    for (const auto &yielding : std::as_const(m_yieldingPanes))
        if (yielding.window && !yielding.window->isDeleted()) unredirect(yielding.window);
    m_yieldingPanes.clear();
    KWin::effects->addRepaintFull();
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
            || !landsAt(QRectF(window->window()->moveResizeGeometry()), m.to)
            // A pane is drawn moving only while its layout is what the display
            // presents; one that left it, or a layout gone into Spread, is
            // drawn as what it is now.
            || !m_desktopStage->managesWindow(window)
            || (isTabletOutput(m.output) && m_cardStage->isActive()
                && !m_cardStage->presentsBento())) return std::nullopt;
        const double t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
            paneArrivalProgress(started, started ? m.timer.elapsed() : 0, duration));
        return QRectF(m.from.topLeft()+(m.to.topLeft()-m.from.topLeft())*t,
            m.from.size()+(m.to.size()-m.from.size())*t);
    }
    return std::nullopt;
}

void Effect::appearInPlace(KWin::EffectWindow *window)
{
    // A window that has just opened, or a card a Stack steps to, appears in
    // its pane rather than flying in from where it waited.
    m_justOpened.removeIf([window](const auto &opened) {
        return !opened.first || opened.first->isDeleted() || opened.first == window
            || opened.second.elapsed() > OpenedStill;
    });
    if (!window) return;
    QElapsedTimer since;
    since.start();
    m_justOpened.append({QPointer<KWin::EffectWindow>(window), since});
}

void Effect::stepInUnseen(KWin::EffectWindow *window)
{
    // A card a Stack steps to is drawn only once it stands where it is asked
    // to, so it never shows for a frame where it waited.
    if (!window) return;
    m_steppingIn.removeIf([window](const auto &entry) { return !entry.first || entry.first == window; });
    QElapsedTimer since;
    since.start();
    m_steppingIn.append({QPointer<KWin::EffectWindow>(window), since});
    QTimer::singleShot(StepInUnseen + 16, this, [] { KWin::effects->addRepaintFull(); });
}

bool Effect::steppingInUnseen(const KWin::EffectWindow *window) const
{
    for (auto it = m_steppingIn.begin(); it != m_steppingIn.end(); ++it) {
        if (it->first != window) continue;
        if (!window->window() || it->second.elapsed() > StepInUnseen) {
            m_steppingIn.erase(it);
            return false;
        }
        const QRectF drawn(window->frameGeometry());
        const QRectF asked(window->window()->moveResizeGeometry());
        const bool there = std::abs(drawn.x() - asked.x()) < 2.0 && std::abs(drawn.y() - asked.y()) < 2.0
            && std::abs(drawn.width() - asked.width()) < 2.0 && std::abs(drawn.height() - asked.height()) < 2.0;
        if (there) {
            m_steppingIn.erase(it);
            return false;
        }
        return true;
    }
    return false;
}

bool Effect::justOpened(const KWin::EffectWindow *window) const
{
    return std::any_of(m_justOpened.cbegin(), m_justOpened.cend(), [window](const auto &opened) {
        return opened.first == window && opened.second.elapsed() <= OpenedStill;
    });
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
    // §11, Connected: a placed layout can open or fill room, which decides
    // whether the cards behind it are held aside. Asked after the turn, as the
    // layout is still being applied.
    if (output && isTabletOutput(output))
        QTimer::singleShot(0, this, [this] {
            if (!m_releasing && m_cardStage) m_cardStage->syncSelectedElevation();
        });
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
            || QRectF(w->window()->moveResizeGeometry()) != to[i] || justOpened(w)) continue;
        m_bentoMotions.append({w,output,from[i],to[i],QRectF(output->geometry()),clock});
    }
    if (!m_bentoMotions.isEmpty()) KWin::effects->addRepaintFull();
}

void Effect::startDropSettle(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                            const QRectF &from, const QRectF &to, bool onCards)
{
    clearDropSettle();
    // Logical commitment is not proof of native placement. Require KWin's
    // accepted target; the client's buffer may follow it late. Painting maps
    // whichever buffer is current. A move may wait a short, bounded time for
    // that buffer, as a card arriving in a pane does (PaneArrival.h), but it
    // never holds input or retries geometry.
    if (!window || window->isDeleted() || !output || (isTabletOutput(output) && !onCards)
        || !from.isValid() || !to.isValid() || from == to
        || !window->window() || window->window()->moveResizeOutput() != output
        || QRectF(window->window()->moveResizeGeometry()) != to
        || window->isUserMove() || window->isUserResize() || window->isMinimized()) return;
    m_settlingWindow = window; m_settlingOutput = output;
    m_settleFrom = from; m_settleTo = to;
    m_settleOutputGeometry = QRectF(output->geometry());
    m_settleOnCards = onCards;
    m_settleAsked.start();
    // On the card display it waits where it was let go for its client to
    // draw the shape it settles into, as a card arriving in a pane does.
    if (!onCards) m_dropSettleTimer.start();
    KWin::effects->addRepaintFull();
}

std::optional<QRectF> Effect::dropSettleRect() const
{
    const double Duration = motion(220);
    const bool started = m_dropSettleTimer.isValid();
    if (!m_settlingWindow || m_settlingWindow->isDeleted() || !m_settlingOutput
        || (started ? m_dropSettleTimer.elapsed() >= Duration
                    : !m_settleAsked.isValid() || m_settleAsked.elapsed() >= PaneArrivalAbandon)
        || (m_settleOnCards && !cardDisplayPresents(m_settlingWindow))
        || !m_settlingWindow->window()
        || m_settlingWindow->window()->moveResizeOutput() != m_settlingOutput
        || QRectF(m_settlingOutput->geometry()) != m_settleOutputGeometry
        || !landsAt(QRectF(m_settlingWindow->window()->moveResizeGeometry()), m_settleTo)
        || m_settlingWindow->isUserMove() || m_settlingWindow->isUserResize()
        || m_settlingWindow->isMinimized()) return std::nullopt;
    const auto t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
        paneArrivalProgress(started, started ? m_dropSettleTimer.elapsed() : 0, int(Duration)));
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
            SessionScope own(this, m_scopedSession, stageKeyOf(window));
            m_cardStage->handleActiveGeometryChanged(window);
            return;
        }
    }
}

void Effect::handleWindowMoveResizeStepped(
    KWin::EffectWindow *window, const KWin::RectF &geometry)
{
    // §11, Every display: the Active card may be on its way to another
    // display holding its own cards, so where it lands decides.
    SessionScope scope(this, m_scopedSession, stageKeyOf(window));
    if (window && window == m_heldActiveMove && !carriesToAnotherCardDisplay()
        && QLineF(m_heldActiveMoveOrigin, QRectF(geometry).topLeft()).length() > ActiveMoveTravel) {
        m_heldActiveMove.clear();
        qInfo() << "Kadunce" << Revision << applicationIdentity(window) << "travelled, so the cards let go";
        m_cardStage->handleManualWindowChange(window);
    }
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
    bool givenToDisplay = false;
    if (window && window == m_heldActiveMove) {
        m_heldActiveMove.clear();
        SessionScope scope(this, m_scopedSession, stageKeyOf(window));
        KWin::LogicalOutput *landed = window->screen();
        if (isOtherCardDisplay(landed) && m_cardStage->liveCardIndex(window) >= 0) {
            // §11, Every display: carried to another display holding its own
            // cards, the Active card is the card that display shows, its
            // Stack with it.
            givenToDisplay = true;
            const QPointer<KWin::EffectWindow> card = window;
            const QString there = landed->name();
            QTimer::singleShot(0, this, [this, card, there] {
                if (!card || card->isDeleted()) return;
                KWin::LogicalOutput *display = screenNamed(there);
                if (isCardDisplay(display) && moveCardToDisplay(card, display, true)) return;
                SessionScope own(this, m_scopedSession, stageKeyOf(card));
                m_cardStage->handleManualWindowChange(card);
            });
        } else if (QLineF(m_heldActiveMoveOrigin, QRectF(window->frameGeometry()).topLeft()).length() > ActiveMoveTravel) {
            qInfo() << "Kadunce" << Revision << applicationIdentity(window) << "travelled, so the cards let go";
            m_cardStage->handleManualWindowChange(window);
        } else {
            qInfo() << "Kadunce" << Revision << "keeps the cards: the move of" << applicationIdentity(window)
                    << "went nowhere";
            m_cardStage->followWorkArea(true);
        }
    }
    m_desktopStage->handleWindowMoveResizeFinished(window);
    // Use KWin's completed output assignment, not the cursor: Escape restores
    // the source output even if the pointer remains over the destination.
    if (carried && !fromBento && !givenToDisplay && window->screen()
        && window->screen()->name() != source) {
        if (isCardDisplay(window->screen())) {
            admitTransferredWindowToTablet(window, window->screen());
        } else {
            (void)m_desktopStage->admitCardWindow(window, window->screen(), window->frameGeometry());
        }
    }
    if (carried) {
        syncSelectedElevation();
        KWin::effects->addRepaintFull();
        // §11, Every display: a window carried loose onto another display
        // holding its own cards becomes a card there.
        const QPointer<KWin::EffectWindow> moved = window;
        if (!givenToDisplay)
            QTimer::singleShot(0, this, [this, moved, source] { claimWindowForItsDisplay(moved, source); });
    }
}

std::optional<CarryEdge> Effect::carryEdgeAt(const KWin::LogicalOutput *output, QPointF contact) const
{
    if (!output) return std::nullopt;
    const auto edge = monitorCarryEdge(QRectF(output->geometry()), contact);
    if (edge && *edge != CarryEdge::Top && isPanelPoint(contact)) return std::nullopt;
    return edge;
}

bool Effect::carriesToAnotherCardDisplay() const
{
    if (!m_cardsOnEveryDisplay || !tabletOutput()) return false;
    const auto screens = KWin::effects->screens();
    return std::any_of(screens.cbegin(), screens.cend(),
        [this](KWin::LogicalOutput *output) { return isOtherCardDisplay(output); });
}

void Effect::claimWindowForItsDisplay(KWin::EffectWindow *window, const QString &from)
{
    // §11, Every display: a window that left another display loose on one
    // holding its own cards is a card there, as one opening there is. A
    // window let go on its own display stays as it was let go.
    if (!window || window->isDeleted() || m_releasing || m_letGo || !window->window()
        || !isCardWindow(window) || isDependentWindow(window) || window->isMinimized()
        || window->isUserMove() || window->isUserResize() || window == m_carriedWindow
        || window == m_nativeCarry || !window->window()->readyForPainting()) return;
    KWin::LogicalOutput *output = window->screen();
    if (!isCardDisplay(output) || output->name() == from || sessionHolding(window)
        || m_desktopStage->managesWindow(window)) return;
    if (window == m_letGoOnDisplay) {
        qInfo() << "Kadunce" << Revision << "leaves" << logNameOf(window) << "on" << output->name()
                << "where it was let go at the bottom edge";
        return;
    }
    qInfo() << "Kadunce" << Revision << "takes" << logNameOf(window) << "left loose on"
            << output->name() << "as a card there";
    SessionScope scope(this, m_scopedSession, stageKeyFor(output));
    const bool taken = m_desktopStage->hasSessionOnOutput(output->name())
        ? m_desktopStage->handleWindowAdded(window)
        : m_cardStage->isActive() ? admitTransferredWindowToTablet(window, output)
        : startTabletInCards(window);
    if (taken) {
        KWin::effects->activateWindow(window);
        observeCardOwnership();
        Q_EMIT workspaceContextChanged();
    }
}

bool Effect::admitTransferredWindowToTablet(KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore)
{
    // The display named takes it, else the display holding cards it stands on.
    if (!output && window && isCardDisplay(window->screen())) output = window->screen();
    std::optional<SessionScope> scope;
    if (output) scope.emplace(this, m_scopedSession, stageKeyFor(output));
    // Copy before guest/source cleanup can retire NativeCarryRuntime's pose.
    const QRectF carriedOrigin = window == m_carriedWindow && m_carryRuntime
        ? QRectF(m_carryRuntime->handoff.carry().position(), m_carryPickup.size())
        : QRectF();
    if (m_cardStage->launcherGuestActive()) closeLauncherGuest();
    // A caller that still held the window's session record supplies it. Asking
    // the desktop stage afterwards would read the pane rectangle instead, since
    // by then the session has published a plan that no longer names the window.
    const auto source = restore ? std::nullopt : m_desktopStage->prepareNativeCarrySource(window);
    const auto derived = source ? std::optional<NativeMoveSnapshot>(source->restoreSnapshot())
        : std::nullopt;
    if (!restore && derived) restore = &*derived;
    // §11: a card another display's stage holds leaves it in the same step,
    // and its Stack follows it here.
    CardStageController *keeper = stageKeepingStackOf(window);
    if (keeper == m_cardStage) keeper = nullptr;
    const QList<KWin::EffectWindow *> behind = keeper ? cardsBehind(*keeper, window)
                                                      : QList<KWin::EffectWindow *>{};
    const QPointer<KWin::EffectWindow> moving = window;
    const std::function<bool()> commit = keeper
        ? std::function<bool()>([commitSource, keeper, moving] {
              return commitSource() && moving
                  && (keeper->liveCardIndex(moving) < 0 || keeper->releaseCardToStage(moving));
          })
        : commitSource;
    const bool admitted = m_cardStage->admitTransferredWindowToTablet(window, commit,
        carriedOrigin, restore);
    if (admitted && keeper) {
        keeper->forgetPaneStack(window);
        rejoinStack(window, behind);
    }
    return admitted;
}

void Effect::handleScreenRemoved(KWin::LogicalOutput *output)
{
    m_displaysChanged.start();
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
    // §11: a stage whose display went away, or that the way displays hold
    // cards no longer keeps, gives each card to the display it stands on now.
    handOverGoneStages();
    // Each display holding cards settles its own, and is held again once
    // every card is placed.
    QTimer::singleShot(0, this, [this] { (void)holdDisplaysInCards(nullptr); });
    bool again = false;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (!isCardDisplay(output)) continue;
        SessionScope scope(this, m_currentSession, stageKeyFor(output));
        if (settleCardsOnThisDisplay()) again = true;
    }
    // A client can still acknowledge the placement KWin asked of it before the
    // return, and KWin applies an acknowledged placement where it was asked:
    // a card that kept its size goes straight back, then that answer takes it
    // to the other display again. Look once more after the round trip, until
    // nothing needs to move, a bounded number of times.
    constexpr int SettleRounds = 8;
    if (again && ++m_cardDisplaySettleRounds < SettleRounds) {
        QTimer::singleShot(250, this, [this] {
            if (!m_cardDisplaySettleQueued) settleCardsOnDisplays();
        });
    }
}

void Effect::handOverGoneStages()
{
    // Each card goes, its Stack with it, to the stage of the display KWin
    // moved its window to, else the primary display's, and waits at the end
    // of that Spread's row. The receiving display's Active card and layout stay.
    if (m_scopeDepth == 0) restInUse();
    KWin::LogicalOutput *primary = primaryOutput();
    if (!primary || !isCardDisplay(primary)) return;
    for (const auto &entry : m_sessions) {
        DesktopSession *session = entry.second.get();
        QStringList gone;
        for (const auto &display : session->displays) {
            const QString &key = display.first;
            if (key.isEmpty() ? m_cardsOnEveryDisplay : !isCardDisplay(screenNamed(key)) || !m_cardsOnEveryDisplay)
                gone.append(key);
        }
        for (const QString &key : std::as_const(gone)) {
            const auto found = session->displays.find(key);
            if (found == session->displays.end()) continue;
            CardStageController *stage = found->second->cards.get();
            QList<QPointer<KWin::EffectWindow>> cards;
            for (const auto &card : stage->liveCards())
                if (card && !card->isDeleted()) cards.append(card);
            if (cards.isEmpty()) continue;
            qInfo() << "Kadunce" << Revision << "hands" << cards.size() << "cards held for"
                    << (key.isEmpty() ? QStringLiteral("the one card display") : key) << "to the displays they stand on";
            // The card the gone display showed is shown where it went, when
            // nothing is shown there.
            const QPointer<KWin::EffectWindow> wasShown = stage->presentation() == CardPresentation::Active
                ? stage->activeCardIdentity() : nullptr;
            for (const auto &card : std::as_const(cards)) {
                if (!card || card->isDeleted() || stage->liveCardIndex(card) < 0) continue;
                // A card behind another goes with its face.
                const auto deck = stage->stackFrontToBack(card);
                if (!deck.isEmpty() && deck.first() != card) continue;
                KWin::LogicalOutput *there = card->window() ? card->window()->moveResizeOutput() : nullptr;
                if (!isCardDisplay(there)) there = primary;
                if (!moveCardToDisplay(card, there, false))
                    qInfo() << "Kadunce" << Revision << "could not hand" << logNameOf(card) << "to" << there->name();
            }
            const auto heldAt = wasShown && !wasShown->isDeleted() ? session->keyHolding(wasShown) : std::nullopt;
            if (heldAt && *heldAt != key) {
                SessionScope there(this, session, *heldAt);
                if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Desktop)
                    (void)m_cardStage->presentCardActive(wasShown, false);
            }
        }
    }
    forgetGoneStages();
}

void Effect::forgetGoneStages()
{
    // A stage no display holds and no card is left in is let go, unless it is
    // the one named now.
    for (const auto &entry : m_sessions) {
        DesktopSession *session = entry.second.get();
        for (auto it = session->displays.begin(); it != session->displays.end();) {
            const QString &key = it->first;
            const bool kept = key.isEmpty() ? !m_cardsOnEveryDisplay
                                            : m_cardsOnEveryDisplay && isCardDisplay(screenNamed(key));
            if (kept || (session == m_scopedSession && key == m_scopedKey)
                || !it->second->cards->liveCards().isEmpty()) {
                ++it;
                continue;
            }
            it->second->cards->release();
            it = session->displays.erase(it);
        }
    }
}

bool Effect::settleCardsOnThisDisplay()
{
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet || !KWin::effects->screens().contains(tablet)) return false;
    // Spread itself moved: every card takes its place on the new display,
    // including those KWin already put there when the old one went away.
    const bool moved = std::exchange(m_cardDisplayMoved, false);
    if (moved) {
        // A layout Card Stage presented stayed on the display Spread left.
        if (m_cardStage->presentsBento() && !m_desktopStage->hasSessionOnOutput(tablet->name()))
            m_cardStage->leaveBentoPresentation();
        // The one card the new display showed becomes a card of Spread's.
        m_desktopStage->endLonePanes();
    }
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
    if (!m_cardStage->isActive() || (arrivals.isEmpty() && !strayed && !moved)) return false;
    cancelInputForCardStage();
    // A client can still acknowledge the placement KWin asked of it before the
    // return, and KWin applies an acknowledged placement where it was asked:
    // a card that kept its size goes straight back, then that answer takes it
    // to the other display again. Look once more after the round trip, until
    // nothing needs to move, a bounded number of times.
    const bool returned = m_cardStage->returnCardsToDisplay(moved) > 0;
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
    return returned;
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

bool Effect::claimTopEdge(const QString &key, const QString &outputName,
    double x, double y, double width, double height)
{
    if (!calledFromDBus() || key.isEmpty() || outputName.isEmpty()
        || !(width > 0.0) || !(height > 0.0)) {
        return false;
    }
    const QString owner = message().service();
    watchClaimOwner(owner);
    m_topEdgeClaims.insert(owner + QLatin1Char('\n') + key,
        TopEdgeClaim{owner, outputName, QRectF(x, y, width, height)});
    return true;
}

void Effect::watchClaimOwner(const QString &owner)
{
    if (!m_topEdgeClaimWatcher) {
        m_topEdgeClaimWatcher = new QDBusServiceWatcher(
            QString(), QDBusConnection::sessionBus(),
            QDBusServiceWatcher::WatchForUnregistration, this);
        connect(m_topEdgeClaimWatcher, &QDBusServiceWatcher::serviceUnregistered,
                this, [this](const QString &service) {
            m_topEdgeClaims.removeIf([&service](const auto &claim) {
                return claim.value().owner == service;
            });
            const bool hadLine = m_contextLineClaims.removeIf([&service](const auto &claim) {
                return claim.value().owner == service;
            }) > 0;
            m_topEdgeClaimWatcher->removeWatchedService(service);
            // Kadunce's own words come back on the displays it left.
            if (hadLine) KWin::effects->addRepaintFull();
        });
    }
    m_topEdgeClaimWatcher->addWatchedService(owner);
}

bool Effect::claimContextLine(const QString &key, const QString &outputName)
{
    if (!calledFromDBus() || key.isEmpty() || outputName.isEmpty()) return false;
    const QString owner = message().service();
    watchClaimOwner(owner);
    m_contextLineClaims.insert(owner + QLatin1Char('\n') + key, ContextLineClaim{owner, outputName});
    KWin::effects->addRepaintFull();
    return true;
}

void Effect::clearContextLineClaim(const QString &key)
{
    if (!calledFromDBus()) return;
    if (m_contextLineClaims.remove(message().service() + QLatin1Char('\n') + key) > 0)
        KWin::effects->addRepaintFull();
}

bool Effect::contextLineClaimed(const KWin::LogicalOutput *output) const
{
    if (!output || m_contextLineClaims.isEmpty()) return false;
    for (const ContextLineClaim &claim : m_contextLineClaims)
        if (claim.output == output->name()) return true;
    return false;
}

QJsonValue Effect::contextNotice() const
{
    const auto notice = [](const QString &kind, const QString &text, const KWin::LogicalOutput *output) {
        return QJsonObject{
            {QStringLiteral("kind"), kind},
            {QStringLiteral("text"), text},
            {QStringLiteral("output"), output ? output->name() : QString()},
        };
    };
    if (m_detachShownOn) return notice(QStringLiteral("returnToDesktop"), tr("Return to desktop"), m_detachShownOn);
    if (m_gestureNote && m_gestureNote->output && m_gestureNote->shown.elapsed() < GestureNoteTiming::TotalMs)
        return notice(QStringLiteral("refused"), m_gestureNote->text, m_gestureNote->output);
    if (m_zoneNotice && m_zoneNotice->shown.elapsed() < GestureNoteTiming::TotalMs)
        return notice(QStringLiteral("zones"), m_zoneNotice->text, nullptr);
    // The open room's offer says how to choose on its display's own line.
    if (m_roomOffer && m_roomOffer->output && contextLineClaimed(m_roomOffer->output))
        return notice(QStringLiteral("offer"), roomOfferHint(), m_roomOffer->output);
    return QJsonValue(QJsonValue::Null);
}

void Effect::eachCardDisplay(const std::function<void(KWin::LogicalOutput *)> &f) const
{
    // Each display holding cards, its own stage named while it is read.
    auto *self = const_cast<Effect *>(this);
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (!isCardDisplay(output) || !m_currentSession) continue;
        SessionScope scope(self, m_currentSession, stageKeyFor(output));
        f(output);
    }
}

QJsonObject Effect::cardStageContext() const
{
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
    const auto workspace = m_cardStage->workspaceSnapshot();
    QJsonArray selectedStack;
    for (const auto &member : workspace.selectedStack) selectedStack.append(member);
    KWin::EffectWindow *active = m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Active ? m_cardStage->activeCardIdentity() : nullptr;
    return QJsonObject{
        {QStringLiteral("active"), m_cardStage->isActive()},
        {QStringLiteral("presentation"), presentation},
        {QStringLiteral("launcherGuestActive"), m_cardStage->launcherGuestActive()},
        {QStringLiteral("selectedCardId"), workspace.selectedCardId.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(workspace.selectedCardId)},
        {QStringLiteral("selectedStack"), selectedStack},
        {QStringLiteral("spreadEntry"), spreadEntryContext()},
        {QStringLiteral("activeCardId"), active ? QJsonValue(windowIdentity(active)) : QJsonValue(QJsonValue::Null)},
    };
}

QJsonValue Effect::spreadEntryContext() const
{
    if (!m_cardStage->isActive() || m_cardStage->presentation() != CardPresentation::Spread)
        return QJsonValue(QJsonValue::Null);
    const SpreadModel &model = m_cardStage->model();
    const auto &cards = m_cardStage->liveCards();
    const int id = model.count() > 0 ? model.idAtOffset(0) : 0;
    KWin::EffectWindow *window = id >= 1 && id <= cards.size() ? cards.at(id - 1).data() : nullptr;
    if (!window || window->isDeleted()) return QJsonValue(QJsonValue::Null);
    const int size = std::max(1, model.stackSizeForId(id));
    return QJsonObject{
        {QStringLiteral("cardId"), window->internalId().toString(QUuid::WithoutBraces)},
        {QStringLiteral("name"), spreadCardName(window, id)},
        {QStringLiteral("bento"), !m_cardStage->layoutPartners(window).isEmpty()},
        {QStringLiteral("stackPosition"), size > 1 ? m_cardStage->shownStackPosition(id) + 1 : 1},
        {QStringLiteral("stackSize"), size},
        {QStringLiteral("sleeping"), window->isMinimized()},
    };
}

QString Effect::contextLineSignature() const
{
    // Every display's line, so a change on any one is announced once.
    QString signature;
    eachCardDisplay([&](KWin::LogicalOutput *output) {
        signature += output->name() + QLatin1Char(' ');
        if (!m_cardStage->isActive() || m_cardStage->presentation() != CardPresentation::Spread) {
            signature += QLatin1Char('\n');
            return;
        }
        const SpreadModel &model = m_cardStage->model();
        const auto &cards = m_cardStage->liveCards();
        const int id = model.count() > 0 ? model.idAtOffset(0) : 0;
        KWin::EffectWindow *window = id >= 1 && id <= cards.size() ? cards.at(id - 1).data() : nullptr;
        if (window && !window->isDeleted()) {
            signature += QStringLiteral("%1 %2 %3 %4 %5").arg(id)
                .arg(m_cardStage->shownStackPosition(id)).arg(model.stackSizeForId(id))
                .arg(window->isMinimized()).arg(spreadCardName(window, id));
        }
        signature += QLatin1Char('\n');
    });
    if (m_detachShownOn) signature += QStringLiteral("d ") + m_detachShownOn->name();
    else if (m_gestureNote && m_gestureNote->output && m_gestureNote->shown.elapsed() < GestureNoteTiming::TotalMs)
        signature += QStringLiteral("r ") + m_gestureNote->text;
    else if (m_zoneNotice && m_zoneNotice->shown.elapsed() < GestureNoteTiming::TotalMs)
        signature += QStringLiteral("z ") + m_zoneNotice->text;
    else if (m_roomOffer && m_roomOffer->output && contextLineClaimed(m_roomOffer->output))
        signature += QStringLiteral("o ") + m_roomOffer->output->name();
    return signature;
}

void Effect::clearTopEdgeClaim(const QString &key)
{
    if (!calledFromDBus()) return;
    m_topEdgeClaims.remove(message().service() + QLatin1Char('\n') + key);
}

bool Effect::topEdgeClaimedForInput(const QPointF &position) const
{
    if (m_topEdgeClaims.isEmpty()) return false;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (!output) continue;
        const QRectF display(output->geometry());
        if (!display.contains(position)) continue;
        for (const TopEdgeClaim &claim : m_topEdgeClaims) {
            if (claim.output == output->name()
                && claim.area.translated(display.topLeft()).contains(position)) {
                return true;
            }
        }
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

bool Effect::clientReceivesTouchAt(const QPointF &position) const
{
    // The window KWin would deliver the touch to. On its frame or the resize
    // border beyond it, KWin's decoration handling takes the contact and no
    // client ever holds it.
    const KWin::Window *window = KWin::input()->findToplevel(position);
    return window && (!window->decoration() || QRectF(window->clientGeometry()).contains(position));
}

QString Effect::describeTitleTouch(const QPointF &position) const
{
    // Only while Cards is up on the tablet, and only on a title: on the
    // decoration of the window KWin would deliver the touch to, or along the
    // top of the chosen card, whatever lies there.
    if (!m_cardStage || !m_cardStage->isActive() || !isTabletPoint(position)) return {};
    const KWin::EffectWindow *chosen = m_cardStage->selectedWindow();
    const KWin::Window *card = chosen ? chosen->window() : nullptr;
    const KWin::Window *under = KWin::input()->findToplevel(position);
    const bool onDecoration = under && under->decoration()
        && !QRectF(under->clientGeometry()).contains(position);
    bool onCardTop = false;
    if (card) {
        const QRectF frame(card->frameGeometry());
        onCardTop = position.x() >= frame.left() && position.x() < frame.right()
            && position.y() >= frame.top() - 12.0 && position.y() < frame.top() + 60.0;
    }
    if (!onDecoration && !onCardTop) return {};
    const auto name = [](const KWin::Window *window) { return logNameOf(window); };
    QString presentation = QStringLiteral("Spread");
    if (m_cardStage->presentation() == CardPresentation::Active) presentation = QStringLiteral("Active");
    else if (m_cardStage->presentation() == CardPresentation::Bento) presentation = QStringLiteral("Bento");
    const KWin::Window *moving = KWin::workspace()->moveResizeWindow();
    const auto box = [](const KWin::Window *window) {
        const QRectF frame(window->frameGeometry());
        return QStringLiteral(" frame %1,%2 %3x%4").arg(frame.x()).arg(frame.y()).arg(frame.width()).arg(frame.height());
    };
    QString line;
    QDebug(&line).nospace().noquote() << "in " << presentation << "; KWin delivers it to " << name(under)
        << (under ? (onDecoration ? " on its title or border" : " inside its client area") : "")
        << (under ? box(under) : QString())
        << "; the chosen card is " << name(card)
        << (card ? box(card) + (card->isActive() ? QStringLiteral(", focused") : QStringLiteral(", not focused"))
                       + (card->isMinimized() ? QStringLiteral(", minimized") : QString()) : QString())
        << (under && under == card ? " (the same window)" : " (a different window)")
        << "; KWin is moving " << name(moving)
        << "; Kadunce is carrying " << (m_carriedWindow ? "a window" : "nothing");
    if (m_touchWitness && m_touchWitness->describeTouchState)
        line += QStringLiteral("; before this press KWin ") + m_touchWitness->describeTouchState();
    return line;
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
    if (!tablet) return false;
    // Half-open and unrounded: the display's last physical rows map to within
    // a pixel of its logical bottom, and rounding them carried them off it.
    const QRectF area(tablet->geometry());
    return position.x() >= area.left() && position.x() < area.right()
        && position.y() >= area.top() && position.y() < area.bottom();
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
    KWin::EffectWindow *focused = KWin::effects->activeWindow();
    // §11: every display's own stage, under that display's name.
    struct StageView {
        QString display;
        const CardStageController *cards;
        CardWorkspaceSnapshot workspace;
    };
    std::vector<StageView> stages;
    eachCardDisplay([&](KWin::LogicalOutput *output) {
        stages.push_back({output->name(), m_cardStage, m_cardStage->workspaceSnapshot()});
    });
    const auto viewHolding = [&](const KWin::EffectWindow *window) -> const StageView * {
        for (const auto &view : stages)
            if (view.cards->liveCardIndex(window) >= 0) return &view;
        return nullptr;
    };

    QJsonArray applications;
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        if (!isApplicationWindow(window)) {
            continue;
        }
        const StageView *view = viewHolding(window);
        const auto *card = view ? view->workspace.find(windowIdentity(window)) : nullptr;
        const CardStageController *keeper = stageKeepingStackOf(window);
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
            {QStringLiteral("pane"), m_desktopStage->managesWindow(window)},
            // The display whose card stage holds it as a card; empty when none does.
            {QStringLiteral("stage"), card ? view->display : QString()},
            // Whether Kadunce rounds its corners, and what would keep it from it.
            {QStringLiteral("corners"), QJsonObject{
                {QStringLiteral("rounded"), trimsCorners(window)},
                {QStringLiteral("available"), roundsCorners()},
                {QStringLiteral("shadow"), drawsOutsideFrame(window)},
                // How its last frame was drawn, and the sizes that decide it.
                {QStringLiteral("drawnAs"), m_drawnAs.value(window)},
                {QStringLiteral("frame"), rectText(window->frameGeometry())},
                {QStringLiteral("expanded"), rectText(window->expandedGeometry())},
                {QStringLiteral("buffer"), rectText(window->bufferGeometry())},
                {QStringLiteral("maximized"), window->window()
                    && window->window()->maximizeMode() == KWin::MaximizeFull}}},
            // §11, Connected: the cards waiting behind a pane, front to back.
            {QStringLiteral("paneStack"), [&] {
                QJsonArray behind;
                if (keeper)
                    for (auto *member : keeper->paneStack(window)) behind.append(windowIdentity(member));
                return behind;
            }()},
        };
        if (card) {
            const CardStageController *cards = view->cards;
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
            const int id = cards->liveCards().indexOf(window) + 1;
            application.insert(QStringLiteral("entry"), cards->model().entryIndexForId(id));
            auto *display = screenNamed(view->display);
            if (display && cards->isActive()
                && cards->presentation() == CardPresentation::Spread
                && cards->paintSlot(window) != 99) {
                auto rect = cards->previewTargetForWindow(display, window);
                const auto pose = cards->stackPoseForWindow(window, rect.width());
                rect.translate(qRound(pose.x), qRound(pose.y));
                application.insert(QStringLiteral("spreadRect"), geometryContext(rect));
            }
        }
        applications.append(application);
    }

    // A note being typed over a window has the keys, and the window it is
    // stuck to stays the one in use: focus names that window and says so.
    KWin::EffectWindow *notesHost = notesSurfaceHost(focused);
    KWin::EffectWindow *inUse = notesHost ? notesHost : focused;
    QJsonObject focus;
    if (inUse && isApplicationWindow(inUse)) {
        const bool focusedHasCard = viewHolding(inUse) != nullptr;
        focus = {
            {QStringLiteral("windowId"), windowIdentity(inUse)},
            {QStringLiteral("appId"), applicationIdentity(inUse)},
            {QStringLiteral("title"), inUse->caption()},
            {QStringLiteral("hasCard"), focusedHasCard},
        };
        if (focusedHasCard) {
            focus.insert(QStringLiteral("cardId"),
                         windowIdentity(inUse));
        }
        if (notesHost) focus.insert(QStringLiteral("notes"), true);
    }

    // The display in use: its stage is the top-level cardStage, and it is the
    // display whose role is "tablet".
    const QJsonObject inUseStage = cardStageContext();

    QJsonArray displays;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (!output) {
            continue;
        }
        const bool tablet = isTabletOutput(output);
        const bool holdsCards = isCardDisplay(output);
        QJsonObject own;
        if (holdsCards) {
            SessionScope scope(const_cast<Effect *>(this), m_currentSession, stageKeyFor(output));
            own = cardStageContext();
            own.insert(QStringLiteral("spreadOpen"), m_cardStage->isActive()
                && m_cardStage->presentation() == CardPresentation::Spread && !m_cardStage->spreadOpening());
        }
        // What the display presents and the windows it shows that way: its
        // own Active card, or a layout's panes.
        QJsonArray shows;
        QString presents = QStringLiteral("none");
        if (m_desktopStage->hasSessionOnOutput(output->name())) {
            presents = QStringLiteral("layout");
            for (const auto &pane : m_desktopStage->panesOn(output->name()))
                if (pane && !pane->isMinimized()) shows.append(windowIdentity(pane));
        } else if (holdsCards && own.value(QStringLiteral("active")).toBool()) {
            presents = own.value(QStringLiteral("presentation")).toString();
            if (own.value(QStringLiteral("activeCardId")).isString())
                shows.append(own.value(QStringLiteral("activeCardId")));
        }
        QJsonObject display{
            {QStringLiteral("name"), output->name()},
            {QStringLiteral("role"), tablet
                ? QStringLiteral("tablet") : QStringLiteral("external")},
            {QStringLiteral("geometry"), geometryContext(output->geometry())},
            {QStringLiteral("bentoActive"),
             m_desktopStage->hasSessionOnOutput(output->name())},
            {QStringLiteral("sizing"), sizingName(sizingFor(output))},
            {QStringLiteral("sizingChosen"), !chosenSizingFor(output).isEmpty()},
            {QStringLiteral("holdsCards"), holdsCards},
            {QStringLiteral("primary"), output->name() == m_cardOutputName},
            {QStringLiteral("touchscreen"), output->name() == m_touchOutputName},
            {QStringLiteral("presentation"), presents},
            {QStringLiteral("shows"), shows},
            // Why its layout draws no dividers, when it has one and draws none.
            {QStringLiteral("dividersWithheld"), m_desktopStage->hasSessionOnOutput(output->name())
                ? m_desktopStage->dividersWithheld(output) : QString()},
        };
        if (holdsCards) display.insert(QStringLiteral("cardStage"), own);
        displays.append(display);
    }

    QJsonObject root{
        {QStringLiteral("schema"),
         QStringLiteral("co.goodinput.kadunce.workspace-context")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("focus"), focus.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(focus)},
        {QStringLiteral("applications"), applications},
        {QStringLiteral("cardStage"), inUseStage},
        {QStringLiteral("notice"), contextNotice()},
        {QStringLiteral("desktopStage"), QJsonObject{
            {QStringLiteral("active"), hasActiveDesktopStage()},
        }},
        {QStringLiteral("table"), QJsonObject{
            {QStringLiteral("open"), m_table.isOpen()},
        }},
        {QStringLiteral("displayContext"), QJsonObject{
            {QStringLiteral("edgeBackend"), m_usesDirectSystemEdges
                ? QStringLiteral("z13-direct")
                : QStringLiteral("plasma-native")},
            {QStringLiteral("displays"), displays},
            {QStringLiteral("spreadDisplay"), m_spreadChoice.display},
            {QStringLiteral("primaryDisplay"), m_cardOutputName},
        }},
    };
    return QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool Effect::activateApplicationWindow(const QString &windowId)
{
    if (!callerMayDrive()) return false;
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

        closeLauncherGuest();
        if (window->isMinimized()) {
            window->unminimize();
        }
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
    // gives an Active card, as the middle does. On Connected (§11) the edge
    // still names a place, the card alone there.
    if ((aim.kind == PlacementAimKind::Left || aim.kind == PlacementAimKind::Right)
        && !m_cardStage->isActive()) {
        for (auto *output : KWin::effects->screens()) {
            if (output && output->name() == aim.output && m_cardStage->canOwnCards(output)
                && !connectedOutputForDesktopStage(output))
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
        const auto side = sideChoiceOn(output, right, point.y());
        // §11, Connected: the box is the place, beside a column at that edge,
        // over the top of that side and under its bottom.
        if (connectedOutputForDesktopStage(output)) {
            const double width = area.width() / 2.0;
            const double top = side.large || side.over ? area.top() : area.center().y();
            const double bottom = side.over ? area.center().y() : area.bottom();
            return KWin::RectF(right ? area.right() - width : area.left(), top, width,
                               bottom - top);
        }
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
    if (!callerMayDrive()) return {};
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
    if (!callerMayDrive()) return;
    if (!m_placementAim && !m_placementPreview) return;
    m_placementAim.reset();
    m_placementPreview.reset();
    m_placementPreviewOutput.clear();
    KWin::effects->addRepaintFull();
}

bool Effect::placeApplication(const QStringList &applicationIds, const QString &windowId,
                              double x, double y, const QString &requestToken)
{
    if (!callerMayDrive()) return false;
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
    if (!callerMayDrive()) return;
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
    SessionScope scope(this, m_scopedSession, stageKeyFor(output));
    const bool card = m_cardStage->isActive() && m_cardStage->liveCardIndex(window) >= 0;
    if (m_cardStage->canOwnCards(output)) {
        // An ordinary window shown elsewhere comes to the card display as one
        // carried there would, and is then handled as a card already here. A
        // pane keeps its layout.
        if (!card && window->screen() != output) {
            if (m_carriedWindow || m_nativeCarry || m_desktopStage->managesWindow(window)
                || !admitTransferredWindowToTablet(window, output, [] { return true; })
                || m_cardStage->liveCardIndex(window) < 0) return false;
            (void)m_cardStage->promoteToActive(window);
            observeCardOwnership();
        }
        if (!activateApplicationWindow(windowIdentity(window))) return false;
        if (aim.kind == PlacementAimKind::Card) return true;
        // CARD-LIFECYCLE.md §3, as the Active card carried to that edge.
        const bool right = aim.kind == PlacementAimKind::Right;
        auto side = sideChoiceOn(output, right, point.y());
        side.connected = connectedOutputForDesktopStage(output);
        QPointer<KWin::LogicalOutput> destination = output;
        QPointer<KWin::EffectWindow> placed = window;
        QTimer::singleShot(0, this, [this, destination, placed, side, right] {
            if (!destination || !placed) return;
            // §11, Connected: the edge names a place, as it does for the
            // Active card carried there: alone on an open display, or among
            // the columns of the layout the card was played into.
            if (side.connected) {
                m_aloneOnPurpose.removeAll(placed);
                // A layout already standing takes the card as a pane first,
                // as a pick from Spread would; only an open display places
                // it alone.
                if (!m_desktopStage->managesWindow(placed))
                    (void)admitActivatedCardToLiveBento(placed);
                if (!m_desktopStage->managesWindow(placed) && m_cardStage->hasBentoGroup())
                    (void)playPickedCardIntoLayout(placed);
                if (m_desktopStage->managesWindow(placed))
                    (void)m_desktopStage->placePaneBySide(placed, destination, side);
                else if (m_cardStage->activeCardIdentity() == placed
                         && !m_cardStage->hasBentoGroup())
                    (void)placeActiveCardAlone(destination, side);
                observeCardOwnership();
                return;
            }
            if (m_cardStage->activeCardIdentity() != placed) return;
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

bool Effect::toggleBentoOnOutput(const QString &outputName)
{
    if (!callerMayDrive()) return false;
    const bool toggled = m_desktopStage->toggleOnOutput(outputName);
    observeCardOwnership();
    return toggled;
}

bool Effect::handoffBentoLeadToOutput(
    const QString &sourceName, const QString &destinationName)
{
    if (!callerMayDrive()) return false;
    return m_desktopStage->handoffLeadToOutput(
        sourceName, destinationName);
}

// The gutters are Kadunce's. Every card and Bento pane stands a gutter in from
// the display's edges and from its neighbours, and an application drawing its
// own title bar keeps an invisible resize border wider than that, so in a
// gutter the pointer was over a resize border and took a resize shape. While
// the pointer is in the gutter around one of Kadunce's own cards or panes,
// Kadunce holds it, and its shape says what a press there does: a page tab in
// the gap beside the Active card, a divider's resize across a divider's whole
// reach, panes included, and an arrow elsewhere, where nothing takes a press.
// KWin picks the pointer's window before any input filter runs, so the hold
// is the mouse interception KWin gives effects such as Overview; the input
// router comes before KWin's effect filter, so its presses still arrive.
void Effect::quietCardGap(const QPointF &position)
{
    // An open Table holds the pointer too, as Overview does: it is drawn over
    // the windows rather than being one, so without the hold KWin hands the
    // pointer to the client beneath, which sets its own shape and never hears
    // the pointer move again. While a finger types a name
    // on the keys it lets go: while an effect holds the pointer KWin finds no
    // window under any touch, so the keys would never feel their own taps.
    HeldPointerPlace place;
    HeldPointer held = HeldPointer::None;
    if (m_table.isOpen()) {
        place.tableOpen = true;
        place.tableCarrying = m_table.carrying();
        if (!(m_tableRenaming >= 0 && m_tableRenameByTouch)) held = heldPointer(place);
    } else {
        // A press, a drag and a move or resize under way keep what they have.
        if (KWin::input()->pointer()->areButtonsPressed() || KWin::waylandServer()->seat()->isDragPointer()
            || KWin::workspace()->moveResizeWindow()) return;
        // Since the last touch the pointer is hidden and still: a hold would
        // keep every touch from the window under it.
        if (m_pointerResting) held = HeldPointer::None;
        else {
            // A panel, or a popup of one lying over a divider, keeps its
            // own pointer, as its clicks are its own.
            const bool panel = isPanelPoint(position);
            if (!panel)
                for (const auto &rail : m_desktopStage->grabRails())
                    if (rail.hitArea.contains(position)) place.divider = rail.vertical ? 1 : 2;
            // The dock, the keys and Search beside the Active card keep their
            // own pointer.
            if (!panel && !inputPanelContainsForInput(position)
                && !(launcherGuestActiveForInput() && launcherGuestContainsForInput(position)))
                place.activeSide = isTabletPoint(position) ? activeSideForPoint(position)
                                                           : stackSideForPoint(position);
            place.cardGap = !panel && place.divider == 0 && inCardGap(position);
            held = heldPointer(place);
        }
    }
    if (held == m_heldPointer) return;
    const bool wasHeld = m_heldPointer != HeldPointer::None;
    m_heldPointer = held;
    m_gapHeld = held != HeldPointer::None;
    if (!m_gapHeld) {
        KWin::effects->stopMouseInterception(this);
        return;
    }
    showHeldPointer(held, wasHeld);
}

// KWin lets an effect holding the pointer choose among Qt's shapes, which the
// arrow, the resize shapes and grabbing are. A page tab is named in the
// cursor theme, so it is set on the source KWin shows for the hold; a theme
// without it shows the arrow.
void Effect::showHeldPointer(HeldPointer held, bool wasHeld)
{
    const Qt::CursorShape shape = held == HeldPointer::ResizeColumns ? Qt::SplitHCursor
        : held == HeldPointer::ResizeRows ? Qt::SplitVCursor
        : held == HeldPointer::Carrying ? Qt::ClosedHandCursor
        : Qt::ArrowCursor;
    if (wasHeld) KWin::effects->defineCursor(shape);
    else KWin::effects->startMouseInterception(this, shape);
    if (held != HeldPointer::PageLeft && held != HeldPointer::PageRight) return;
    // Another effect's hold, a drag or the lock screen shows a source of its
    // own, which keeps its shape.
    auto *source = qobject_cast<KWin::ShapeCursorSource *>(KWin::Cursors::self()->mouse()->source());
    if (!source || source->shape() != KWin::CursorShape(shape).name()) return;
    source->setShape(heldPointerName(held));
    if (source->image().isNull()) source->setShape(shape);
}

// A touch puts the pointer to rest, hidden where it was, until it moves
// again; an open Table keeps its own hold.
void Effect::touchBeganForInput()
{
    m_keysUpAtTouch = m_keysForPerson;
    m_pointerResting = true;
    if (m_table.isOpen() || m_heldPointer == HeldPointer::None) return;
    m_heldPointer = HeldPointer::None;
    m_gapHeld = false;
    KWin::effects->stopMouseInterception(this);
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
        // The cards a presented layout hides lie behind its panes, and the
        // gutter between the panes is the layout's, not theirs.
        if (m_cardStage->presentsBento() && window->effectWindow()
            && m_cardStage->liveCardIndex(window->effectWindow()) >= 0) continue;
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
    // A window floating at its own size takes what lands on it.
    for (auto *window : KWin::effects->stackingOrder())
        if (floatsAtItsOwnSize(window) && !window->isMinimized() && window->isOnCurrentDesktop()
            && QRectF(window->frameGeometry()).contains(position)) return 0;
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
        const auto edge = carryEdgeAt(output, position);
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
        } else {
            // §11, Connected: a card carried from Active to a side snaps
            // there too, so the room it leaves offers the other cards.
            if (reserved.placesAlone()) askRoomOffer(reserved.destinationOutput());
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
        && m_cardStage->canOwnCards(edgeOutput)
        && carryEdgeAt(edgeOutput, position) == CarryEdge::Top) {
        m_cardStage->finishCardGrab(false);
        m_lineDestinationWindow.clear();
        return true;
    }
    // A card let go on a display that cannot hold cards joins its layout, and
    // one the layout has no room for springs back into the row.
    QPointer<KWin::EffectWindow> grabbed = selectedWindow();
    QPointer<KWin::LogicalOutput> destination = KWin::effects->screenAt(position.toPoint());
    const bool toLayout = grabbed && destination
        && QRectF(destination->geometry()).contains(position)
        && !m_cardStage->canOwnCards(destination);
    const bool result = m_cardStage->finishCardGrabOnOutput(position);
    m_lineDestination.reset(); m_lineDestinationWindow.clear();
    if (result && toLayout && grabbed && destination && m_cardStage->liveCardIndex(grabbed) >= 0)
        showGestureNote(destination, position, GestureRefusal::NoRoom);
    return result;
}

void Effect::showGestureNote(KWin::LogicalOutput *output, QPointF contact, GestureRefusal reason)
{
    if (!output) return;
    const QString text = gestureRefusalText(reason);
    if (m_gestureNote) KWin::effects->addRepaint(KWin::RectF(m_gestureNote->box).toAlignedRect());
    ShownGestureNote note{output, gestureNoteBox(QRectF(output->geometry()), contact,
                                                 m_gestureNoteLabel.size(text)), text, {}};
    note.shown.start();
    qInfo() << "Kadunce" << Revision << "refused a gesture on" << output->name() << ":" << text;
    m_gestureNote = std::move(note);
    KWin::effects->addRepaint(KWin::RectF(m_gestureNote->box).toAlignedRect());
    // Said rather than drawn on a claimed display, so no frame ends it.
    if (contextLineClaimed(output)) m_contextNoticeEnd.start();
}


bool Effect::notesShownInSpread() const
{
    return m_stuckNotesWatcher && !m_stuckNotes.isEmpty() && tabletOutput()
        && m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread
        && !m_cardStage->spreadOpening() && !m_cardStage->launcherGuestActive()
        && !m_cardStage->cardGrabActive() && !m_table.isOpen() && !m_previewDesktop;
}

// What Spread says for its chosen entry: each entry by its front card, a
// pane's card with the panes beside it, and the entry's place in the row.
QString Effect::spreadLine()
{
    const SpreadModel &model = m_cardStage->model();
    const auto &cards = m_cardStage->liveCards();
    const int count = model.count();
    QList<SpreadReadingEntry> entries;
    entries.reserve(count);
    for (int i = 0; i < count; ++i) {
        const int id = model.idAtOffset(i - model.selectedIndex());
        KWin::EffectWindow *window = id >= 1 && id <= cards.size() ? cards.at(id - 1).data() : nullptr;
        SpreadReadingEntry entry;
        if (window && !window->isDeleted()) {
            entry.application = applicationDisplayName(window);
            entry.title = window->caption();
            entry.stacked = std::max(1, model.stackSizeForId(id));
            entry.sleeping = window->isMinimized();
            for (auto *pane : m_cardStage->layoutPartners(window))
                entry.pairedWith.append(applicationDisplayName(pane));
            entry.behind = namesBehindInSpread(id);
        }
        entries.append(entry);
    }
    return Kadunce::spreadLine(entries, model.selectedIndex());
}

// What Table says: what is carried and where it would go, the card chosen,
// or the tab under the finger, else the workspace shown.
QString Effect::tableLine() const
{
    const auto workspaceName = [this](int index) {
        const TableWorkspace &workspace = m_tableWorkspaces[index];
        QString name = workspace.name.isEmpty() ? tr("Workspace %1").arg(index + 1)
                                                : tr("Workspace %1, %2").arg(index + 1).arg(workspace.name);
        if (workspace.current) name = tr("%1, current").arg(name);
        return name;
    };
    const auto cardName = [](const TableCard &card) {
        SpreadReadingEntry entry;
        entry.application = card.application;
        entry.title = card.title;
        entry.stacked = card.stacked + 1;
        return spreadEntryName(entry);
    };
    const auto validWorkspace = [this](int index) { return index >= 0 && index < m_tableWorkspaces.size(); };
    if (m_table.cancelling()) return tr("Let go to cancel");
    if (m_table.carrying()) {
        if (m_table.over() == TableGesture::Over::Plus) return tr("New workspace");
        if (m_table.over() == TableGesture::Over::Workspace && validWorkspace(m_table.overWorkspace()))
            return tr("Move to %1").arg(workspaceName(m_table.overWorkspace()));
        const int from = m_table.carryFrom();
        const int card = m_table.carryCard();
        if (validWorkspace(from) && card >= 0 && card < m_tableWorkspaces[from].cards.size())
            return tr("Moving %1").arg(cardName(m_tableWorkspaces[from].cards[card]));
    }
    const int tray = m_table.trayWorkspace();
    if (m_table.level() == TableLevel::Cards && validWorkspace(tray) && m_table.card() >= 0
        && m_table.card() < m_tableWorkspaces[tray].cards.size())
        return cardName(m_tableWorkspaces[tray].cards[m_table.card()]);
    if (m_table.onPlus()) return tr("New workspace");
    if (validWorkspace(m_table.hovered())) return workspaceName(m_table.hovered());
    if (validWorkspace(m_table.shownWorkspace())) return workspaceName(m_table.shownWorkspace());
    return tr("Table");
}

// Spread and Table are said to a screen reader while either shows, the
// line following what is chosen. A workspace's name being typed has the
// focus to itself, and Search over Spread's centre speaks for itself.
void Effect::syncSpokenLine()
{
    if (!SpokenLine::listening() || m_tableRenaming >= 0) {
        m_spokenSurface = SpokenSurface::None;
        m_spokenText.clear();
        if (m_spokenLine && m_spokenLine->visible()) m_spokenLine->hide();
        return;
    }
    KWin::LogicalOutput *output = nullptr;
    SpokenSurface surface = SpokenSurface::None;
    QString line;
    if (m_table.isOpen()) {
        output = tableOutput();
        surface = SpokenSurface::Table;
        line = m_tableLine;
    } else if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread
               && !m_cardStage->launcherGuestActive() && !m_previewDesktop
               && !m_cardStage->liveCards().isEmpty()) {
        output = tabletOutput();
        surface = SpokenSurface::Spread;
        line = spreadLine();
    }
    if (!output || line.isEmpty()) {
        m_spokenSurface = SpokenSurface::None;
        m_spokenText.clear();
        if (m_spokenLine && m_spokenLine->visible()) m_spokenLine->hide();
        return;
    }
    // Opening says what it opened; after that only a change is said.
    const bool opened = surface != m_spokenSurface || !m_spokenLine || !m_spokenLine->visible();
    if (!opened && line == m_spokenText) return;
    m_spokenSurface = surface;
    m_spokenText = line;
    if (opened) line = surface == SpokenSurface::Table ? tr("Table, %1").arg(line) : tr("Spread, %1").arg(line);
    if (!m_spokenLine) m_spokenLine = std::make_unique<SpokenLine>();
    m_spokenLine->say(QRectF(output->geometry()).toRect(), line);
}

// Gooseberry's notes over a window are a layer surface laid over that
// window; its quick-note card is one that covers the whole display. A layer
// surface carries no application id, so KWin names it by its program,
// gooseberry, rather than by Gooseberry's desktop file name; either is taken.
// The notes take the keys while they are open, so they are told from the card
// by their size, and, while live, by taking no keys when they take none. A
// closing surface keeps its size, so it is told the same way through its fade.
bool Effect::isNotesSurface(const KWin::EffectWindow *window) const
{
    if (!window || !window->window() || !window->window()->inherits("KWin::LayerShellV1Window")) return false;
    const QString identity = applicationIdentity(window);
    if (identity.compare(QStringLiteral("io.github.carlsonjm.Gooseberry"), Qt::CaseInsensitive) != 0
        && identity.compare(QStringLiteral("gooseberry"), Qt::CaseInsensitive) != 0) {
        return false;
    }
    if (!window->isDeleted() && !window->acceptsFocus()) return true;
    const auto *output = window->screen();
    return output && window->frameGeometry().toRect() != output->geometry();
}

bool Effect::isSearchSurface(const KWin::EffectWindow *window)
{
    if (!window || !window->window() || !window->window()->inherits("KWin::LayerShellV1Window")) return false;
    const QString identity = applicationIdentity(window);
    return identity.compare(QStringLiteral("io.github.carlsonjm.Tettegouche"), Qt::CaseInsensitive) == 0
        || identity.compare(QStringLiteral("tettegouche"), Qt::CaseInsensitive) == 0;
}

// While Spread is up the notes are drawn on the cards instead, so that
// surface is left undrawn from the frame Spread opens, and through its fade
// as it goes, rather than once Gooseberry answers the pause: the notes read
// as one set moving onto the card. As Spread closes, Spread's picture of them
// stays over the Active card until the surface stands there (syncNotesHandover).
bool Effect::notesSurfaceStepsAside(const KWin::EffectWindow *window) const
{
    return (m_notesSpreadShown || m_notesHandover) && isNotesSurface(window);
}

void Effect::syncNotesHandover(bool spreadClosed)
{
    if (spreadClosed) {
        m_notesHandover.reset();
        auto *card = m_cardStage->presentation() == CardPresentation::Active
            ? m_cardStage->activeCardIdentity() : nullptr;
        const auto *entry = card ? m_stuckNotes.entryFor(card->internalId()) : nullptr;
        const bool asked = card && !m_notesAskedFor.isNull() && m_notesAskedFor == card->internalId();
        m_notesAskedFor = {};
        if (entry && (entry->shown || asked) && m_stuckNotesWatcher && m_stuckNotesWatcher->present()) {
            m_notesHandover = NotesHandover{card, {}, asked};
            m_notesHandover->since.start();
        }
    }
    if (!m_notesHandover) return;
    auto *card = m_notesHandover->card.data();
    const auto *entry = card && !card->isDeleted() ? m_stuckNotes.entryFor(card->internalId()) : nullptr;
    bool done = !entry || (!entry->shown && !m_notesHandover->asked) || !m_cardStage->isActive()
        || m_cardStage->presentation() != CardPresentation::Active || m_cardStage->activeCardIdentity() != card
        || m_notesHandover->since.elapsed() > NotesHandoverLimitMs;
    if (!done) {
        // Gooseberry's surface stands over the window once it is laid at the
        // window's place and size, as Gooseberry lays it once the desktop says
        // where the window is; from this frame it is drawn and Spread's
        // picture is not.
        const QRectF frame(card->frameGeometry());
        const QRectF display = card->screen() ? QRectF(card->screen()->geometry()) : QRectF();
        for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
            if (!window || window->isDeleted() || !window->window() || !window->window()->readyForPainting()
                || !isNotesSurface(window)) continue;
            if (notesSurfaceStandsOn(QRectF(window->frameGeometry()), frame, display)) {
                done = true;
                break;
            }
        }
    }
    if (done) {
        const bool late = m_notesHandover->since.elapsed() > NotesHandoverLimitMs;
        if (late) qInfo() << "Kadunce gave the Active card's notes back to Gooseberry without its surface in place";
        m_notesHandover.reset();
        KWin::effects->addRepaintFull();
    }
}

void Effect::paintNotesHandover(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                                QList<QRectF> &drawn)
{
    if (!m_notesHandover) return;
    auto *card = m_notesHandover->card.data();
    const auto *entry = card && !card->isDeleted() ? m_stuckNotes.entryFor(card->internalId()) : nullptr;
    if (!entry) return;
    for (const auto &mark : windowNoteMarks(QRectF(card->frameGeometry()), *entry)) {
        if (mark.kind == CardNoteMark::Kind::Dot) {
            m_noteStackRenderer.renderDot(renderTarget, viewport, mark.rect, entry->notes.first().colourHex,
                                          mark.count);
        } else {
            m_noteStackRenderer.renderNote(renderTarget, viewport, mark.rect, entry->notes.at(mark.note), 1.0);
        }
        drawn << mark.rect;
    }
}

// The window a note with the keys is stuck to: the latest used of those whose
// notes Gooseberry says are up, or, where it has not said yet, the window last
// in use, which the note was opened over.
KWin::EffectWindow *Effect::notesSurfaceHost(const KWin::EffectWindow *window) const
{
    if (!isNotesSurface(window)) return nullptr;
    KWin::EffectWindow *host = nullptr;
    quint64 latest = 0;
    for (KWin::EffectWindow *candidate : KWin::effects->stackingOrder()) {
        if (!isApplicationWindow(candidate) || candidate->isMinimized()
            || !m_stuckNotes.shown(candidate->internalId())) {
            continue;
        }
        const quint64 used = m_activationOrder.value(windowIdentity(candidate));
        if (!host || used > latest) {
            host = candidate;
            latest = used;
        }
    }
    if (host) return host;
    return isApplicationWindow(m_lastActiveApplication) ? m_lastActiveApplication.data() : nullptr;
}

QList<NotesCard> Effect::notesCards() const
{
    QList<NotesCard> cards;
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet) return cards;
    const auto &model = m_cardStage->model();
    const auto &windows = m_cardStage->liveCards();
    auto *selected = m_cardStage->selectedWindow();
    // The centred card first, as a tap finds cards, then the other faces.
    for (int pass = 0; pass < 2; ++pass) {
        for (int index = 0; index < windows.size(); ++index) {
            auto *window = windows.at(index).data();
            if (!window || window->isDeleted() || (window == selected) != (pass == 0)
                || m_cardStage->paintSlot(window) == 99 || m_cardStage->thrownAway(window)) continue;
            const int cardId = index + 1;
            if (model.stackPositionForId(cardId) != model.stackActivePositionForId(cardId)) continue;
            // Where the card was last drawn, as the notes on it were; a card
            // not drawn yet stands where Spread puts it.
            const auto drawn = m_cardLabelTargets.constFind(window);
            const QRectF rect = drawn != m_cardLabelTargets.cend()
                ? *drawn : QRectF(m_cardStage->posedTargetForWindow(tablet, window));
            cards.append({window->internalId(), rect, window->frameGeometry().size(),
                          m_cardFaceRects.value(window)});
        }
    }
    return cards;
}

KWin::EffectWindow *Effect::notesCardWindow(const QUuid &id) const
{
    for (const auto &window : m_cardStage->liveCards())
        if (window && !window->isDeleted() && window->internalId() == id) return window.data();
    return nullptr;
}

bool Effect::pressNotesFromInput(const QPointF &position)
{
    if (!notesShownInSpread() || !m_cardStage->rowStill()) return false;
    const bool taken = m_stuckNotes.press(position, notesCards());
    if (taken) KWin::effects->addRepaintFull();
    return taken;
}

void Effect::moveNotesFromInput(const QPointF &position)
{
    m_stuckNotes.move(position);
    if (m_stuckNotes.carrying()) KWin::effects->addRepaintFull();
}

void Effect::holdNotesFromInput()
{
    if (notesShownInSpread() && m_stuckNotes.hold()) KWin::effects->addRepaintFull();
}

void Effect::releaseNotesFromInput(const QPointF &position, bool still)
{
    const auto cards = notesCards();
    const auto request = m_stuckNotes.release(position, still, cards);
    KWin::effects->addRepaintFull();
    auto *window = request ? notesCardWindow(request->window) : nullptr;
    if (!window || !m_stuckNotesWatcher) return;
    // Gooseberry acts and says so; what Spread draws follows its word.
    const QString windowId = window->internalId().toString();
    using Kind = StuckNotesSpread::Request::Kind;
    if (request->kind == Kind::Stick) {
        m_stuckNotesWatcher->stickTo(request->noteId, windowId, window->caption(),
                                     applicationIdentity(window));
        return;
    }
    // A stuck note is typed on its window, never in Spread: the notes are
    // asked up over the window, a tapped note ready to type, and the card
    // opens as a tap on it anywhere else would. Gooseberry stays paused until
    // the card is Active, and then shows them there.
    if (request->kind == Kind::Write)
        m_stuckNotesWatcher->write(request->noteId, windowId, window->caption(), applicationIdentity(window));
    else
        m_stuckNotesWatcher->show(windowId, window->caption(), applicationIdentity(window));
    m_notesAskedFor = request->window;
    const auto reaches = [this](const QPointF &at) {
        auto *card = m_cardStage->cardUnderPoint(at);
        return card ? card->internalId() : QUuid();
    };
    if (const auto at = StuckNotesSpread::openingPoint(request->window, position, cards, reaches))
        tapSpreadFromInput(*at);
}

void Effect::syncNotesPause()
{
    // Gooseberry's notes over the windows step aside while Spread covers them,
    // or while a window is carried, which they cannot follow under the hand,
    // and come back as it closes or lands, still out.
    if (!m_stuckNotesWatcher || !m_stuckNotesWatcher->present()) {
        m_notesPaused = false;
        return;
    }
    const bool aside = m_notesSpreadShown || bool(m_carriedWindow);
    if (m_notesPaused == aside) return;
    m_notesPaused = aside;
    m_stuckNotesWatcher->pause(m_notesPaused);
}

void Effect::cancelNotesFromInput()
{
    m_stuckNotes.cancel();
    KWin::effects->addRepaintFull();
}

void Effect::paintStuckNotes(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                             const KWin::Region &deviceRegion, KWin::LogicalOutput *screen,
                             const QRectF &heldRect, QList<QRectF> &drawn)
{
    if (m_stuckNotes.isEmpty() || m_cardStage->launcherGuestActive()) return;
    const auto &model = m_cardStage->model();
    const auto &windows = m_cardStage->liveCards();
    auto *held = m_cardStage->cardGrabActive() ? m_cardStage->selectedWindow() : nullptr;
    const auto carried = m_stuckNotes.carriedNote();
    for (int index = 0; index < windows.size(); ++index) {
        auto *window = windows.at(index).data();
        if (!window || window->isDeleted() || m_cardStage->paintSlot(window) == 99) continue;
        const int cardId = index + 1;
        if (model.stackPositionForId(cardId) != model.stackActivePositionForId(cardId)) continue;
        const auto *entry = m_stuckNotes.entryFor(window->internalId());
        const auto target = m_cardLabelTargets.constFind(window);
        if (!entry || target == m_cardLabelTargets.cend()) continue;
        // Worked out afresh every frame from where the card and its window
        // are drawn this frame, so the notes ride the card through every
        // motion. A held card is above the row, so no note of the row shows
        // through it.
        const auto marks = cardNoteMarks(*target, m_cardFaceRects.value(window), window->frameGeometry().size(),
                                         *entry);
        for (const auto &mark : marks) {
            if (window != held && !heldRect.isEmpty() && mark.reach.intersects(heldRect)) continue;
            if (mark.kind == CardNoteMark::Kind::Dot) {
                m_noteStackRenderer.renderDot(renderTarget, viewport, mark.rect, entry->notes.first().colourHex,
                                              mark.count);
                drawn << mark.rect;
                continue;
            }
            if (mark.kind == CardNoteMark::Kind::Stack) {
                QStringList colours;
                for (const int note : mark.sheetNotes) colours.append(entry->notes.at(note).colourHex);
                m_noteStackRenderer.renderStack(renderTarget, viewport, mark.sheets, colours,
                                                 entry->notes.at(mark.note).title, mark.count);
                drawn << mark.sheets;
                continue;
            }
            const auto &note = entry->notes.at(mark.note);
            // The note in hand leaves a faint place behind; one standing in
            // its place is faint while its notes are put away.
            const bool lifted = carried && carried->id == note.id;
            const bool putAway = !m_stuckNotes.shown(window->internalId());
            m_noteStackRenderer.renderNote(renderTarget, viewport, mark.rect, note,
                                           lifted ? 0.35 : putAway ? NoteGeometry::PutAwayOpacity : 1.0);
            drawn << mark.rect;
        }
    }
    if (carried) {
        if (auto *target = notesCardWindow(m_stuckNotes.dropTarget(notesCards()));
            target && m_destinationShader) {
            if (const auto box = m_cardLabelTargets.constFind(target); box != m_cardLabelTargets.cend()) {
                drawDestinationOutline(renderTarget, viewport, deviceRegion, screen, *box);
                drawn.append(*box);
            }
        }
        m_noteStackRenderer.renderNote(renderTarget, viewport, m_stuckNotes.carriedRect(), *carried, 1.0);
        drawn.append(m_stuckNotes.carriedRect());
    }
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
    // Three fingers on the touchscreen open its display's Spread.
    followSpreadGestureOn(progress, screenNamed(m_touchOutputName));
}

void Effect::followSpreadGestureOn(qreal progress, KWin::LogicalOutput *output)
{
    using Mode = SpreadGesture::Mode;
    auto &gesture = m_spreadGesture;
    if (!gesture.key) gesture.key = output ? stageKeyFor(output) : m_scopedKey;
    SessionScope scope(this, m_currentSession, gesture.key);
    noteDisplayInUse(output);
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
    SessionScope scope(this, m_currentSession, m_spreadGesture.key);
    // Past halfway Spread opens; short of it the card springs back.
    if (m_spreadGesture.mode == SpreadGesture::Mode::Follow)
        m_cardStage->finishOpenSpread(m_spreadGesture.progress > 0.5);
    m_spreadGesture = {};
}

void Effect::setPagingShortcutsActive(bool active)
{
    // Each display's cards answer the keys while that display shows them.
    m_cardsShownForKeys.insert(m_scopedKey, active);
}

QString Effect::keyStageKey() const
{
    // §11: the keys act on one display. While a Spread is open, the one on
    // the display in use, else any open; otherwise the focused window's
    // display, then the active one, then the primary display.
    if (!m_cardsOnEveryDisplay) return QString();
    QString open;
    eachCardDisplay([&](KWin::LogicalOutput *) {
        if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread
            && (open.isEmpty() || m_scopedKey == m_inUseKey)) open = m_scopedKey;
    });
    if (!open.isEmpty()) return open;
    KWin::EffectWindow *focused = KWin::effects->activeWindow();
    if (focused && isCardDisplay(focused->screen())) return stageKeyOf(focused);
    KWin::LogicalOutput *active = KWin::effects->activeScreen();
    return isCardDisplay(active) ? active->name() : m_cardOutputName;
}

bool Effect::answerKey(int key, Qt::KeyboardModifiers modifiers, bool repeat)
{
    const QString stage = keyStageKey();
    SessionScope scope(this, m_scopedSession, stage);
    KeyContext context;
    context.table = true;
    context.cardsShown = m_cardsShownForKeys.value(m_scopedKey);
    KWin::EffectWindow *focused = KWin::effects->activeWindow();
    // The card a display sized Solo shows, other than the primary display's,
    // answers the keys as a pane: Meta and the arrows reach the display beside.
    context.pane = focused && (m_desktopStage->managesWindow(focused)
        || (context.cardsShown && m_cardsOnEveryDisplay && !connectedForCardStage()
            && tabletOutput() != primaryOutput()
            && m_cardStage->presentation() == CardPresentation::Active
            && m_cardStage->activeCardIdentity() == focused));
    // So does the Active card of any display sized Solo with cards on every
    // display: Meta and Shift's arrows send it, with its Stack, to the one
    // beside (carryToward), the primary display's included.
    context.lone = context.cardsShown && m_cardStage->presentation() == CardPresentation::Active
        && m_cardStage->activeCardIdentity()
        && (connectedForCardStage()
            || (m_cardsOnEveryDisplay && m_cardStage->activeCardIdentity() == focused));
    context.stacked = context.cardsShown && m_cardStage->presentation() == CardPresentation::Active
        && m_cardStage->stackFrontToBack(m_cardStage->activeCardIdentity()).size() > 1;
    // Plain keys go to an effect holding the keyboard, as Table does while open.
    context.spreadShown = context.cardsShown && !KWin::effects->hasKeyboardGrab()
        && m_cardStage->presentation() == CardPresentation::Spread
        && !m_cardStage->launcherGuestActive() && !m_cardStage->cardGrabActive();
    const KeyAction action = keyActionFor(key, modifiers, context);
    if (action == KeyAction::None || (repeat && !keyActionRepeats(action))) return false;
    // Open, Table keeps every key but its own.
    if (m_table.isOpen() && action != KeyAction::Table) return false;
    // A ladder shown by the keys answers only Shift's Up and Down; any other
    // key of Kadunce's places the card first.
    if (m_stackScrubByKeys && action != KeyAction::Deeper && action != KeyAction::LetGo)
        QTimer::singleShot(0, this, &Effect::commitKeyLadder);
    // The action runs after the key's own handling, never inside an input
    // filter's callback.
    noteDisplayInUse(screenNamed(m_scopedKey));
    onThisStageLater(0, [this, action]() { runKeyAction(action); });
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
    case KeyAction::FocusLeft: focusToward(KeyDirection::Left); break;
    case KeyAction::FocusRight: focusToward(KeyDirection::Right); break;
    case KeyAction::FocusUp: focusToward(KeyDirection::Up); break;
    case KeyAction::FocusDown: focusToward(KeyDirection::Down); break;
    case KeyAction::CarryLeft: carryToward(KeyDirection::Left); break;
    case KeyAction::CarryRight: carryToward(KeyDirection::Right); break;
    case KeyAction::Deeper: stepKeyLadder(1); break;
    case KeyAction::LetGo:
        if (m_stackScrubByKeys) stepKeyLadder(-1);
        else if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread)
            (void)m_cardStage->pullOutSelected();
        else if (KWin::EffectWindow *focused = KWin::effects->activeWindow();
                 focused && m_desktopStage->managesWindow(focused))
            letPaneGo();
        else letActiveCardGo();
        break;
    case KeyAction::DividerLeft: moveDividerByKey(-1); break;
    case KeyAction::DividerRight: moveDividerByKey(1); break;
    case KeyAction::Hold:
    case KeyAction::None: break;
    }
}

QList<KWin::EffectWindow *> Effect::keyTargets() const
{
    QList<KWin::EffectWindow *> targets;
    for (auto *output : KWin::effects->screens()) {
        if (!output) continue;
        for (const auto &pane : m_desktopStage->panesOn(output->name()))
            if (pane && !pane->isDeleted() && !pane->isMinimized() && !targets.contains(pane.data()))
                targets.append(pane.data());
    }
    // The Active card each display shows.
    eachCardDisplay([&](KWin::LogicalOutput *) {
        if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Active)
            if (KWin::EffectWindow *card = m_cardStage->activeCardIdentity();
                card && !card->isMinimized() && !targets.contains(card))
                targets.append(card);
    });
    return targets;
}

KWin::LogicalOutput *Effect::outputToward(const KWin::LogicalOutput *output, KeyDirection direction) const
{
    if (!output) return nullptr;
    QList<KWin::LogicalOutput *> others;
    QList<QRectF> rects;
    for (auto *other : KWin::effects->screens()) {
        if (!other || other == output) continue;
        others.append(other);
        rects.append(QRectF(other->geometry()));
    }
    const int to = keyNeighbour(QRectF(output->geometry()), rects, direction);
    return to < 0 ? nullptr : others.at(to);
}

KWin::EffectWindow *Effect::paneUsedBefore(const KWin::EffectWindow *pane) const
{
    if (!pane || !pane->screen()) return nullptr;
    KWin::EffectWindow *before = nullptr;
    quint64 latest = 0;
    for (const auto &other : m_desktopStage->panesOn(pane->screen()->name())) {
        if (!other || other.data() == pane || other->isDeleted()) continue;
        const quint64 at = m_activationOrder.value(windowIdentity(other.data()), 0);
        if (!before || at > latest) {
            before = other.data();
            latest = at;
        }
    }
    return before;
}

void Effect::focusToward(KeyDirection direction)
{
    KWin::EffectWindow *from = KWin::effects->activeWindow();
    if (!from) return;
    QList<KWin::EffectWindow *> targets = keyTargets();
    targets.removeAll(from);
    QList<QRectF> rects;
    for (auto *target : std::as_const(targets)) rects.append(QRectF(target->frameGeometry()));
    const int to = keyNeighbour(QRectF(from->frameGeometry()), rects, direction);
    if (to < 0) return;
    qInfo() << "Kadunce" << Revision << "moved the focus from" << logNameOf(from)
            << "to" << logNameOf(targets.at(to));
    KWin::effects->activateWindow(targets.at(to));
}

void Effect::carryToward(KeyDirection direction)
{
    const bool right = direction == KeyDirection::Right;
    KWin::EffectWindow *focused = KWin::effects->activeWindow();
    SessionScope scope(this, m_scopedSession, stageKeyOf(focused));
    // §11, Every display: the Active card of a display sized Solo has no room
    // beside it, so it goes on to the display beside with its Stack, and is
    // shown there: in the layout there, or as its Active card.
    if (focused && m_cardsOnEveryDisplay && m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Active
        && m_cardStage->activeCardIdentity() == focused && !connectedForCardStage()) {
        KWin::LogicalOutput *next = focused->screen() ? outputToward(focused->screen(), direction) : nullptr;
        if (!next || !isCardDisplay(next)) return;
        qInfo() << "Kadunce" << Revision << "sends" << logNameOf(focused) << "to" << next->name();
        if (!moveCardToDisplay(focused, next, true))
            qInfo() << "Kadunce" << Revision << "could not send" << logNameOf(focused) << "to" << next->name();
        return;
    }
    if (focused && m_desktopStage->managesWindow(focused) && focused->window() && focused->screen()) {
        KWin::LogicalOutput *output = focused->screen();
        // A pane beside it on its display trades places with it.
        QList<KWin::EffectWindow *> beside;
        QList<QRectF> rects;
        for (const auto &other : m_desktopStage->panesOn(output->name())) {
            if (!other || other.data() == focused || other->isDeleted()) continue;
            beside.append(other.data());
            rects.append(QRectF(other->frameGeometry()));
        }
        const int to = keyNeighbour(QRectF(focused->frameGeometry()), rects, direction);
        if (to >= 0) {
            if (m_desktopStage->swapPanes(focused, beside.at(to))) {
                qInfo() << "Kadunce" << Revision << "swapped" << logNameOf(focused)
                        << "with" << logNameOf(beside.at(to)) << "by the keys";
                observeCardOwnership();
                KWin::effects->activateWindow(focused);
            } else {
                qInfo() << "Kadunce" << Revision << "found" << logNameOf(focused)
                        << "could not trade places with" << logNameOf(beside.at(to));
            }
            return;
        }
        // With open room toward that edge, it moves there, as a side snap
        // would place it.
        if (connectedOutputForDesktopStage(output)
            && keyRoomToward(QRectF(focused->frameGeometry()), QRectF(output->geometry()), direction)
            && m_desktopStage->placePaneBySide(focused, output, BentoSidePlacement{right, true, true})) {
            qInfo() << "Kadunce" << Revision << "placed" << logNameOf(focused)
                    << (right ? "right" : "left") << "by the keys";
            observeCardOwnership();
            return;
        }
        // At the display's edge it goes on to the display beside, as KWin's
        // own window-to-screen key sends it (handleWindowOutputChanged).
        if (KWin::LogicalOutput *next = outputToward(output, direction)) {
            qInfo() << "Kadunce" << Revision << "sends" << logNameOf(focused) << "to" << next->name();
            // The pane it leaves on Spread's display is its again when it
            // comes back.
            if (isTabletOutput(output)) {
                LeftLayout left;
                left.right = QRectF(focused->frameGeometry()).center().x()
                    > QRectF(output->geometry()).center().x();
                for (const auto &other : m_desktopStage->panesOn(output->name()))
                    if (other && other.data() != focused) left.partners.append(other);
                m_leftLayouts.insert(focused, left);
            }
            focused->window()->sendToOutput(next);
        }
        return;
    }
    // The Active card alone on a Connected display: a full card takes the
    // half toward that side, a half moves into the open half, and at the edge
    // it goes on to the display beside.
    KWin::EffectWindow *card = m_cardStage->activeCardIdentity();
    if (!card || !card->window() || !card->screen() || !connectedForCardStage()) {
        qInfo() << "Kadunce" << Revision << "found nothing to carry; the focus is on"
                << (focused ? logNameOf(focused) : QStringLiteral("nothing"));
        return;
    }
    KWin::LogicalOutput *output = card->screen();
    const QRectF frame(card->frameGeometry());
    const QRectF display(output->geometry());
    const bool wide = frame.width() > display.width() * 0.6;
    if ((wide || keyRoomToward(frame, display, direction))
        && placeActiveCardAlone(output, BentoSidePlacement{right, true, true})) {
        qInfo() << "Kadunce" << Revision << "placed" << logNameOf(card)
                << (right ? "right" : "left") << "by the keys";
        observeCardOwnership();
        return;
    }
    if (KWin::LogicalOutput *next = outputToward(output, direction)) {
        qInfo() << "Kadunce" << Revision << "sends" << logNameOf(card) << "to" << next->name();
        card->window()->sendToOutput(next);
    }
}

void Effect::stepKeyLadder(int delta)
{
    // A ladder a hand is holding is the hand's.
    if (m_stackScrub.active() && !m_stackScrubByKeys) return;
    if (!m_stackScrub.active()) {
        KWin::EffectWindow *pane = KWin::effects->activeWindow();
        if (!pane || !m_desktopStage->managesWindow(pane) || delta <= 0) return;
        if (m_cardStage->paneStack(pane).isEmpty()) {
            // With no Stack it joins the one of the pane used before it, as a
            // pane rested on another does.
            if (KWin::EffectWindow *onto = paneUsedBefore(pane); onto && stackPaneOnto(pane, onto))
                qInfo() << "Kadunce" << Revision << "stacked" << logNameOf(pane) << "by the keys";
            return;
        }
        m_stackScrubPane = pane;
        m_stackScrubByKeys = true;
        m_stackScrub.begin(int(m_cardStage->paneStack(pane).size()) + 1, 0, 0.0);
        qInfo() << "Kadunce" << Revision << "shows the ladder of" << logNameOf(pane) << "by the keys";
    }
    m_stackScrub.stepKey(delta);
    refreshStackScrubShown();
    KWin::effects->addRepaintFull();
}

void Effect::commitKeyLadder()
{
    if (!m_stackScrubByKeys) return;
    const QPointer<KWin::EffectWindow> pane = m_stackScrub.active() ? m_stackScrubPane : nullptr;
    const int place = m_stackScrub.place;
    // A Stack that changed under the ladder has nothing to order.
    const bool whole = pane && int(m_cardStage->paneStack(pane).size()) + 1 == m_stackScrub.size;
    endStackScrub();
    if (whole && place > 0) (void)reorderPaneStack(pane, place);
}

void Effect::keyReleased(int key)
{
    // Queued behind the actions of the keys pressed before it, so a ladder
    // those keys begin is placed even when Meta is let go at once.
    if (key == Qt::Key_Meta || key == Qt::Key_Super_L || key == Qt::Key_Super_R)
        QTimer::singleShot(0, this, &Effect::commitKeyLadder);
}

void Effect::letPaneGo()
{
    KWin::EffectWindow *pane = KWin::effects->activeWindow();
    if (!pane || !m_desktopStage->managesWindow(pane)) return;
    // Its Stack is kept by the stage of the display it stands on.
    SessionScope scope(this, m_scopedSession, stageKeyOf(pane));
    const QList<KWin::EffectWindow *> behind = m_cardStage->paneStack(pane);
    if (!behind.isEmpty()) {
        // Out of its Stack: the next card takes the pane, and this one waits
        // in Spread as a card of its own, as one pulled down there does.
        KWin::EffectWindow *next = behind.first();
        m_cardStage->forgetPaneStack(pane);
        m_cardStage->notePaneStack(next, behind.mid(1));
        if (!m_desktopStage->replacePaneWithCard(pane, next,
                [this, next] {
                    return m_cardStage->releaseCardToLiveBento(next);
                })) {
            m_cardStage->forgetPaneStack(next);
            m_cardStage->notePaneStack(pane, behind);
            return;
        }
        m_cardStage->settlePaneStacks();
        observeCardOwnership();
        qInfo() << "Kadunce" << Revision << "took" << logNameOf(pane) << "out of its Stack by the keys";
        // The card let go joins the layout as a pane of its own beside the
        // Stack it left, as a card called while the layout shows does; with
        // no room it waits in its display's Spread, as one pulled down does.
        const QPointer<KWin::EffectWindow> released = pane;
        const QPointer<KWin::EffectWindow> stayed = next;
        const QString key = stageKeyOf(stayed);
        QTimer::singleShot(0, this, [this, released, stayed, key] {
            SessionScope scope(this, m_scopedSession, key);
            const bool joined = released && !released->isDeleted()
                && m_cardStage->liveCardIndex(released) >= 0
                && m_desktopStage->admitCardToLiveBento(released,
                    [this, released] { return m_cardStage->releaseCardToLiveBento(released); });
            if (joined) {
                qInfo() << "Kadunce" << Revision << "gave" << logNameOf(released)
                        << "a pane of its own by the keys";
                observeCardOwnership();
                KWin::effects->activateWindow(released);
            } else if (stayed && !stayed->isDeleted()) {
                KWin::effects->activateWindow(stayed);
            }
            KWin::effects->addRepaintFull();
        });
        return;
    }
    // Out of the layout: it waits in Spread as a card, and the pane used
    // last has the keys.
    const QString outputName = pane->screen() ? pane->screen()->name() : QString();
    const QPointer<KWin::EffectWindow> released = pane;
    const bool left = m_desktopStage->extractPaneToCards(pane, [] { return true; }, true);
    if (!left) return;
    qInfo() << "Kadunce" << Revision << "took" << logNameOf(pane) << "out of its layout by the keys";
    observeCardOwnership();
    KWin::EffectWindow *latest = nullptr;
    for (const auto &standing : m_desktopStage->panesOn(outputName)) {
        if (!standing || standing == released || standing->isDeleted() || standing->isMinimized())
            continue;
        if (!latest || lastUsedForDesktopStage(standing) > lastUsedForDesktopStage(latest))
            latest = standing.data();
    }
    if (latest && KWin::effects->activeWindow() == released) KWin::effects->activateWindow(latest);
}

void Effect::letActiveCardGo()
{
    KWin::EffectWindow *card = m_cardStage->activeCardIdentity();
    QList<KWin::EffectWindow *> stack = m_cardStage->stackFrontToBack(card);
    if (!card || stack.size() < 2 || !m_cardStage->letActiveCardOutOfStack()) return;
    stack.removeAll(card);
    const QPointer<KWin::EffectWindow> released = card;
    const QPointer<KWin::EffectWindow> next = stack.first();
    qInfo() << "Kadunce" << Revision << "took the Active card" << logNameOf(card)
            << "out of its Stack by the keys";
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
    // Solo, it stays the card shown, and the Stack it left waits in Spread.
    KWin::LogicalOutput *output = card->screen();
    if (!output || !connectedOutputForDesktopStage(output)) return;
    // §11, Connected: the two are laid out side by side, the rest of the
    // Stack behind the card it left, and the layout fills as Meta+B fills it.
    if (m_desktopStage->hasSessionOnOutput(output->name())) {
        toggleBento();
        return;
    }
    if (!next || !pairCardsIntoBento(output, released, next, BentoSidePlacement{false, true})) {
        qInfo() << "Kadunce" << Revision << "could not lay" << logNameOf(released)
                << "out beside the Stack it left on" << output->name();
        return;
    }
    qInfo() << "Kadunce" << Revision << "laid" << logNameOf(released) << "out beside"
            << logNameOf(next) << "on" << output->name();
    observeCardOwnership();
    onThisStageLater(0, [this, output = QPointer(output)] { fillLayoutWithCards(output); });
}

void Effect::moveDividerByKey(int delta)
{
    KWin::EffectWindow *pane = KWin::effects->activeWindow();
    if (!pane || !m_desktopStage->managesWindow(pane) || !pane->screen()) return;
    const QRectF frame(pane->frameGeometry());
    const QString output = pane->screen()->name();
    // The divider at the pane's right edge, else at its left.
    std::optional<DesktopStageController::GrabRail> chosen;
    double best = 0.0;
    for (const auto &rail : m_desktopStage->grabRails()) {
        if (rail.output != output || !rail.vertical) continue;
        if (rail.hitArea.bottom() < frame.top() || rail.hitArea.top() > frame.bottom()) continue;
        const double x = rail.pill.center().x();
        const double distance = std::min(std::abs(x - frame.right()), std::abs(x - frame.left()) + 1.0);
        if (distance > 40.0 || (chosen && distance >= best)) continue;
        chosen = rail;
        best = distance;
    }
    if (!chosen) return;
    const QPointF start = chosen->hitArea.center();
    if (!m_desktopStage->beginRail(start)) return;
    m_desktopStage->updateRail(start + QPointF(pane->screen()->geometry().width() * 0.05 * delta, 0.0));
    m_desktopStage->finishRail(true);
    qInfo() << "Kadunce" << Revision << "moved a divider of" << logNameOf(pane) << "by the keys";
}

void Effect::activateSelectedFromInput()
{
    // Defer the state change so the input filter is never destroyed from
    // inside one of its own callbacks.
    const auto ticket = m_inputActivationGuard.issue();
    const QPointer<KWin::EffectWindow> requested = selectedWindow();
    onThisStageLater(0, [this, ticket, requested]() {
        if (m_inputActivationGuard.accepts(ticket)
            && requested && !requested->isDeleted() && selectedWindow() == requested
            && !nativeWindowInteractionForInput() && !m_cardStage->cardGrabActive()
            && !m_cardStage->launcherGuestActive() && m_cardStage->isActive()
            && m_cardStage->presentation() == CardPresentation::Spread) {
            // CARD-LIFECYCLE.md section 6: choosing a pane's card resumes its
            // layout, on that pane. A refused resume is not an invitation to
            // try the individual-card path on it, which cannot admit a pane
            // and leaves the selection doing nothing at all.
            if (m_cardStage->selectedIsBentoGroup()) {
                if (!m_cardStage->resumeSelectedBentoProjection()) {
                    qWarning() << "Kadunce" << Revision
                               << "declined to resume the selected Bento layout";
                } else if (m_desktopStage->managesWindow(requested)
                           && KWin::effects->activeWindow() != requested) {
                    KWin::effects->activateWindow(requested);
                }
                observeCardOwnership();
            // §10: a card the top edge left alone is picked alone, as it left.
            } else if (!requested->isMinimized() && !m_aloneOnPurpose.contains(requested)
                       && playPickedCardIntoLayout(requested)) {
                observeCardOwnership();
            } else if (requested->isMinimized()) {
                // §2: selecting a sleeping card wakes it and presents it as
                // Active, as picking it from the dock does.
                KWin::effects->activateWindow(requested);
            } else {
                toggleOwnedPresentation(true);
            }
        }
    });
}

void Effect::cardGutterChangedForCardStage()
{
    m_desktopStage->relayAllSessions();
    KWin::effects->addRepaintFull();
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
    m_letGo = false;
    if (m_cardStage->launcherGuestActive()) {
        dismissLauncherGuestFromInput();
    }
    {
        // §2: the live session's panes become cards in Spread, whether or not
        // this stage already owns individual cards beside them.
        KWin::LogicalOutput *tablet = tabletOutput();
        if (tablet && m_desktopStage->hasSessionOnOutput(tablet->name())) {
            m_desktopStage->transferTabletSessionToSpread(tablet,
                [this](const auto &projection, const auto &commit) {
                    return m_cardStage->admitBentoStack(projection, commit);
                });
            m_cardStage->standPanesInCardPlace();
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
                       << "declined to resume the selected Bento layout";
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
    m_letGo = true;
    m_letGoOnDisplay.clear();
    qInfo() << "Kadunce" << Revision << "lets every window go";
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
        // Each display's cards go back where their records hold, on that display.
        forEachDisplayStage([&](CardStageController &stage) {
            hadCards = hadCards || stage.isActive();
            stage.release();
            // Kept, a Stack would stand behind that window again when cards
            // next start, though every window is its own now.
            stage.forgetAllPaneStacks();
        });
    });
    // Stacks carried to desktops not entered yet stay windows.
    m_pendingDecks.clear();
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
    if (stepPaneStack(delta)) return;
    if (m_cardStage->stepActiveStack(delta)) {
        if (auto *shown = m_cardStage->activeCardIdentity()) {
            appearInPlace(shown);
            stepInUnseen(shown);
            KWin::effects->activateWindow(shown);
        }
        return;
    }
    m_cardStage->pageStack(delta);
}

bool Effect::stepPaneStack(int delta)
{
    // CARD-LIFECYCLE.md §11: the focused pane's Stack steps in place. The
    // pane's slot shows the next card, and the layout keeps its shape.
    KWin::EffectWindow *pane = KWin::effects->activeWindow();
    if (!pane || !m_desktopStage->managesWindow(pane)) return false;
    SessionScope scope(this, m_scopedSession, stageKeyOf(pane));
    // Its own display's layout, Kadunce showing it as Bento or as the desktop.
    if (pane->screen() != tabletOutput() || !connectedForCardStage()
        || (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread)) return false;
    return stepPane(pane, delta);
}

bool Effect::stackPaneOnto(KWin::EffectWindow *pane, KWin::EffectWindow *onto)
{
    if (!pane || !onto || pane == onto || !m_desktopStage->managesWindow(pane)
        || !m_desktopStage->managesWindow(onto)) return false;
    SessionScope scope(this, m_scopedSession, stageKeyOf(onto));
    const QList<KWin::EffectWindow *> paneBehind = m_cardStage->paneStack(pane);
    const QList<KWin::EffectWindow *> ontoBehind = m_cardStage->paneStack(onto);
    KWin::EffectWindow *next = paneBehind.isEmpty() ? nullptr : paneBehind.first();
    if (!m_desktopStage->stackPaneOnto(pane, onto, next,
            [this, next] {
                return !next || m_cardStage->releaseCardToLiveBento(next);
            })) return false;
    // The pane is the Stack's front, the one it covered next, and that one's
    // Stack after it; its own Stack stays with the slot it left.
    QList<KWin::EffectWindow *> stack{onto};
    stack.append(ontoBehind);
    m_cardStage->forgetPaneStack(pane);
    m_cardStage->forgetPaneStack(onto);
    m_cardStage->notePaneStack(pane, stack);
    if (next) {
        m_cardStage->notePaneStack(next, paneBehind.mid(1));
    }
    m_cardStage->settlePaneStacks();
    observeCardOwnership();
    KWin::effects->activateWindow(pane);
    qInfo() << "Kadunce" << Revision << "stacked" << logNameOf(pane) << "on" << logNameOf(onto);
    KWin::effects->addRepaintFull();
    return true;
}

bool Effect::followStackScrub(QPointF contact)
{
    if (m_stackScrub.active()) {
        // A card closed or the pane gone mid-scrub leaves nothing to order.
        if (!m_stackScrubPane
            || int(m_cardStage->paneStack(m_stackScrubPane).size()) + 1 != m_stackScrub.size) {
            if (m_carryRuntime) m_carryRuntime->cancel();
            return true;
        }
        // Shown on the hold alone: drawn aside or down, it folds away and
        // the pane is carried as before.
        if (m_stackScrubHeldStill && !stackScrubHeldStillGoesOn()) return false;
        m_stackScrub.follow(stackScrubHand(contact.y()));
        refreshStackScrubShown();
        KWin::effects->addRepaintFull();
        return true;
    }
    if (!m_stackScrubArmed || !m_carryRuntime) return false;
    const auto held = m_carryRuntime->observer.contacts().held(m_carryRuntime->route.owner());
    if (!held || !m_stackScrubPane) { m_stackScrubArmed = false; return false; }
    const QPointF travel = held->travel;
    const auto heldMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - held->pressedAt).count();
    switch (stackScrubStart(travel.x(), travel.y(), heldMs)) {
    case StackScrubStart::Undecided: return false;
    case StackScrubStart::Carry: m_stackScrubArmed = false; return false;
    case StackScrubStart::Scrub: break;
    }
    beginStackScrub(held->pressedPosition.y());
    m_stackScrub.follow(stackScrubHand(contact.y()));
    refreshStackScrubShown();
    KWin::effects->addRepaintFull();
    return true;
}

double Effect::stackScrubHand(double pointerY) const
{
    // A pane in the top row has its title bar at the screen's edge, where the
    // pointer stops; the hand's own travel still counts.
    if (!m_carryRuntime) return pointerY;
    const auto held = m_carryRuntime->observer.contacts().held(m_carryRuntime->route.owner());
    return held ? held->pressedPosition.y() + held->travel.y() : pointerY;
}

bool Effect::stackScrubHeldStillGoesOn()
{
    const auto held = m_carryRuntime
        ? m_carryRuntime->observer.contacts().held(m_carryRuntime->route.owner()) : std::nullopt;
    if (!held) return true;
    switch (stackScrubStart(held->travel.x(), held->travel.y(), StackScrubHoldMs)) {
    case StackScrubStart::Undecided: return true;
    case StackScrubStart::Scrub: m_stackScrubHeldStill = false; return true;
    case StackScrubStart::Carry: break;
    }
    qInfo() << "Kadunce" << Revision << "folds the ladder of" << logNameOf(m_stackScrubPane)
            << "and carries it";
    endStackScrub();
    return false;
}

void Effect::beginStackScrub(double y)
{
    m_stackScrubArmed = false;
    if (!m_stackScrubPane) return;
    m_stackScrub.begin(int(m_cardStage->paneStack(m_stackScrubPane).size()) + 1, 0, y);
    // Whatever the carry aimed at before the hold was told is withdrawn.
    m_carryDestination.reset(); m_carryPreview.reset();
    m_carryCardEntryOutput.clear(); m_carryCardExitOutput.clear();
    m_carryStackOnto.clear(); m_paneStackAim.clear(); m_paneStackDwell.stop();
    if (m_carryRuntime) m_carryRuntime->handoff.withdrawDrop();
    qInfo() << "Kadunce" << Revision << "scrubs the Stack of" << logNameOf(m_stackScrubPane)
            << "of" << m_stackScrub.size;
}

void Effect::endStackScrub()
{
    const bool shown = m_stackScrub.active();
    m_stackScrub.clear();
    m_stackScrubByKeys = false;
    m_stackScrubArmed = false;
    m_stackScrubHeldStill = false;
    m_stackScrubPane.clear();
    m_stackScrubShown.clear();
    if (shown) KWin::effects->addRepaintFull();
}

void Effect::refreshStackScrubShown()
{
    // The cards behind are hidden while the layout shows; the ones on the
    // ladder are drawn there, and only there.
    QSet<KWin::EffectWindow *> ladder;
    if (m_stackScrub.active() && m_stackScrubPane) {
        const auto order = stackScrubOrder(m_stackScrubPane.data(),
            m_cardStage->paneStack(m_stackScrubPane), m_stackScrub.place);
        const int end = std::min<int>(order.size(), m_stackScrub.ladderStart() + m_stackScrub.ladderCount());
        for (int i = m_stackScrub.ladderStart(); i < end; ++i)
            if (order.at(i) && order.at(i) != m_stackScrubPane) ladder.insert(order.at(i));
    }
    for (auto it = m_stackScrubShown.begin(); it != m_stackScrubShown.end();)
        it = ladder.contains(it.key()) ? std::next(it) : m_stackScrubShown.erase(it);
    for (auto *window : std::as_const(ladder))
        if (!m_stackScrubShown.contains(window) && !window->isDeleted())
            m_stackScrubShown.insert(window, KWin::EffectWindowVisibleRef(window,
                KWin::EffectWindow::PAINT_DISABLED | KWin::EffectWindow::PAINT_DISABLED_BY_MINIMIZE
                | KWin::EffectWindow::PAINT_DISABLED_BY_DESKTOP));
}

std::optional<std::pair<QRectF, QRectF>> Effect::stackScrubRung(const KWin::EffectWindow *window) const
{
    if (!window || !m_stackScrub.active() || !m_stackScrubPane || m_stackScrubPane->isDeleted())
        return std::nullopt;
    const auto order = stackScrubOrder(m_stackScrubPane.data(),
        m_cardStage->paneStack(m_stackScrubPane), m_stackScrub.place);
    const int index = order.indexOf(const_cast<KWin::EffectWindow *>(window));
    const int rung = index - m_stackScrub.ladderStart();
    const int count = std::min<int>(m_stackScrub.ladderCount(), order.size() - m_stackScrub.ladderStart());
    if (index < 0 || rung < 0 || rung >= count) return std::nullopt;
    const QRectF pane(m_stackScrubPane->frameGeometry());
    const auto box = stackLadderRung(pane.x(), pane.y(), pane.width(), pane.height(), rung, count);
    const QRectF rect(box.x, box.y, box.width, box.height);
    // The card in front shows whole; each behind it, its title bar.
    return std::pair{rect, rung == 0 ? rect : QRectF(rect.x(), rect.y(), rect.width(), StackLadderRung)};
}

bool Effect::reorderPaneStack(KWin::EffectWindow *pane, int place)
{
    if (!pane || !m_desktopStage->managesWindow(pane)) return false;
    const QList<KWin::EffectWindow *> behind = m_cardStage->paneStack(pane);
    if (behind.isEmpty() || place <= 0) return false;
    SessionScope scope(this, m_scopedSession, stageKeyOf(pane));
    const QList<KWin::EffectWindow *> order = stackScrubOrder(pane, behind, place);
    KWin::EffectWindow *face = order.first();
    m_cardStage->forgetPaneStack(pane);
    m_cardStage->notePaneStack(face, order.mid(1));
    if (!m_desktopStage->replacePaneWithCard(pane, face,
            [this, face] {
                return m_cardStage->releaseCardToLiveBento(face);
            })) {
        m_cardStage->forgetPaneStack(face);
        m_cardStage->notePaneStack(pane, behind);
        return false;
    }
    m_cardStage->settlePaneStacks();
    observeCardOwnership();
    KWin::effects->activateWindow(face);
    qInfo() << "Kadunce" << Revision << "put" << logNameOf(pane) << "at place" << place + 1
            << "of its Stack; " << logNameOf(face) << "is in front";
    KWin::effects->addRepaintFull();
    return true;
}

double Effect::stackLeanNow() const
{
    const int duration = motion(150);
    if (!m_stackLeanClock.isValid() || duration <= 1) return m_stackLeanTo ? 1.0 : 0.0;
    const double t = std::min(1.0, double(m_stackLeanClock.elapsed()) / duration);
    const double eased = 1.0 - (1.0 - t) * (1.0 - t);
    return m_stackLeanFrom + ((m_stackLeanTo ? 1.0 : 0.0) - m_stackLeanFrom) * eased;
}

double Effect::stackLean()
{
    // §11: a window rested where it would join a Stack shrinks a little and
    // tilts, as a card does joining a deck, and straightens off it again.
    const bool to = m_carriedWindow && m_carryStackOnto;
    if (to != m_stackLeanTo) {
        m_stackLeanFrom = stackLeanNow();
        m_stackLeanTo = to;
        m_stackLeanClock.start();
    }
    const double lean = stackLeanNow();
    if (lean != (m_stackLeanTo ? 1.0 : 0.0)) KWin::effects->addRepaintFull();
    return lean;
}

QRectF Effect::leanedIntoStack(const QRectF &rect, double lean) const
{
    // Shrunk to 80% about the pointer, so what is under the hand stays there.
    if (lean <= 0.0) return rect;
    const double scale = 1.0 - 0.2 * lean;
    const QPointF hand = m_lastCarryContact;
    return QRectF(hand + (rect.topLeft() - hand) * scale, rect.size() * scale);
}

void Effect::drawStackLadderOutline(const KWin::RenderTarget &renderTarget,
    const KWin::RenderViewport &viewport, const KWin::Region &deviceRegion,
    KWin::LogicalOutput *screen)
{
    // §11, Connected: a pane holding a Stack shows nothing of it at rest. Held
    // and drawn up, the card on its ladder is outlined, so its place shows.
    if (!m_destinationShader || !screen || !m_stackScrubPane || m_stackScrubPane->screen() != screen) return;
    if (const auto rung = stackScrubRung(m_stackScrubPane))
        drawDestinationOutline(renderTarget, viewport, deviceRegion, screen, rung->first);
}

bool Effect::callPaneStackCard(KWin::EffectWindow *pane, KWin::EffectWindow *card)
{
    const QList<KWin::EffectWindow *> behind = m_cardStage->paneStack(pane);
    const int at = behind.indexOf(card);
    if (at < 0) return false;
    SessionScope scope(this, m_scopedSession, stageKeyOf(pane));
    // As if stepped to it: the cards it passed go round to the back, in turn.
    const QList<KWin::EffectWindow *> ring = behind.mid(at + 1) + QList<KWin::EffectWindow *>{pane} + behind.mid(0, at);
    m_cardStage->forgetPaneStack(pane);
    m_cardStage->notePaneStack(card, ring);
    appearInPlace(card);
    stepInUnseen(card);
    if (!m_desktopStage->replacePaneWithCard(pane, card,
            [this, card] {
                return m_cardStage->releaseCardToLiveBento(card);
            })) {
        m_cardStage->forgetPaneStack(card);
        m_cardStage->notePaneStack(pane, behind);
        return false;
    }
    m_cardStage->settlePaneStacks();
    observeCardOwnership();
    qInfo() << "Kadunce" << Revision << "brought" << logNameOf(card) << "to the front of its Stack's pane";
    KWin::effects->addRepaintFull();
    return true;
}

bool Effect::stepPane(KWin::EffectWindow *pane, int delta)
{
    if (!pane || !m_desktopStage->managesWindow(pane)) return false;
    SessionScope scope(this, m_scopedSession, stageKeyOf(pane));
    const QList<KWin::EffectWindow *> behind = m_cardStage->paneStack(pane);
    if (behind.isEmpty()) return false;
    KWin::EffectWindow *next = delta > 0 ? behind.first() : behind.last();
    // The ring after the step: going forward the old front goes to the back,
    // going back it returns to the front.
    QList<KWin::EffectWindow *> ring = behind;
    ring.removeAll(next);
    if (delta > 0) ring.append(pane);
    else ring.prepend(pane);
    m_cardStage->forgetPaneStack(pane);
    m_cardStage->notePaneStack(next, ring);
    appearInPlace(next);
    stepInUnseen(next);
    if (!m_desktopStage->replacePaneWithCard(pane, next,
            [this, next] {
                return m_cardStage->releaseCardToLiveBento(next);
            })) {
        m_cardStage->forgetPaneStack(next);
        m_cardStage->notePaneStack(pane, behind);
        return false;
    }
    m_cardStage->settlePaneStacks();
    observeCardOwnership();
    qInfo() << "Kadunce" << Revision << "stepped the Stack of" << logNameOf(pane)
            << (delta > 0 ? "forward;" : "back;") << logNameOf(next) << "is in front";
    activateShownPane(next);
    return true;
}

bool Effect::restoreLeftLayout(KWin::EffectWindow *front, const QList<KWin::EffectWindow *> &behind)
{
    // §11, Connected: a pane carried away from Spread's display by the keys
    // and carried back takes the side it left, beside what it left beside.
    // Stepped while it was away, its Stack comes back with another card in
    // front, which takes that place.
    // Only a display it can take back its side on uses the record up; on its
    // way through another display it keeps it.
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet || front->screen() != tablet || !connectedForCardStage()
        || m_desktopStage->managesWindow(front) || m_cardStage->liveCardIndex(front) < 0) return false;
    std::optional<LeftLayout> found;
    for (auto *member : QList<KWin::EffectWindow *>{front} + behind) {
        const auto record = m_leftLayouts.constFind(member);
        if (record == m_leftLayouts.cend()) continue;
        if (!found) found = record.value();
        m_leftLayouts.erase(record);
    }
    if (!found) return false;
    const LeftLayout left = *found;
    const BentoSidePlacement side{left.right, true, true};
    bool placed = false;
    if (m_desktopStage->hasSessionOnOutput(tablet->name())) {
        placed = m_desktopStage->admitCardToLiveBento(front,
            [this, front] { return m_cardStage->releaseCardToLiveBento(front); });
    } else {
        KWin::EffectWindow *partner = nullptr;
        for (const auto &other : left.partners)
            if (other && !other->isDeleted() && other.data() != front && !other->isMinimized()
                && other->screen() == tablet && m_cardStage->liveCardIndex(other) >= 0) {
                partner = other.data();
                break;
            }
        (void)m_cardStage->promoteToActive(front);
        placed = partner ? pairCardsIntoBento(tablet, front, partner, side)
                         : placeActiveCardAlone(tablet, side);
    }
    qInfo() << "Kadunce" << Revision << (placed ? "gave" : "could not give") << logNameOf(front)
            << "back the" << (left.right ? "right" : "left") << "pane it left";
    return placed;
}

void Effect::rejoinStack(KWin::EffectWindow *front, const QList<KWin::EffectWindow *> &behind)
{
    // §11, Every display: a Stack that followed its front card to this
    // display is still one Stack, whether that card arrives as a card or as
    // a pane. Members another display's stage still holds leave it for this one.
    if (!front) return;
    QList<QPointer<KWin::EffectWindow>> members;
    for (auto *member : behind) members.append(member);
    onThisStageLater(0, [this, front = QPointer(front), members] {
        if (!front || front->isDeleted() || m_releasing) return;
        QList<KWin::EffectWindow *> alive;
        for (const auto &member : members) {
            if (!member || member->isDeleted() || member == front) continue;
            CardStageController *from = stageHolding(member);
            if (from && from != m_cardStage) {
                const auto restore = from->managedRestore(member);
                const QPointer<KWin::EffectWindow> follower = member;
                if (!m_cardStage->admitCardFromOtherDisplay(member,
                        [from, follower] { return follower && from->releaseCardToStage(follower); },
                        restore ? &*restore : nullptr)) continue;
            }
            alive.append(member.data());
        }
        for (const auto &display : m_scopedSession->displays)
            if (display.second->cards.get() != m_cardStage) display.second->cards->forgetPaneStack(front);
        (void)restoreLeftLayout(front, alive);
        QList<QPointer<KWin::EffectWindow>> kept;
        for (auto *member : std::as_const(alive)) kept.append(member);
        if (!kept.isEmpty()) keepStackBehind(front, kept, 10);
    });
}

void Effect::keepStackBehind(KWin::EffectWindow *front, const QList<QPointer<KWin::EffectWindow>> &behind, int tries)
{
    // §11, Every display: the cards that came with `front` wait behind it
    // here, behind its pane if a layout took it, or in its Stack in the row.
    // A layout takes it in its own time, and until then it is neither, so it
    // is looked at again.
    if (!front || front->isDeleted() || m_releasing) return;
    QList<KWin::EffectWindow *> alive;
    for (const auto &member : behind)
        if (member && !member->isDeleted() && member != front && m_cardStage->liveCardIndex(member) >= 0)
            alive.append(member.data());
    if (alive.isEmpty()) return;
    if (m_desktopStage->managesWindow(front)) {
        m_cardStage->notePaneStack(front, alive);
        m_cardStage->settlePaneStacks();
    } else if (m_cardStage->liveCardIndex(front) < 0) {
        if (tries > 0) {
            onThisStageLater(50, [this, front = QPointer(front), behind, tries] {
                if (front) keepStackBehind(front, behind, tries - 1);
            });
            return;
        }
        qInfo() << "Kadunce" << Revision << "could not stack" << alive.size()
                << "cards behind" << logNameOf(front) << "again: it is neither a card nor a pane here";
        return;
    } else if (!m_cardStage->stackBehind(front, alive)) {
        qInfo() << "Kadunce" << Revision << "could not stack" << alive.size()
                << "cards behind" << logNameOf(front) << "again";
        return;
    }
    qInfo() << "Kadunce" << Revision << "brought" << logNameOf(front) << "back with"
            << alive.size() << "behind it";
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
}

void Effect::activateShownPane(KWin::EffectWindow *pane)
{
    // The card that now shows has the focus, so the next key reaches it. It
    // may only be shown the turn after, so the focus is asked for again then.
    KWin::effects->activateWindow(pane);
    const QPointer<KWin::EffectWindow> shown = pane;
    QTimer::singleShot(0, this, [shown] {
        if (shown && !shown->isDeleted() && KWin::effects->activeWindow() != shown)
            KWin::effects->activateWindow(shown);
    });
}

void Effect::prePaintScreen(KWin::ScreenPrePaintData &data)
{
    // §11, Every display: each display is framed for its own cards.
    SessionScope scope(this, m_currentSession,
        isCardDisplay(data.screen) ? stageKeyFor(data.screen) : m_scopedKey);
    if (m_tablePresenter && m_tablePresenter->visible() && data.screen == m_tablePresenterOutput)
        m_tablePresenter->prePaint(data.frame);
    // The offer lasts while the room it fills stays open as it was.
    if (m_roomOffer && m_roomOffer->output == data.screen) {
        // §11, Solo: beside the Active card, while it stays Active.
        const auto room = m_roomOffer->pairSide ? std::optional<QRectF>(pairOfferRoom(data.screen, *m_roomOffer->pairSide))
                                                : m_desktopStage->openRoomOn(data.screen);
        bool stands = room && *room == m_roomOffer->room
            && (m_roomOffer->pairSide
                ? m_roomOffer->pairWith && m_cardStage->activeCardIdentity() == m_roomOffer->pairWith
                    && m_cardStage->presentation() == CardPresentation::Active
                : m_cardStage->presentsBento());
        for (const auto &card : std::as_const(m_roomOffer->cards))
            stands = stands && card && !card->isDeleted();
        if (!stands) QTimer::singleShot(0, this, [this] {
            if (!m_roomOffer) return;
            qInfo() << "Kadunce" << Revision << "leaves the offer: the room it filled changed";
            leaveRoomOffer();
        });
    }
    m_framingTablet = isTabletOutput(data.screen);
    if (m_framingTablet) noteTabletFrameStart();
    // A released row, a returning card or a thrown one moves once per frame.
    if (isTabletOutput(data.screen)) m_cardStage->advanceMotion();
    // Notes and Search follow the Spread of the display in use, the one a
    // person reaches for last.
    if (isTabletOutput(data.screen) && isInUseDisplay(data.screen)) {
        const bool wasShown = std::exchange(m_notesSpreadShown, m_cardStage->isActive()
            && m_cardStage->presentation() == CardPresentation::Spread);
        if (!m_notesSpreadShown) m_stuckNotes.cancel();
        // Spread opening reads every entry again, so a missed signal cannot
        // leave a card's fan disagreeing with its window.
        if (m_notesSpreadShown && !wasShown && m_stuckNotesWatcher) m_stuckNotesWatcher->refresh();
        if (m_notesSpreadShown && !wasShown) m_notesAskedFor = {};
        syncNotesHandover(wasShown && !m_notesSpreadShown);
        syncNotesPause();
        syncSpokenLine();
        // Every way Spread opens ends in a frame of it formed; a swipe still
        // under the fingers may yet spring back.
        const bool spreadOpen = m_notesSpreadShown && !m_cardStage->spreadOpening();
        if (spreadOpen && !std::exchange(m_spreadOpenForLauncher, true)) inviteFloatingLauncher();
        if (!spreadOpen) m_spreadOpenForLauncher = false;
    }
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
    if (isInUseDisplay(data.screen) || !m_cardsOnEveryDisplay) prepareNeighbors(data.screen);
    m_continueRepaint = false;

    // Each frame looks again for Gooseberry's surface over the Active card.
    if (m_notesHandover) m_continueRepaint = true;
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
        else {
            // A settle on the card display moves once its client draws the
            // shape it settles into or has had long enough to.
            if (!m_dropSettleTimer.isValid() && !m_settleDrawnMoving) {
                const auto frame = m_settlingWindow->frameGeometry();
                m_settleDrawnMoving = !paneArrivalWaits(
                    paneArrivalShaped(frame.width(), frame.height(), m_settleTo.width(), m_settleTo.height()),
                    m_settleAsked.elapsed());
            }
            data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS; m_continueRepaint = true;
        }
    }
    for (auto it = m_yieldingPanes.begin(); it != m_yieldingPanes.end();) {
        if (!yieldingPaneOpacity(it->window)) {
            if (it->window && !it->window->isDeleted()) unredirect(it->window);
            it = m_yieldingPanes.erase(it);
        } else {
            data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
            m_continueRepaint = true;
            ++it;
        }
    }
    if (m_carriedWindow) data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    if (m_gestureNote) {
        if (!m_gestureNote->output
            || m_gestureNote->shown.elapsed() >= GestureNoteTiming::TotalMs) {
            KWin::effects->addRepaint(KWin::RectF(m_gestureNote->box).toAlignedRect());
            m_gestureNote.reset();
        } else {
            m_continueRepaint = true;
        }
    }
    if (m_cardStage->isActive()
        && (isTabletOutput(data.screen) || m_cardStage->cardGrabActive())) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        if (m_cardStage->animationsRunning()) {
            m_continueRepaint = true;
        }
    }
    if (data.screen) {
        const auto whole = [](const QRectF &rect) { return KWin::Rect(rect.toAlignedRect()); };
        // Glass drawn last frame, and any about to be, is drawn afresh whole,
        // so the blur only ever reads what lies under it.
        for (const QRectF &rect : m_frostDrawn.value(data.screen->name())) data.paint |= whole(rect);
        if (const auto box = heldDestinationOn(data.screen)) data.paint |= whole(*box);
        if (m_placementPreview && m_placementPreviewOutput == data.screen)
            data.paint |= whole(QRectF(*m_placementPreview));
        // The keyboard's window, outlined: where the outline leaves and
        // arrives repaints, and it fades in over the next frames.
        KWin::EffectWindow *outlined = keyboardOutlineWindow(data.screen);
        auto &outline = m_keyboardOutlines[data.screen->name()];
        if (outline.window != outlined) {
            if (outline.window && !outline.window->isDeleted())
                data.paint |= whole(QRectF(outline.window->frameGeometry()));
            outline.window = outlined;
            outline.since.start();
        }
        if (outlined && outline.since.elapsed()
                < motionDuration(KeyboardOutlineFadeMs, KWin::effects->animationTimeFactor())) {
            data.paint |= whole(QRectF(outlined->frameGeometry()));
            m_keyboardOutlineFading.append(QRectF(outlined->frameGeometry()).toAlignedRect());
        }
    }
    KWin::effects->prePaintScreen(data);
}

void Effect::prepareNeighbors(KWin::LogicalOutput *screen)
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
    if (isTabletOutput(screen)) m_neighborPreparedThisFrame = false;
}

void Effect::postPaintScreen()
{
    // KWin consumes current layer damage after prePaintScreen. Request the
    // NEXT frame only after paint. Latch pre-paint activity so an animation
    // expiring during this frame still gets one exact final-state frame.
    const bool continueRepaint = m_continueRepaint;
    m_continueRepaint = false;
    for (const QRect &rect : std::exchange(m_keyboardOutlineFading, {})) KWin::effects->addRepaint(KWin::Rect(rect));
    for (auto &m : m_bentoMotions)
        if (m.drawnMoving && !m.timer.isValid()) m.timer.start();
    if (m_settlingWindow && m_settleDrawnMoving && !m_dropSettleTimer.isValid()) m_dropSettleTimer.start();
    KWin::effects->postPaintScreen();
    if (std::exchange(m_framingTablet, false)) noteTabletFrameEnd(continueRepaint);
    // Spread, Bento and the rest open and close along many paths, and every
    // one ends in a frame, so the context is announced here whenever the
    // presentation it reports changes: the dock's Spread button and the
    // Context Bar follow at once.
    QString presentation;
    eachCardDisplay([&](KWin::LogicalOutput *output) {
        presentation += output->name() + QLatin1Char(' ')
            + QString::number(m_cardStage->isActive() ? int(m_cardStage->presentation()) : -1) + QLatin1Char(' ');
    });
    // What a context line says changes the same way, while one is claimed.
    const QString line = m_contextLineClaims.isEmpty() ? QString() : contextLineSignature();
    const bool lineChanged = line != std::exchange(m_reportedContextLine, line);
    if (presentation != std::exchange(m_reportedPresentation, presentation) || lineChanged)
        Q_EMIT workspaceContextChanged();
    if (continueRepaint) {
        KWin::effects->addRepaintFull();
    }
    // A kept picture is looked at again each frame until it is let go.
    if (!m_heldPictures.empty()) KWin::effects->addRepaintFull();
}

void Effect::noteTabletFrameStart()
{
    if (!m_frameClock.isValid()) m_frameClock.start();
    const qint64 now = m_frameClock.elapsed();
    const qint64 gap = m_frameStartedAt < 0 ? 0 : now - m_frameStartedAt;
    // A frame is due about every 16 ms while motion runs; a gap of three is
    // one a person sees. Only gaps after a frame that asked for the next count.
    if (m_frameWanted && gap >= FrameStallMs) {
        qInfo() << "Kadunce" << Revision << "frame stall of" << gap << "ms on the card display:"
                << "the frame before spent" << m_frameWork << "ms painting and made"
                << m_framePreviewsMade << "new previews;"
                << (!m_cardStage->isActive() ? "cards off"
                    : m_cardStage->presentation() == CardPresentation::Spread ? "Spread"
                    : m_cardStage->presentation() == CardPresentation::Bento ? "Bento"
                    : m_cardStage->presentation() == CardPresentation::Desktop ? "desktop" : "Active")
                << (m_cardStage->cardGrabActive() ? "with a card held" : "")
                << (m_carriedWindow ? "with a window carried" : "");
    }
    m_frameStartedAt = now;
    m_framePreviewsMade = 0;
}

void Effect::noteTabletFrameEnd(bool wanted)
{
    if (m_frameStartedAt >= 0) m_frameWork = m_frameClock.elapsed() - m_frameStartedAt;
    m_frameWanted = wanted;
}

// CARD-LIFECYCLE.md §2: a sleeping card is chosen in Spread to wake it, so
// Spread keeps it drawn, dimmed, from the last frame its window showed. KWin
// paints no minimized window unless an effect holds it visible.
void Effect::showSleepingCardsInSpread()
{
    // Every display's Spread keeps its own sleeping cards drawn.
    QSet<KWin::EffectWindow *> sleeping;
    eachCardDisplay([&](KWin::LogicalOutput *) {
        if (!m_cardStage->isActive() || m_cardStage->presentation() != CardPresentation::Spread) return;
        for (const auto &card : m_cardStage->liveCards())
            if (card && !card->isDeleted() && card->isMinimized()) sleeping.insert(card.data());
    });
    for (auto it = m_sleepingShown.begin(); it != m_sleepingShown.end();) {
        if (sleeping.contains(it.key())) ++it;
        else it = m_sleepingShown.erase(it);
    }
    for (KWin::EffectWindow *window : std::as_const(sleeping))
        if (!m_sleepingShown.contains(window))
            m_sleepingShown.insert(window, KWin::EffectWindowVisibleRef(window, KWin::EffectWindow::PAINT_DISABLED_BY_MINIMIZE));
}

void Effect::prePaintWindow(KWin::RenderView *view,
                            KWin::EffectWindow *window,
                            KWin::WindowPrePaintData &data)
{
    // Asked of the stage holding the window, else its display's.
    SessionScope scope(this, m_currentSession, stageKeyOf(window));
    if (window == m_carriedWindow || (window == m_settlingWindow && dropSettleRect()) || bentoMotionRect(window)
        || yieldingPaneOpacity(window) || m_stackScrubShown.contains(window)
        || m_roomOfferShown.contains(window)) {
        data.setTransformed(); data.setTranslucent();
    }
    // Left undrawn, so it must not hide what is under it from the scene.
    if (m_previewDesktop ? hiddenByDesktopPreview(window) : hiddenOnOtherDesktop(window)) data.setTranslucent();
    // Left undrawn while Spread draws its notes on the card.
    if (notesSurfaceStepsAside(window)) data.setTranslucent();
    if (steppingInUnseen(window)) data.setTranslucent();
    // Drawn with its card, not where it stands, so it hides nothing there.
    if (m_drawnDialogs.contains(window) || drawnWithCarriedCard(window)) data.setTranslucent();
    // Its trimmed corners show what is under them, so that must be drawn.
    if (trimsCorners(window)) data.setTranslucent();
    // A card a display holds but does not show, such as one waiting behind
    // the Active card or a layout in its place, is left undrawn, so it must
    // not hide what is under it either: that is what those corners show.
    if (m_cardStage->isActive() && window != m_nativeCarry && isCardWindow(window)
        && !m_desktopStage->managesWindow(window)
        && m_cardStage->presentation() != CardPresentation::Desktop
        && m_cardStage->paintSlot(window) == 99) data.setTranslucent();
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

void Effect::askPairOffer(KWin::LogicalOutput *output, KWin::EffectWindow *card, BentoSidePlacement side)
{
    const int ask = ++m_roomOfferAsk;
    if (m_pairOfferAsked && m_pairOfferAsked->card != card) releasePairOfferPlace(m_pairOfferAsked->card);
    m_pairOfferAsked = PairOfferAsked{card, side};
    const QPointer<KWin::LogicalOutput> landed = output;
    QTimer::singleShot(motion(320), this, [this, landed, ask] {
        if (landed) offerOpenRoom(landed, ask);
    });
}

QList<KWin::EffectWindow *> Effect::sleepingCardsOn(const KWin::LogicalOutput *output) const
{
    QList<KWin::EffectWindow *> sleeping;
    for (const auto &card : m_cardStage->liveCards()) {
        if (!card || card->isDeleted() || !card->isMinimized() || card->screen() != output
            || m_desktopStage->managesWindow(card) || m_cardStage->paneHolding(card)) continue;
        const auto stack = m_cardStage->stackFrontToBack(card);
        if (!stack.isEmpty() && stack.first() != card) continue;
        sleeping.append(card.data());
    }
    return sleeping;
}

QRectF Effect::pairOfferRoom(KWin::LogicalOutput *output, const BentoSidePlacement &side) const
{
    // The share the snapped card leaves, beside the share it names: two
    // thirds from the upper half of the edge, one third from the lower, as a
    // pair on Solo stands (bentoProportionalSideLayout).
    const QRectF whole(m_cardStage->activeTarget(output));
    // The Visual sheet's gutter between two panes.
    const double gap = 10.0;
    const double named = (whole.width() - gap) * (side.large ? 2.0 / 3.0 : 1.0 / 3.0);
    const double room = whole.width() - gap - named;
    return QRectF(side.right ? whole.x() : whole.x() + named + gap, whole.y(), room, whole.height());
}

void Effect::releasePairOfferPlace(KWin::EffectWindow *card, bool refit)
{
    if (!card || card->isDeleted()) return;
    SessionScope scope(this, m_currentSession, stageKeyFor(card->screen()));
    if (!m_cardStage->holdActivePlace(card, std::nullopt) || !refit) return;
    // Moved after the frame that let the offer go, never during its paint.
    const QPointer<KWin::EffectWindow> held = card;
    QTimer::singleShot(0, this, [this, held] {
        if (!held || held->isDeleted()) return;
        SessionScope scope(this, m_currentSession, stageKeyFor(held->screen()));
        (void)m_cardStage->refitActive(held);
    });
}

void Effect::askRoomOffer(KWin::LogicalOutput *output)
{
    const int ask = ++m_roomOfferAsk;
    if (m_pairOfferAsked) releasePairOfferPlace(m_pairOfferAsked->card);
    m_pairOfferAsked.reset();
    if (!output || !connectedOutputForDesktopStage(output)) return;
    const QPointer<KWin::LogicalOutput> landed = output;
    QTimer::singleShot(motion(320), this, [this, landed, ask] {
        if (landed) offerOpenRoom(landed, ask);
    });
}

void Effect::offerOpenRoom(KWin::LogicalOutput *output, int ask, int tries)
{
    if (ask != m_roomOfferAsk) return;
    const auto pair = m_pairOfferAsked;
    // The card a new pair offer stands beside keeps the place it now holds.
    if (pair && m_roomOffer && m_roomOffer->pairWith == pair->card) m_roomOffer->pairSide.reset();
    leaveRoomOffer();
    // A pair offer that does not open gives the card its own place back.
    const auto giveUp = [this, pair] {
        if (pair) releasePairOfferPlace(pair->card);
    };
    if (!output || m_releasing || !KWin::effects->screens().contains(output)) {
        giveUp();
        return;
    }
    SessionScope scope(this, m_currentSession, stageKeyFor(output));
    const bool settled = !m_carriedWindow && (pair
        ? pair->card && m_cardStage->activeCardIdentity() == pair->card
            && m_cardStage->presentation() == CardPresentation::Active
        : connectedOutputForDesktopStage(output) && m_cardStage->presentsBento());
    const auto room = !settled ? std::nullopt
        : pair ? std::optional<QRectF>(pairOfferRoom(output, pair->side)) : m_desktopStage->openRoomOn(output);
    if (!room) {
        // A layout the snap began shows once its card has glided and grown
        // into place.
        const QPointer<KWin::LogicalOutput> later = output;
        if (tries > 0)
            QTimer::singleShot(60, this, [this, later, ask, tries] {
                if (later) offerOpenRoom(later, ask, tries - 1);
            });
        else {
            qInfo() << "Kadunce" << Revision << "found no room open on" << output->name() << "to offer;"
                    << "carrying" << bool(m_carriedWindow) << "connected" << connectedOutputForDesktopStage(output)
                    << "Bento" << m_cardStage->presentsBento();
            giveUp();
        }
        return;
    }
    // The fronts of the display's other cards, in Spread's order, asleep ones
    // too; a card waiting behind a pane stays there.
    RoomOfferShown offer{output, *room, {}, {}, -1, {}, {}};
    if (pair) {
        offer.pairWith = pair->card;
        offer.pairSide = pair->side;
    }
    QList<QSizeF> sizes;
    for (const auto &card : m_cardStage->liveCards()) {
        if (!card || card->isDeleted() || card->screen() != output || card == offer.pairWith
            || m_desktopStage->managesWindow(card) || m_cardStage->paneHolding(card)) continue;
        // Beside the Active card on Solo, only sleeping cards: an awake one
        // would have been its partner.
        if (pair && !card->isMinimized()) continue;
        const auto stack = m_cardStage->stackFrontToBack(card);
        if (!stack.isEmpty() && stack.first() != card) continue;
        offer.cards.append(card);
        sizes.append(QRectF(card->frameGeometry()).size());
    }
    offer.tiles = roomOfferTiles(*room, sizes);
    offer.cards = offer.cards.mid(0, offer.tiles.size());
    if (offer.tiles.isEmpty()) {
        giveUp();
        return;
    }
    for (const auto &card : std::as_const(offer.cards))
        m_roomOfferShown.insert(card.data(), KWin::EffectWindowVisibleRef(card.data(),
            KWin::EffectWindow::PAINT_DISABLED | KWin::EffectWindow::PAINT_DISABLED_BY_MINIMIZE
            | KWin::EffectWindow::PAINT_DISABLED_BY_DESKTOP));
    qInfo() << "Kadunce" << Revision << "offers" << offer.cards.size() << "cards in the room open on"
            << output->name();
    m_roomOffer = std::move(offer);
    m_roomOfferInput->active = true;
    KWin::effects->addRepaintFull();
    Q_EMIT workspaceContextChanged();
}

QString Effect::roomOfferHint() const
{
    return tr("Pick one for this side \u00b7 Esc leaves it open");
}

void Effect::leaveRoomOffer(bool restore)
{
    if (m_roomOfferInput) m_roomOfferInput->active = false;
    if (!m_roomOffer) return;
    QPointer<KWin::EffectWindow> beside;
    if (m_roomOffer->pairSide) beside = m_roomOffer->pairWith;
    m_roomOffer.reset();
    if (beside) releasePairOfferPlace(beside, restore);
    m_roomOfferShown.clear();
    m_offerPictures.clear();
    KWin::effects->addRepaintFull();
    Q_EMIT workspaceContextChanged();
}

int Effect::roomOfferPictureAt(QPointF position) const
{
    if (!m_roomOffer || !m_roomOffer->output) return -2;
    for (int i = 0; i < m_roomOffer->tiles.size(); ++i)
        if (m_roomOffer->tiles.at(i).adjusted(0, 0, 0, RoomOfferNameRow).contains(position)) return i;
    return m_roomOffer->room.contains(position) ? -1 : -2;
}

void Effect::pickFromRoomOffer(int index)
{
    if (!m_roomOffer || index < 0 || index >= m_roomOffer->cards.size()) return;
    const QPointer<KWin::EffectWindow> card = m_roomOffer->cards.at(index);
    const QPointer<KWin::LogicalOutput> output = m_roomOffer->output;
    const QPointer<KWin::EffectWindow> pairWith = m_roomOffer->pairWith;
    const auto pairSide = m_roomOffer->pairSide;
    leaveRoomOffer(!pairSide);
    if (pairSide) {
        // §11, Solo: the card picked wakes and pairs with the Active card,
        // which keeps the side it was snapped to.
        QTimer::singleShot(0, this, [this, card, output, pairWith, pairSide] {
            if (!card || card->isDeleted() || !output || !pairWith || pairWith->isDeleted()) return;
            SessionScope scope(this, m_currentSession, stageKeyFor(output));
            if (m_cardStage->activeCardIdentity() != pairWith || m_cardStage->liveCardIndex(card) < 0) {
                qInfo() << "Kadunce" << Revision << "could not pair" << logNameOf(pairWith) << "with" << logNameOf(card)
                        << "picked beside it: it is no longer Active, or the pick is no card";
                (void)m_cardStage->refitActive(pairWith);
                return;
            }
            if (card->isMinimized()) card->unminimize();
            if (!card || card->isMinimized() || !pairCardsIntoBento(output, pairWith, card, *pairSide)) {
                qInfo() << "Kadunce" << Revision << "could not pair" << logNameOf(pairWith) << "with"
                        << (card ? logNameOf(card) : QStringLiteral("a closed card"));
                (void)m_cardStage->refitActive(pairWith);
                return;
            }
            qInfo() << "Kadunce" << Revision << "paired" << logNameOf(pairWith) << "with" << logNameOf(card)
                    << "picked beside it";
            observeCardOwnership();
            Q_EMIT workspaceContextChanged();
        });
        return;
    }
    // After the press that picked it, as a pick in Spread is: the card fills
    // the room, its Stack waiting behind it.
    QTimer::singleShot(0, this, [this, card, output] {
        if (!card || card->isDeleted() || !output) return;
        SessionScope scope(this, m_currentSession, stageKeyFor(output));
        if (!m_cardStage->presentsBento() || m_cardStage->liveCardIndex(card) < 0
            || !m_desktopStage->hasSessionOnOutput(output->name())) return;
        // A card picked asleep wakes into the room.
        if (card->isMinimized()) card->unminimize();
        if (!card) return;
        const QList<KWin::EffectWindow *> behind = m_cardStage->stackFrontToBack(card).mid(1);
        if (!m_desktopStage->admitCardToLiveBento(card,
                [this, card] { return card && m_cardStage->releaseCardToLiveBento(card); }, true)) return;
        if (!behind.isEmpty()) m_cardStage->notePaneStack(card, behind);
        m_cardStage->settlePaneStacks();
        KWin::effects->activateWindow(card);
        qInfo() << "Kadunce" << Revision << "filled the open room with" << logNameOf(card);
        observeCardOwnership();
        Q_EMIT workspaceContextChanged();
    });
}

void Effect::drawRoomOffer(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
    const KWin::Region &deviceRegion, KWin::LogicalOutput *screen, QList<QRectF> &overlaid)
{
    if (!m_roomOffer || !screen || m_roomOffer->output != screen) return;
    const QRectF room = m_roomOffer->room;
    // The pictures stand on the desktop itself, with no box behind them.
    QRegion dirtied;
    for (const auto &dirty : deviceRegion.rects())
        dirtied += QRectF(viewport.mapFromDeviceCoordinates(KWin::RectF(dirty))).toAlignedRect();
    // The picture under the pointer takes the selected edge.
    const int hovered = m_roomOffer->hovered;
    if (hovered >= 0 && hovered < m_roomOffer->tiles.size() && hovered < m_roomOffer->cards.size()
        && m_roomOffer->cards.at(hovered)) {
        const QRectF picture = offerPicture(m_roomOffer->tiles.at(hovered),
            QRectF(m_roomOffer->cards.at(hovered)->frameGeometry()).size());
        QList<QRectF> edge;
        for (const QRect &part : dirtied.intersected(picture.toAlignedRect())) edge.append(QRectF(part));
        fillDestinationParts(renderTarget, viewport, edge, picture, 0.f, 0.24f);
    }
    double bottom = room.top();
    for (int i = 0; i < m_roomOffer->tiles.size() && i < m_roomOffer->cards.size(); ++i) {
        KWin::EffectWindow *card = m_roomOffer->cards.at(i);
        if (!card) continue;
        const QRectF tile = m_roomOffer->tiles.at(i);
        m_cardLabelRenderer.render(renderTarget, viewport, tile,
            {spreadCardName(card, m_cardStage->liveCardIndex(card) + 1), QString()});
        overlaid.append(CardLabelRenderer::labelRect(tile));
        bottom = std::max(bottom, tile.bottom() + RoomOfferNameRow);
    }
    // How to choose, and how not to, under the pictures.
    // A label stands under the rect it names, so the rect is one pixel tall
    // and ends where the pictures' names do.
    // A display whose Context Panel speaks for it says it there instead.
    if (contextLineClaimed(screen)) return;
    const QRectF line(room.x() + RoomOfferGap, bottom - 1.0, room.width() - 2 * RoomOfferGap, 1.0);
    if (CardLabelRenderer::labelRect(line).bottom() <= room.bottom())
        m_cardLabelRenderer.render(renderTarget, viewport, line, {roomOfferHint(), QString()});
}

std::optional<QRectF> Effect::heldDestinationOn(KWin::LogicalOutput *screen)
{
    const auto &reservation = m_carriedWindow ? m_carryDestination : m_lineDestination;
    const auto &preview = m_carriedWindow ? m_carryPreview : m_linePreview;
    const auto cardEntry = m_carriedWindow ? m_carryCardEntryOutput : m_lineCardEntryOutput;
    const auto cardExit = m_carriedWindow ? m_carryCardExitOutput : QPointer<KWin::LogicalOutput>();
    // A window leaning into a Stack is its own preview; no box is drawn.
    if (!m_destinationShader || !screen || !preview || (m_carriedWindow && m_carryStackOnto)
        || !(m_carriedWindow || m_cardStage->cardGrabActive())) return std::nullopt;
    if (cardEntry == screen || (cardExit && cardExit == screen)
        || (reservation && !cardEntry && reservation->showsPlacementOutline()
            && reservation->destinationOutput() == screen
            && m_desktopStage->cardDropValid(*reservation)))
        return QRectF(*preview);
    return std::nullopt;
}

void Effect::drawHeldBoxUnder(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport)
{
    // The box sits under the window in hand, so the glass frosts the windows
    // and wallpaper beneath it and never the window itself.
    KWin::LogicalOutput *screen = m_paintingOutput;
    if (!screen || m_heldBoxDrawnOn == screen) return;
    if (const auto box = heldDestinationOn(screen)) {
        drawDestinationOutline(renderTarget, viewport, m_paintingRegion, screen, *box);
        m_heldBoxDrawnOn = screen;
    }
}

// The paint path's own test: a window Card Stage does not draw as a card.
bool Effect::paintedAsItStands(KWin::EffectWindow *window)
{
    return !m_cardStage->isActive() || !(isCardWindow(window) || m_sleepingShown.contains(window))
        || m_desktopStage->managesWindow(window)
        || (m_cardStage->presentation() == CardPresentation::Desktop
            && m_cardStage->liveCardIndex(window) < 0);
}

KWin::EffectWindow *Effect::keyboardOutlineWindow(KWin::LogicalOutput *screen)
{
    if (!m_showsKeyboardOutline || !m_keyboardOutlineShader || !m_keyboardEdgeShader || !screen || m_previewDesktop)
        return nullptr;
    // Nothing is outlined while a window is in hand, or while Spread, Table or
    // an open room's offer stands on the display.
    if (m_carriedWindow || m_cardStage->cardGrabActive()) return nullptr;
    if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread) return nullptr;
    if (m_tablePresenter && m_tablePresenter->visible() && m_tablePresenterOutput == screen) return nullptr;
    if (m_roomOffer && m_roomOffer->output == screen) return nullptr;
    KWin::EffectWindow *active = KWin::effects->activeWindow();
    if (!active || active->isDeleted() || active->isMinimized() || active->isFullScreen()
        || !(active->isNormalWindow() || active->isDialog()) || active->screen() != screen
        || !active->isOnCurrentDesktop() || !paintedAsItStands(active)) return nullptr;
    // A lone window needs no marker: it is outlined only while another shows
    // beside it on the display.
    const QRectF frame(active->frameGeometry());
    const auto order = KWin::effects->stackingOrder();
    for (KWin::EffectWindow *other : order) {
        if (!other || other == active || other->isDeleted() || other->isMinimized()
            || !(other->isNormalWindow() || other->isDialog()) || other->screen() != screen
            || !other->isOnCurrentDesktop() || !paintedAsItStands(other)) continue;
        // A dialog and the window it belongs to are one thing.
        if (other->mainWindows().contains(active) || active->mainWindows().contains(other)) continue;
        if (frame.contains(QRectF(other->frameGeometry()))) continue;
        return active;
    }
    return nullptr;
}

float Effect::keyboardOutlineOpacity(KWin::EffectWindow *window, const KWin::WindowPaintData &data) const
{
    if (!m_paintingOutput || window->screen() != m_paintingOutput) return 0.0F;
    const auto found = m_keyboardOutlines.constFind(m_paintingOutput->name());
    if (found == m_keyboardOutlines.cend() || found->window != window) return 0.0F;
    const int duration = motionDuration(KeyboardOutlineFadeMs, KWin::effects->animationTimeFactor());
    const double progress = std::clamp(double(found->since.elapsed()) / duration, 0.0, 1.0);
    return float((1.0 - std::pow(1.0 - progress, 3.0)) * data.opacity());
}

QColor Effect::keyboardOutlineInk()
{
    // The accent, or the scheme's text where the accent would vanish into its
    // ground.
    const SurfaceTone &tone = SurfaceTone::current();
    const auto luma = [](const QColor &c) { return 0.299 * c.redF() + 0.587 * c.greenF() + 0.114 * c.blueF(); };
    return std::abs(luma(tone.accent) - luma(tone.ground)) < 0.2 ? tone.text : tone.accent;
}

void Effect::paintKeyboardOutline(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
    KWin::EffectWindow *window, const KWin::Region &deviceRegion, const KWin::WindowPaintData &data)
{
    // A window that keeps its own corners has its outline drawn with its
    // picture (drawWindow); one Kadunce trims gets it here, on the trim.
    if (!m_keyboardOutlineShader || !trimsCorners(window)) return;
    const float opacity = keyboardOutlineOpacity(window, data);
    if (opacity <= 0.0F) return;
    const QRectF frame(window->frameGeometry());
    const QRectF box(frame.x() + data.xTranslation(), frame.y() + data.yTranslation(),
                     frame.width() * data.xScale(), frame.height() * data.yScale());
    if (box.isEmpty()) return;
    QList<QVector2D> vertices;
    for (const auto &dirty : deviceRegion.rects()) {
        const auto part = QRectF(viewport.mapFromDeviceCoordinates(KWin::RectF(dirty))).intersected(box);
        if (part.isEmpty()) continue;
        vertices << QVector2D(part.topLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomLeft())
                 << QVector2D(part.bottomLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomRight());
    }
    if (vertices.isEmpty()) return;
    const QColor ink = keyboardOutlineInk();
    KWin::ShaderBinder binder(m_keyboardOutlineShader.get());
    auto matrix = viewport.projectionMatrix();
    matrix.scale(viewport.scale(), viewport.scale());
    m_keyboardOutlineShader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
    m_keyboardOutlineShader->setUniform("destinationBox", QVector4D(box.x(), box.y(), box.width(), box.height()));
    m_keyboardOutlineShader->setUniform("outlineRadius", float(CardCornerRadius));
    m_keyboardOutlineShader->setUniform("outlineInk", QVector3D(ink.redF(), ink.greenF(), ink.blueF()));
    m_keyboardOutlineShader->setUniform("outlineOpacity", opacity);
    m_keyboardOutlineShader->setColorspaceUniforms(KWin::ColorDescription::sRGB,
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

void Effect::drawDestinationOutline(const KWin::RenderTarget &renderTarget,
    const KWin::RenderViewport &viewport, const KWin::Region &deviceRegion,
    KWin::LogicalOutput *screen, const QRectF &box, const QRectF &hole)
{
    QRectF clip = QRectF(KWin::effects->clientArea(KWin::MaximizeArea, screen))
        .intersected(QRectF(screen->geometry()));
    clip.setBottom(std::min(clip.bottom(), double(screen->geometry().bottom()) - 48.0));
    QList<QRectF> parts;
    for (const auto &dirty : deviceRegion.rects()) {
        const auto whole = QRectF(viewport.mapFromDeviceCoordinates(KWin::RectF(dirty)))
            .intersected(clip).intersected(box);
        if (whole.isEmpty()) continue;
        // Whatever of `hole` the box covers is left undrawn: the bands above,
        // below and beside it.
        const QRectF cut = whole.intersected(hole);
        if (cut.isEmpty()) {
            parts.append(whole);
            continue;
        }
        parts << QRectF(whole.left(), whole.top(), whole.width(), cut.top() - whole.top())
              << QRectF(whole.left(), cut.bottom(), whole.width(), whole.bottom() - cut.bottom())
              << QRectF(whole.left(), cut.top(), cut.left() - whole.left(), cut.height())
              << QRectF(cut.right(), cut.top(), whole.right() - cut.right(), cut.height());
    }
    // A destination is a box of frosted glass, so it reads as a place over
    // any window or wallpaper under it, light or dark. Without the blur it
    // is a filled box at the Visual sheet's hover step.
    if (m_frostedBox.paint(renderTarget, viewport, parts, box, CardCornerRadius, clip)) {
        m_frostDrawn[screen->name()].append(box.intersected(clip));
        return;
    }
    fillDestinationParts(renderTarget, viewport, parts, box, 0.13f, 0.f);
}

void Effect::fillDestinationParts(const KWin::RenderTarget &renderTarget,
    const KWin::RenderViewport &viewport, const QList<QRectF> &parts, const QRectF &box,
    float fill, float ring)
{
    QList<QVector2D> vertices;
    for (const auto &part : parts) {
        if (part.width() <= 0 || part.height() <= 0) continue;
        vertices << QVector2D(part.topLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomLeft())
                 << QVector2D(part.bottomLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomRight());
    }
    if (!m_destinationShader || vertices.isEmpty()) return;
    {
        KWin::ShaderBinder binder(m_destinationShader.get());
        auto matrix = viewport.projectionMatrix();
        matrix.scale(viewport.scale(), viewport.scale());
        m_destinationShader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
        m_destinationShader->setUniform("destinationBox", QVector4D(box.x(), box.y(), box.width(), box.height()));
        m_destinationShader->setUniform("outlineRadius", float(CardCornerRadius));
        const QVector3D ink = SurfaceTone::current().lineInk();
        m_destinationShader->setUniform("surfaceFill", QVector4D(ink, fill));
        m_destinationShader->setUniform("outlineOpacity", ring);
        m_destinationShader->setUniform("outlineInk", ink);
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
    // §11, Every display: each display paints its own cards.
    SessionScope scope(this, m_currentSession, isCardDisplay(screen) ? stageKeyFor(screen) : m_scopedKey);
    m_paintingOutput = screen;
    m_paintingRegion = deviceRegion;
    m_heldBoxDrawnOn = nullptr;
    if (screen) m_frostDrawn.remove(screen->name());
    if (!m_heldPictures.empty()) captureHeldPictures();
    if (screen && screen == tabletOutput()) {
        // Only this display's: another's are read until it paints again.
        for (auto it = m_cardLabelTargets.begin(); it != m_cardLabelTargets.end();) {
            if (!it.key() || it.key()->isDeleted() || it.key()->screen() == screen
                || m_cardStage->liveCardIndex(it.key()) >= 0) it = m_cardLabelTargets.erase(it);
            else ++it;
        }
        for (auto it = m_cardFaceRects.begin(); it != m_cardFaceRects.end();) {
            if (!it.key() || it.key()->isDeleted() || it.key()->screen() == screen
                || m_cardStage->liveCardIndex(it.key()) >= 0) it = m_cardFaceRects.erase(it);
            else ++it;
        }
    }
    if (!painted([&] { return KWin::effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen); })) return paintResult(false);
    // §11: an offered card's picture is kept once the scene has drawn it this
    // frame, so a sleeping window's newest content is what is kept; taken
    // before, a window just woken for the offer can still have none.
    if (m_roomOffer && screen == m_roomOffer->output) captureOfferPictures();
    // Labels, rails and outlines belong to the current desktop's presentation.
    if (m_previewDesktop) return paintResult(true);
    // Where anything below is painted over the scene, and so over the
    // pointer when KWin composites it.
    QList<QRectF> overlaid;
    // A display's dividers are drawn while it shows its layout, its cards
    // waiting behind it, as they are taken (beginRailFromInput).
    if (m_destinationShader && screen
        && (screen != tabletOutput() || !m_cardStage->isActive() || m_cardStage->presentsBento())
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
            const QVector3D ink = SurfaceTone::current().lineInk();
            m_destinationShader->setUniform("surfaceFill", QVector4D(ink, pill ? .85f : .035f));
            m_destinationShader->setUniform("outlineOpacity", pill ? 0.f : .8f);
            m_destinationShader->setUniform("outlineInk", ink);
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
        if (!previews.isEmpty()) overlaid << previews << pills;
    }
    // Draw only from a still-owned reservation. No layout solve, timers, native
    // outline window, or input grab belongs in the paint pass.
    const auto &reservation = m_carriedWindow ? m_carryDestination : m_lineDestination;
    bool detachShown = false;
    const auto cardEntry = m_carriedWindow ? m_carryCardEntryOutput : m_lineCardEntryOutput;
    const auto cardExit = m_carriedWindow ? m_carryCardExitOutput : QPointer<KWin::LogicalOutput>();
    if (const auto held = heldDestinationOn(screen)) {
        const QRectF box = *held;
        // Drawn under the window in hand when it painted; otherwise here.
        if (m_heldBoxDrawnOn != screen) drawDestinationOutline(renderTarget, viewport, deviceRegion, screen, box);
        overlaid.append(box);
        // The outline above is drawn for a Bento reservation, a Card Stage
        // entry or a card leaving at the bottom edge. §10 gives the bottom edge
        // the only detaching release, so a pane leaving its layout and a card
        // leaving its display are labelled, and an entry never is. A display
        // whose context line is claimed has the words said there instead.
        if ((cardExit && cardExit == screen)
            || (reservation && !cardEntry && reservation->detachesToDesktop())) {
            detachShown = true;
            if (!contextLineClaimed(screen)) m_detachLabel.render(renderTarget, viewport, box);
        }
    }
    if (screen && detachShown != (m_detachShownOn == screen))
        m_detachShownOn = detachShown ? screen : nullptr;
    drawStackLadderOutline(renderTarget, viewport, deviceRegion, screen);
    drawRoomOffer(renderTarget, viewport, deviceRegion, screen, overlaid);
    // Where a placement request is aimed, as a carried card's destination is.
    if (m_destinationShader && screen && m_placementPreview && m_placementPreviewOutput == screen) {
        drawDestinationOutline(renderTarget, viewport, deviceRegion, screen, QRectF(*m_placementPreview));
        overlaid.append(QRectF(*m_placementPreview));
    }
    if (screen && screen == tabletOutput() && m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Spread) {
        const auto &model = m_cardStage->model();
        const auto &windows = m_cardStage->liveCards();
        // A held card is above the row, so no name of the row shows through it.
        auto *held = m_cardStage->cardGrabActive() ? m_cardStage->selectedWindow() : nullptr;
        const QRectF heldRect = held ? m_cardLabelTargets.value(held) : QRectF();
        // The names, and a Stack's place, are said on a claimed context line.
        const bool namesSaid = contextLineClaimed(screen);
        for (int index = 0; !namesSaid && index < windows.size(); ++index) {
            auto *window = windows.at(index).data();
            if (!window || window->isDeleted() || m_cardStage->paintSlot(window) == 99)
                continue;
            const int cardId = index + 1;
            if (model.stackPositionForId(cardId)
                != model.stackActivePositionForId(cardId)) continue;
            const QString applicationName = spreadCardName(window, cardId);
            const int count = model.stackSizeForId(cardId);
            const auto target = m_cardLabelTargets.constFind(window);
            if (target == m_cardLabelTargets.cend()) continue;
            if (window != held && !heldRect.isEmpty()
                && CardLabelRenderer::labelRect(*target).intersects(heldRect)) continue;
            m_cardLabelRenderer.render(renderTarget, viewport,
                *target,
                {applicationName, stackPositionLabel(
                    m_cardStage->shownStackPosition(cardId), count)});
            overlaid.append(CardLabelRenderer::labelRect(*target));
        }
        if (isInUseDisplay(screen) || !m_cardsOnEveryDisplay)
            paintStuckNotes(renderTarget, viewport, deviceRegion, screen, heldRect, overlaid);
    }
    // Spread just closed onto the Active card: its notes as Spread drew them,
    // now where Gooseberry draws them, until Gooseberry's own are there.
    if (m_notesHandover && screen && screen == tabletOutput() && (isInUseDisplay(screen) || !m_cardsOnEveryDisplay))
        paintNotesHandover(renderTarget, viewport, overlaid);
    // A refused gesture's note, over everything on the display it was let go on.
    if (m_gestureNote && screen && m_gestureNote->output == screen && !contextLineClaimed(screen)) {
        m_gestureNoteLabel.render(renderTarget, viewport, m_gestureNote->box, m_gestureNote->text,
                                  gestureNoteOpacity(m_gestureNote->shown.elapsed()));
        overlaid.append(m_gestureNote->box);
    }
    // The pointer stays above everything Kadunce draws.
    const bool pointerPainted = paintPointerAbove(renderTarget, viewport, deviceRegion, screen, overlaid);
    m_paintingOutput = nullptr;
    return paintResult(pointerPainted);
}

void Effect::observeCardOwnership()
{
    observeCardBook();
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
        // Each display's cards are individual cards alike.
        std::vector<const CardStageController *> stages;
        for (const auto &display : session->displays) stages.push_back(display.second->cards.get());
        for (const CardStageController *stage : stages) {
            for (const auto &window : stage->liveCards()) {
                if (!window) continue;
                const auto identity = reinterpret_cast<quintptr>(window.data());
                if (held.contains(identity))
                    reported.push_back({identity, OwnershipViolation::Rule::TwoOwners,
                                        QStringLiteral("desktop ") + entry.first});
                held.insert(identity);
                cards.push_back(identity);
            }
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

void Effect::observeCardBook()
{
    m_cardBookSettle.start();
}

void Effect::readCardBook()
{
    if (!m_cardStage || !m_desktopStage || m_releasing || !m_scopedSession) return;
    QList<CardBookDisplay> displays;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        const bool holds = isCardDisplay(output);
        displays.append({output->name(), holds, holds, sizingFor(output) == DisplaySizing::Solo});
    }

    // This desktop's windows, read from every record that names one.
    QHash<quintptr, KWin::EffectWindow *> windows;
    const auto add = [&](KWin::EffectWindow *window) {
        if (window && !window->isDeleted()) windows.insert(reinterpret_cast<quintptr>(window), window);
    };
    QHash<quintptr, QString> paneOf;
    for (const auto &view : m_desktopStage->ownershipView())
        for (const auto pane : view.panes) paneOf.insert(pane, view.output);
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder())
        if (paneOf.contains(reinterpret_cast<quintptr>(window))) add(window);
    // §11: each display's own stage, by the display it holds cards for.
    QHash<quintptr, QStringList> stagesOf;
    QHash<quintptr, std::vector<quintptr>> behind;
    QHash<quintptr, quintptr> faceOf;
    for (const auto &display : m_scopedSession->displays) {
        const CardStageController &cards = *display.second->cards;
        const QString name = display.first.isEmpty() ? m_cardOutputName : display.first;
        for (const auto &card : cards.liveCards()) {
            if (!card) continue;
            add(card.data());
            stagesOf[reinterpret_cast<quintptr>(card.data())].append(name);
            const auto stack = cards.stackFrontToBack(card);
            if (stack.size() > 1)
                faceOf.insert(reinterpret_cast<quintptr>(card.data()), reinterpret_cast<quintptr>(stack.first()));
        }
        const auto &stacks = cards.paneStackRecords();
        for (auto it = stacks.cbegin(); it != stacks.cend(); ++it) {
            const auto holder = reinterpret_cast<quintptr>(it.key());
            for (const auto &member : it.value()) {
                if (!member || member->isDeleted()) continue;
                add(member.data());
                behind[reinterpret_cast<quintptr>(member.data())].push_back(holder);
            }
        }
    }

    std::vector<CardBookEntry> entries;
    entries.reserve(windows.size());
    for (auto it = windows.cbegin(); it != windows.cend(); ++it) {
        KWin::EffectWindow *window = it.value();
        CardBookEntry entry;
        entry.window = it.key();
        entry.screen = window->screen() ? window->screen()->name() : QString();
        const QStringList held = stagesOf.value(it.key());
        entry.card = !held.isEmpty();
        entry.cardStage = held.isEmpty() ? QString() : held.first();
        entry.cardStages = int(held.size());
        entry.paneOf = paneOf.value(it.key());
        entry.behindPanes = behind.value(it.key());
        entry.stackFace = faceOf.value(it.key());
        entry.minimized = window->isMinimized();
        entries.push_back(std::move(entry));
    }

    auto mismatches = checkCardBook(displays, entries);
    // Reported once, as ownership is: the book is read on every change.
    if (mismatches == m_observedCardBook) return;
    for (const auto &mismatch : mismatches) {
        if (std::find(m_observedCardBook.cbegin(), m_observedCardBook.cend(), mismatch)
            != m_observedCardBook.cend()) continue;
        const auto named = windows.value(mismatch.window);
        QString line = (named ? logNameOf(named)
                          : mismatch.window ? QStringLiteral("a window no record holds") : QStringLiteral("a display")) + QStringLiteral(": ")
            + cardBookRuleName(mismatch.rule);
        if (!mismatch.detail.isEmpty()) line += QStringLiteral(" (") + mismatch.detail + QLatin1Char(')');
        qWarning().noquote() << "Kadunce card book:" << line;
        if (m_cardBookLog.size() < 64) m_cardBookLog.append(line);
    }
    m_observedCardBook = std::move(mismatches);
}

KWin::EffectWindow *Effect::selectedWindow() const
{
    return m_cardStage->selectedWindow();
}

void Effect::handleWindowAdded(KWin::EffectWindow *window)
{
    // Gooseberry's notes coming back over the Active card as Spread closes are
    // already on screen as Spread's picture of them, so no opening animation
    // of the desktop's plays on them.
    if ((m_notesSpreadShown || m_notesHandover) && isNotesSurface(window))
        window->setData(KWin::WindowAddedGrabRole, QVariant::fromValue(static_cast<void *>(this)));
    if (m_carriedWindow && m_carryRuntime) m_carryRuntime->cancel();
    // A window just opened, from the dock or anywhere, appears in its pane.
    appearInPlace(window);
    Q_EMIT workspaceContextChanged();
    syncDesktopPreview();
    m_cardStage->liftSurfaceAboveCards(window);
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
    if (floatsAtItsOwnSize(window)) {
        centreFloatingWindow(window);
        return;
    }
    connectManagedWindow(window);
    const QPointer<KWin::EffectWindow> candidate(window);
    // §11, Every display: the display it opens on takes it.
    SessionScope scope(this, m_scopedSession, stageKeyFor(window->screen()));
    // §11, Connected: the card shown when the window opened, read now, since
    // KWin activates the window before it is ready to be admitted.
    KWin::LogicalOutput *cardDisplay = tabletOutput();
    const QPointer<KWin::EffectWindow> shownAtOpen = cardDisplay && connectedForCardStage()
            && m_cardStage->presentation() == CardPresentation::Active
            && !m_cardStage->presentsBento() && !m_cardStage->launcherGuestActive()
            && m_cardStage->activeCardIdentity() != window
        ? m_cardStage->activeCardIdentity() : nullptr;
    const auto admitReadyWindow = [this, candidate, shownAtOpen]() {
        // A window that opens on a desktop not shown is not a card here; that
        // desktop takes it when it is next shown.
        if (!candidate || candidate->isDeleted() || !isCardWindow(candidate)
            || !candidate->window() || !candidate->window()->readyForPainting()) {
            return;
        }
        // §11, Every display: it is a card of the display it opened on.
        SessionScope own(this, m_scopedSession, stageKeyFor(candidate->screen()));
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
        // §11, Connected: a window opening beside the one card shown joins it
        // in a layout, as a pick from Spread does, rather than replacing it.
        // The right edge names its place only, and snaps nothing; the pair
        // shares the width evenly, as every Connected layout does.
        KWin::LogicalOutput *spread = tabletOutput();
        const QPointer<KWin::EffectWindow> shown = spread && connectedForCardStage()
                && !m_cardStage->presentsBento() && !m_cardStage->launcherGuestActive()
                && candidate->screen() == spread
            ? shownAtOpen : nullptr;
        const QList<KWin::EffectWindow *> shownBehind = shown
            ? m_cardStage->stackFrontToBack(shown).mid(1) : QList<KWin::EffectWindow *>{};
        // §8: the layout could not take it, so the layout leaves the screen
        // before the card stage puts a card where the panes were. Without
        // this the card is drawn over a running layout, which is the one
        // state §2 does not name.
        (void)retireLayoutIntoSpreadGroup(candidate->screen());
        (void)m_cardStage->handleWindowAdded(candidate);
        if (shown && shown != candidate && !shown->isDeleted() && !shown->isMinimized()
            && shown->screen() == spread && m_cardStage->liveCardIndex(shown) >= 0
            && m_cardStage->liveCardIndex(candidate) >= 0
            && pairCardsIntoBento(spread, candidate, shown,
                   BentoSidePlacement{true, true, true, false})) {
            qInfo() << "Kadunce" << Revision << "paired opened" << logNameOf(candidate)
                    << "with" << logNameOf(shown);
            m_cardStage->notePaneStack(shown, shownBehind);
            m_cardStage->settlePaneStacks();
            KWin::effects->activateWindow(candidate);
        }
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
    // Nor does a closing one fade where it stood once Spread draws the notes.
    if ((m_notesSpreadShown || m_notesHandover) && isNotesSurface(window))
        window->setData(KWin::WindowClosedGrabRole, QVariant::fromValue(static_cast<void *>(this)));
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
    m_cardFaceRects.remove(window);
    m_previewRefs.remove(window);
    // A destroyed window leaves ownership without a transition: it no longer
    // exists to own, and its identity may be reused by the next allocation.
    m_ownership.forget(reinterpret_cast<quintptr>(window));
    if (window == m_settlingWindow) clearDropSettle();
    if (window == m_carriedWindow && m_carryRuntime) m_carryRuntime->cancel();
    m_activationOrder.remove(windowIdentity(window));
    m_leftLayouts.remove(window);
    {
        // CARD-LIFECYCLE.md §11: a Stack stays whole until one card is left,
        // so a closing pane's slot goes to the next card of its Stack. The
        // stage of the pane's display keeps it.
        SessionScope scope(this, m_scopedSession, stageKeyOf(window));
        if (m_cardStage->presentsBento() && connectedForCardStage() && m_desktopStage->managesWindow(window)) {
            const QList<KWin::EffectWindow *> behind = m_cardStage->paneStack(window);
            KWin::EffectWindow *next = behind.isEmpty() ? nullptr : behind.first();
            if (next && m_desktopStage->replacePaneWithCard(window, next,
                    [this, next] { return m_cardStage->releaseCardToLiveBento(next); }, true)) {
                m_cardStage->forgetPaneStack(window);
                m_cardStage->notePaneStack(next, behind.mid(1));
                m_cardStage->settlePaneStacks();
                if (KWin::effects->activeWindow() == window || !KWin::effects->activeWindow())
                    KWin::effects->activateWindow(next);
            }
        }
    }
    // The window may be held by a desktop that is not shown, on any display.
    forEachSession([&] {
        m_desktopStage->handleWindowClosed(window);
        forEachDisplayStage([&](CardStageController &stage) { stage.handleWindowClosed(window); });
    });
    if (m_table.isOpen()) {
        readTableWorkspaces();
        relayoutTable();
        refreshTable();
    }
    scheduleDissolve();
    Q_EMIT workspaceContextChanged();
}

void Effect::handleWindowActivated(KWin::EffectWindow *window)
{
    // §11: the stage holding the window answers, else its display's, and
    // its display is the one in use.
    SessionScope scope(this, m_scopedSession, stageKeyOf(window));
    if (isApplicationWindow(window)) {
        m_desktopInUse = nullptr;
        noteDisplayInUse(window->screen());
    } else if (window && window->isDesktop() && window->screen()) {
        // A click on a display's desktop makes that display the one in use.
        m_desktopInUse = window->screen();
        noteDisplayInUse(window->screen());
        qInfo() << "Kadunce" << Revision << "the desktop of" << window->screen()->name() << "is in use";
    }
    // Read before anything returns early, so every activation is counted.
    const bool spreadShown = m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Spread;
    const bool toWindowBefore = window && window == m_lastActiveApplication;
    const bool notesKeysBack = m_notesKeysReturn.activated(isNotesSurface(window),
        spreadShown, toWindowBefore);
    // KWin may name no window between Search going and the card it hands
    // the keys to, which is no activation of anything.
    const bool searchKeysBack = window
        && m_searchKeysReturn.activated(isSearchSurface(window), spreadShown, toWindowBefore);
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
                    << logNameOf(window);
            return;
        }
    }
    const QPointer<KWin::EffectWindow> focusedBefore = m_lastActiveApplication;
    if (isApplicationWindow(window)) {
        m_lastActiveApplication = window;
        m_activationOrder.insert(windowIdentity(window), ++m_activationSequence);
        Q_EMIT workspaceContextChanged();
    } else if (isNotesSurface(window)) {
        Q_EMIT workspaceContextChanged();
    }
    if (notesKeysBack) {
        qInfo() << "Kadunce" << Revision << "kept Spread as the notes gave the keys back to"
                << logNameOf(window);
        return;
    }
    // Search closing hands the keys back to the card it opened over. That is
    // Search going, not a pick of the card, even where Spread never took
    // Search in as its guest and so has no guest ending to read it from.
    if (searchKeysBack && !m_cardStage->launcherGuestActive()) {
        qInfo() << "Kadunce" << Revision << "kept Spread as Search gave the keys back to"
                << logNameOf(window);
        return;
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
                    << logNameOf(window);
            return;
        }
        dismissLauncherGuestFromInput();
    }
    if (callAloneAsItLeft(window)) return;
    if (admitActivatedCardToLiveBento(window)) return;
    m_cardStage->handleWindowActivated(window);
}

void Effect::markAloneOnPurpose(KWin::EffectWindow *card)
{
    m_aloneOnPurpose.removeIf([](const auto &held) { return !held || held->isDeleted(); });
    if (card && !m_aloneOnPurpose.contains(card)) m_aloneOnPurpose.append(card);
}

bool Effect::callAloneAsItLeft(KWin::EffectWindow *window)
{
    // CARD-LIFECYCLE.md §10: a card the top edge took out of a layout is
    // called from the dock or Spread as it left, alone, and the layout on
    // Spread's display waits in Spread for it. Only Meta+B or an edge lays it
    // out among them.
    if (!window || window->isDeleted() || !m_aloneOnPurpose.contains(window)
        || !isCardWindow(window) || m_cardStage->liveCardIndex(window) < 0
        || m_desktopStage->managesWindow(window) || m_cardStage->paneHolding(window)
        || m_cardStage->stackFrontToBack(window).size() > 1) return false;
    KWin::LogicalOutput *output = tabletOutput();
    if (!output || window->screen() != output) return false;
    const bool held = m_cardStage->inProjectedLayout(window);
    if (held && !m_cardStage->takePaneOutOfLayout(window)) return false;
    const bool live = m_cardStage->presentsBento() && m_desktopStage->hasSessionOnOutput(output->name());
    if (!held && !live) return false;
    if (live && !retireLayoutIntoSpreadGroup(output)) return false;
    (void)m_cardStage->promoteToActive(window);
    qInfo() << "Kadunce" << Revision << "called" << logNameOf(window) << "alone, as it left the layout on"
            << output->name();
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
    return true;
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
    // §11, Connected: a card waiting behind a pane, called, comes to the
    // front of that pane. It never takes a pane of its own, which would part
    // it from its Stack.
    if (auto *pane = m_cardStage->paneHolding(window); pane && m_desktopStage->managesWindow(pane))
        return callPaneStackCard(pane, window);
    KWin::LogicalOutput *output = window->screen();
    if (!output || !m_desktopStage->hasSessionOnOutput(output->name())) return false;
    qInfo() << "Kadunce" << Revision << "a hidden card was activated while Bento shows:"
            << logNameOf(window);
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

bool Effect::playPickedCardIntoLayout(KWin::EffectWindow *picked)
{
    // CARD-LIFECYCLE.md §11: on a display sized Connected a picked card
    // becomes a pane. A Stack fills one pane, so the cards behind the pick
    // wait behind its pane.
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!picked || picked->isDeleted() || !tablet || !connectedForCardStage()
        || picked->screen() != tablet) return false;
    const QList<KWin::EffectWindow *> stack = m_cardStage->stackFrontToBack(picked);
    const QList<KWin::EffectWindow *> behind = stack.mid(1);
    if (m_cardStage->hasBentoGroup()) {
        if (!m_cardStage->resumeBentoGroup()) return false;
        // The pick's Stack held a pane, and resuming put it there.
        if (m_desktopStage->managesWindow(picked)) {
            KWin::effects->activateWindow(picked);
            return true;
        }
        // §8: grow, or the pane used longest ago yields. With no pane the
        // pick fits, the layout returns to Spread and the pick is Active.
        if (!admitActivatedCardToLiveBento(picked)) {
            m_cardStage->handleWindowActivated(picked);
            return true;
        }
        // A refused retirement keeps the layout as it was.
        if (!m_desktopStage->managesWindow(picked)) return true;
        m_cardStage->notePaneStack(picked, behind);
        m_cardStage->settlePaneStacks();
        KWin::effects->activateWindow(picked);
        return true;
    }
    // No layout yet: the pick and the card in use before it become one, the
    // pick on the right and the two sharing the width evenly, as §11 sizes
    // every Connected layout. With no card in use, or the pick being that
    // card, the pick is shown alone.
    // Spread leaves the keyboard with the card in use; the last application
    // to have it stands in when nothing does. A window used on another
    // display since is in use there, so the card Spread opened over is the
    // one in use here (§11, Every display).
    KWin::EffectWindow *focused = KWin::effects->activeWindow();
    const QPointer<KWin::EffectWindow> inUse = focused && isApplicationWindow(focused)
        ? focused : m_lastActiveApplication.data();
    // A card of the pick's own Stack is the pick's, never its partner.
    const auto partnerHere = [this, picked, tablet, &stack](KWin::EffectWindow *card) {
        return card && card != picked && !stack.contains(card) && !card->isDeleted() && !card->isMinimized()
            && card->screen() == tablet && m_cardStage->liveCardIndex(card) >= 0 ? card : nullptr;
    };
    KWin::EffectWindow *partner = partnerHere(inUse.data());
    if (!partner && inUse && !inUse->isDeleted() && inUse->screen() != tablet)
        partner = partnerHere(m_cardStage->cardSpreadOpenedOver());
    const QList<KWin::EffectWindow *> partnerBehind = partner
        ? m_cardStage->stackFrontToBack(partner).mid(1) : QList<KWin::EffectWindow *>{};
    if (!m_cardStage->presentSelectedNow()) return false;
    if (partner && pairCardsIntoBento(tablet, picked, partner,
            BentoSidePlacement{true, true, true, false})) {
        m_cardStage->notePaneStack(picked, behind);
        m_cardStage->notePaneStack(partner, partnerBehind);
        m_cardStage->settlePaneStacks();
        KWin::effects->activateWindow(picked);
    }
    return true;
}

void Effect::holdLayoutLeftBehind(KWin::EffectWindow *card, KWin::LogicalOutput *output, int tries)
{
    const QPointer<KWin::EffectWindow> shown = card;
    const QPointer<KWin::LogicalOutput> display = output;
    // Only while the card is still the one shown alone in front of it.
    if (!shown || shown->isDeleted() || !display || display != tabletOutput() || m_releasing
        || !m_cardStage->isActive() || m_cardStage->presentation() != CardPresentation::Active
        || m_cardStage->activeCardIdentity() != shown
        || !m_desktopStage->hasSessionOnOutput(display->name())) return;
    if (!m_desktopStage->hasSettledSessionOnOutput(display->name())) {
        if (tries > 0) QTimer::singleShot(100, this, [this, shown, display, tries] {
            if (shown && display) holdLayoutLeftBehind(shown, display, tries - 1);
        });
        return;
    }
    if (!m_desktopStage->transferTabletSessionToSpread(display,
            [this](const auto &projection, const auto &commit) {
                return m_cardStage->admitBentoStack(projection, commit);
            })) return;
    // Spread took the layout and shows its row; the card stays shown alone.
    (void)m_cardStage->promoteToActive(shown);
    qInfo() << "Kadunce" << Revision << "held the layout" << logNameOf(shown) << "left on"
            << display->name() << "in Spread behind it";
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
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
        closeLauncherGuest();
    });
    return true;
}

void Effect::handleMinimumSizeChanged()
{
    // The window learns its new least size on the commit that sets it, so
    // the turn after reads it as KWin does.
    const QPointer<KWin::XdgToplevelInterface> toplevel = qobject_cast<KWin::XdgToplevelInterface *>(sender());
    QTimer::singleShot(0, this, [this, toplevel] {
        if (!toplevel || m_releasing) return;
        for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
            if (!window || window->isDeleted() || !window->window()
                || nativeMoveProtocol(window->window()->surface()) != toplevel) continue;
            DesktopSession *holder = sessionHolding(window);
            if (!holder) return;
            SessionScope scope(this, holder);
            SessionScope own(this, m_scopedSession, stageKeyOf(window));
            m_cardStage->refitAfterMinimumChanged(window);
            return;
        }
    });
}

void Effect::handleActiveGeometryChanged(KWin::EffectWindow *window,
                                         const KWin::RectF &)
{
    DesktopSession *holder = sessionHolding(window);
    SessionScope scope(this, holder ? holder : m_currentSession);
    SessionScope own(this, m_scopedSession, stageKeyOf(window));
    m_cardStage->handleActiveGeometryChanged(window);
}

void Effect::handleSessionStateChanged()
{
    bool owning = false;
    forEachSession([&] {
        owning = owning || hasActiveDesktopStage();
        forEachDisplayStage([&](CardStageController &stage) { owning = owning || stage.isActive(); });
    });
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

bool Effect::drawsOutsideFrame(const KWin::EffectWindow *window) const
{
    // A fractional scale can leave what KWin draws of a window a hair past
    // its frame with nothing there; a shadow lies a whole pixel or more out.
    constexpr double Slack = 0.5;
    const QRectF frame(window->frameGeometry());
    const QRectF drawn(window->expandedGeometry());
    return drawn.left() < frame.left() - Slack || drawn.top() < frame.top() - Slack
        || drawn.right() > frame.right() + Slack || drawn.bottom() > frame.bottom() + Slack;
}

bool Effect::trimsCorners(KWin::EffectWindow *window) const
{
    if (!roundsCorners()) return false;
    // Application windows and their dialogs, with or without a title bar;
    // panels, pop-ups and the keys keep their own shapes.
    if (!(window->isNormalWindow() || window->isDialog()) || window->isFullScreen()) return false;
    // A shadow lies outside the frame, and the trim would cut it away. A
    // window that draws its own shadow draws its own corners too.
    if (drawsOutsideFrame(window)) return false;
    // A maximized window meets the display's edges, which stay square.
    const KWin::Window *client = window->window();
    return !(client && client->maximizeMode() == KWin::MaximizeFull);
}

bool Effect::redirectSource(KWin::EffectWindow *window, bool reportChange)
{
    const QRectF frame(window->frameGeometry());
    const std::array<QRectF, 3> sourceBounds = {
        QRectF(QPointF(), frame.size()),
        QRectF(window->bufferGeometry()).translated(-frame.topLeft()),
        QRectF(window->expandedGeometry()).translated(-frame.topLeft())};
    const auto previousBounds = m_previewSourceBounds.constFind(window);
    const bool fresh = previousBounds == m_previewSourceBounds.cend();
    const bool changed = !fresh && *previousBounds != sourceBounds;
    if (changed) {
        // Damage alone does not describe a changed frame/buffer mapping.
        if (reportChange)
            qInfo() << "Kadunce preview source bounds changed" << logNameOf(window)
                    << "frame" << (*previousBounds)[0] << "->" << sourceBounds[0]
                    << "buffer" << (*previousBounds)[1] << "->" << sourceBounds[1]
                    << "expanded" << (*previousBounds)[2] << "->" << sourceBounds[2];
        unredirect(window);
    }
    m_previewSourceBounds.insert(window, sourceBounds);
    redirect(window);
    return fresh || changed;
}

void Effect::redirectPreviewSource(KWin::EffectWindow *window)
{
    if (redirectSource(window, true)) ++m_framePreviewsMade;
}

PaintResult Effect::drawTrimmedWindow(const KWin::RenderTarget &renderTarget,
                                      const KWin::RenderViewport &viewport,
                                      KWin::EffectWindow *window,
                                      int mask,
                                      const KWin::Region &deviceRegion,
                                      KWin::WindowPaintData &data)
{
    // The window's own picture is cut to the paper tier before any transform,
    // so its corners travel, scale and fade with it.
    (void)redirectSource(window, false);
    setShader(window, m_fanApertureShader.get());
    KWin::ShaderManager::instance()->pushShader(m_fanApertureShader.get());
    const double scale = viewport.scale();
    const QSizeF frame = window->frameGeometry().size();
    m_fanApertureShader->setUniform(m_fanPaintSizeLocation, QVector2D(1.0F, 1.0F));
    m_fanApertureShader->setUniform(m_fanApertureOriginLocation, QVector2D(0.0F, 0.0F));
    m_fanApertureShader->setUniform(m_fanApertureSizeLocation,
        QVector2D(static_cast<float>(std::round(frame.width() * scale)),
                  static_cast<float>(std::round(frame.height() * scale))));
    m_fanApertureShader->setUniform(m_fanApertureRadiusLocation,
        static_cast<float>(CardCornerRadius * scale));
    m_fanApertureShader->setUniform("tiltedSampling",
        qFuzzyIsNull(data.rotationAngle()) ? 0.0F : 1.0F);
    glActiveTexture(GL_TEXTURE0);
    const bool drawn = painted([&] { return KWin::OffscreenEffect::drawWindow(
        renderTarget, viewport, window, mask, deviceRegion, data); });
    KWin::ShaderManager::instance()->popShader();
    return paintResult(drawn);
}

PaintResult Effect::drawWindow(const KWin::RenderTarget &renderTarget,
                        const KWin::RenderViewport &viewport,
                        KWin::EffectWindow *window,
                        int mask,
                        const KWin::Region &deviceRegion,
                        KWin::WindowPaintData &data)
{
    // The picture kept of a card is the window as it stands, drawn plainly.
    if (window == m_capturingHeld)
        return paintResult(painted([&] { m_drawnAs.insert(window, QStringLiteral("kept picture taken")); return KWin::Effect::drawWindow(
            renderTarget, viewport, window, mask, deviceRegion, data); }));
    if (const auto offered = m_offerPictures.find(window);
        offered != m_offerPictures.end() && m_roomOfferShown.contains(window)
        && offered->second.draw(viewport, renderTarget, window, deviceRegion, data)) {
        m_drawnAs.insert(window, QStringLiteral("offered picture"));
        return paintResult(true);
    }
    if (const auto held = m_heldPictures.find(window); held != m_heldPictures.end()) {
        // Let go once the app has drawn twice at its new size.
        if (!held->second.resize && held->second.drawnSince >= 2) releaseHeldPicture(window);
        else if (held->second.picture.draw(viewport, renderTarget, window, deviceRegion, data)) {
            m_drawnAs.insert(window, QStringLiteral("kept picture"));
            return paintResult(true);
        }
    }
    const bool useFanAperture = window == m_fanApertureWindow
        && m_fanApertureShader
        && !m_fanPaintSize.isEmpty()
        && !m_fanApertureSize.isEmpty()
        && m_fanPaintSizeLocation >= 0
        && m_fanApertureOriginLocation >= 0
        && m_fanApertureSizeLocation >= 0
        && m_fanApertureRadiusLocation >= 0;

    if (!useFanAperture) {
        if (trimsCorners(window)) {
            m_drawnAs.insert(window, QStringLiteral("trimmed"));
            return drawTrimmedWindow(renderTarget, viewport, window, mask, deviceRegion, data);
        }
        if (const float outline = m_keyboardEdgeShader ? keyboardOutlineOpacity(window, data) : 0.0F;
            outline > 0.0F) {
            m_drawnAs.insert(window, QStringLiteral("outlined"));
            (void)redirectSource(window, false);
            setShader(window, m_keyboardEdgeShader.get());
            KWin::ShaderManager::instance()->pushShader(m_keyboardEdgeShader.get());
            const QColor ink = keyboardOutlineInk();
            m_keyboardEdgeShader->setUniform("outlineInk", QVector3D(ink.redF(), ink.greenF(), ink.blueF()));
            m_keyboardEdgeShader->setUniform("outlineOpacity", outline);
            m_keyboardEdgeShader->setUniform("bandTexels", float(2.0 * viewport.scale()));
            // How far the picture reaches past the frame, for its shadow, and
            // how far an application's own corner reaches in from it.
            const QRectF frame(window->frameGeometry());
            const QRectF picture(window->expandedGeometry());
            const double margin = std::max({frame.left() - picture.left(), frame.top() - picture.top(),
                picture.right() - frame.right(), picture.bottom() - frame.bottom(), 0.0});
            m_keyboardEdgeShader->setUniform("frameMargin", float(margin * viewport.scale()));
            m_keyboardEdgeShader->setUniform("cornerReach", float(24.0 * viewport.scale()));
            glActiveTexture(GL_TEXTURE0);
            const bool drawn = painted([&] { return KWin::OffscreenEffect::drawWindow(
                renderTarget, viewport, window, mask, deviceRegion, data); });
            KWin::ShaderManager::instance()->popShader();
            return paintResult(drawn);
        }
        m_drawnAs.insert(window, QStringLiteral("plain"));
        // Redirection is otherwise ephemeral. A window Kadunce does not trim,
        // hidden deck members, and shader failure all return
        // to KWin's exact r20 direct path.
        unredirect(window);
        m_previewSourceBounds.remove(window);
        if (!painted([&] { return KWin::OffscreenEffect::drawWindow(
            renderTarget, viewport, window, mask, deviceRegion, data); })) return paintResult(false);
        return paintResult(true);
    }

    m_drawnAs.insert(window, QStringLiteral("aperture"));
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
    // Gooseberry's notes over a window, while Spread draws them on its card.
    if (notesSurfaceStepsAside(window)) return paintResult(true);
    // Keys are drawn only once they are known to be the person's.
    if (!m_keysForPerson && window == KWin::effects->inputPanel()) return paintResult(true);
    // A card flicked closed stays out of sight while its app closes.
    if (m_cardStage->thrownAway(window)) return paintResult(true);
    // A card a Stack steps to waits unseen until it stands in its place.
    if (steppingInUnseen(window)) return paintResult(true);
    // A previewed desktop is drawn as it stands, with nothing of the current
    // desktop's presentation over or under it.
    if (m_previewDesktop) {
        if (!hiddenByDesktopPreview(window))
            if (!painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window, mask, deviceRegion, data); })) return paintResult(false);
        return paintResult(true);
    }
    // §11, Connected: a Stack scrubbed draws its ladder over its pane, and
    // the cards on it nowhere else.
    if (m_stackScrub.active() && (window == m_stackScrubPane || m_stackScrubShown.contains(window))) {
        const auto rung = stackScrubRung(window);
        if (!rung || !m_paintingOutput || m_stackScrubPane->screen() != m_paintingOutput)
            return paintResult(true);
        const auto [box, shows] = *rung;
        const QSizeF size = QRectF(window->frameGeometry()).size();
        if (size.isEmpty()) return paintResult(true);
        // Its own shape, as wide as its rung, its title bar at the rung's top.
        const KWin::Rect target = KWin::RectF(QRectF(box.x(), box.y(), box.width(),
            box.width() * size.height() / size.width())).toRect();
        KWin::Rect logicalRegion = window->expandedGeometry().toRect();
        setPositionTransformations(data, logicalRegion, window, target, Qt::KeepAspectRatio);
        const auto deviceTarget = viewport.mapToDeviceCoordinatesAligned(target);
        const auto clip = deviceRegion
            & KWin::Region(viewport.mapToDeviceCoordinatesAligned(KWin::RectF(shows.intersected(box))))
            & roundedClip(deviceTarget, CardCornerRadius * viewport.scale());
        if (!painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window,
            mask | PAINT_WINDOW_TRANSFORMED, clip, data); })) return paintResult(false);
        return paintResult(true);
    }
    // §11, Connected: a card the open room offers is drawn as its picture
    // there, and nowhere else.
    if (m_roomOffer && m_roomOfferShown.contains(window)) {
        const int index = m_roomOffer->cards.indexOf(window);
        if (index < 0 || index >= m_roomOffer->tiles.size() || !m_paintingOutput
            || m_paintingOutput != m_roomOffer->output) return paintResult(true);
        const QRectF tile = m_roomOffer->tiles.at(index);
        const KWin::Rect target = KWin::RectF(tile).toRect();
        KWin::Rect logicalRegion = window->expandedGeometry().toRect();
        // On the card's backing, as in Spread, so a see-through app still
        // reads as a card.
        // Rounded at its own corners, a narrow picture as a wide one.
        const QRectF picture = offerPicture(tile, QRectF(window->frameGeometry()).size());
        if (logicalRegion.width() > 0 && logicalRegion.height() > 0)
            paintCardSurface(m_destinationShader.get(), renderTarget, viewport,
                deviceRegion & KWin::Region(viewport.mapToDeviceCoordinatesAligned(KWin::RectF(picture))),
                picture, 0.0, float(data.opacity()), 0.0f, SurfaceTone::current().cardBacking(), 1.0F);
        setPositionTransformations(data, logicalRegion, window, target, Qt::KeepAspectRatio);
        const auto deviceTarget = viewport.mapToDeviceCoordinatesAligned(KWin::RectF(picture));
        const auto clip = deviceRegion
            & KWin::Region(deviceTarget)
            & roundedClip(deviceTarget, CardCornerRadius * viewport.scale());
        if (!painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window,
            mask | PAINT_WINDOW_TRANSFORMED, clip, data); })) return paintResult(false);
        return paintResult(true);
    }
    if (hiddenOnOtherDesktop(window)) return paintResult(true);
    // A sleeping card stands in Spread dimmed, so it can be found and woken,
    // unless the context line says so instead and the card stands whole.
    if (m_sleepingShown.contains(window) && !contextLineClaimed(tabletOutput()))
        data.multiplyOpacity(SleepingCardOpacity);
    if (guestNeighborOpacity() <= 0.0 && m_cardStage->launcherGuestActive()
        && m_paintingOutput == tabletOutput() && isCardWindow(window)
        && m_cardStage->paintSlot(window) != 99) return paintResult(true);
    const auto settle = window == m_settlingWindow ? dropSettleRect() : bentoMotionRect(window);
    if (settle && window != m_settlingWindow && m_paintingOutput
        && window->window()->moveResizeOutput() != m_paintingOutput) return paintResult(true);
    // A pane that gave way fades where it stood, wherever its window now is.
    std::optional<QRectF> yieldingRect;
    if (!settle && window != m_carriedWindow) {
        if (const auto opacity = yieldingPaneOpacity(window)) {
            for (const auto &yielding : std::as_const(m_yieldingPanes))
                if (yielding.window == window) yieldingRect = yielding.rect;
            if (yieldingRect) data.multiplyOpacity(*opacity);
        }
    }
    if (((window == m_carriedWindow && m_carryRuntime) || settle || yieldingRect) && m_paintingOutput) {
        const auto drawnAt = settle ? settle : yieldingRect;
        if (window == m_carriedWindow && !drawnAt) drawHeldBoxUnder(renderTarget, viewport);
        const auto plan = drawnAt
            ? carryPaintPlan(*drawnAt, drawnAt->topLeft(), QRectF(m_paintingOutput->geometry()))
            : carryPaintPlan(m_carryPickup, m_carryRuntime->handoff.carry().position(),
                             QRectF(m_paintingOutput->geometry()));
        if (!plan || plan->clip.isEmpty()) return paintResult(true);
        KWin::Rect logicalRegion = window->expandedGeometry().toRect();
        // Leaning into a Stack it would join, it shrinks to 80% about the
        // pointer and tilts 1.5 degrees about its centre.
        const double lean = window == m_carriedWindow && !drawnAt ? stackLean() : 0.0;
        const QRectF leaned = leanedIntoStack(plan->target, lean);
        const KWin::Rect target = KWin::RectF(leaned).toRect();
        setPositionTransformations(data, logicalRegion, window, target, Qt::KeepAspectRatioByExpanding);
        if (lean > 0.0 && data.xScale() > 0 && data.yScale() > 0) {
            data.setRotationAngle(1.5 * lean);
            data.setRotationOrigin(QVector3D((target.x() + target.width() / 2.0 - logicalRegion.x()) / data.xScale(),
                (target.y() + target.height() / 2.0 - logicalRegion.y()) / data.yScale(), 0.0));
        }
        const int tiltReach = lean > 0.0
            ? int(std::ceil(std::sin(1.5 * lean * M_PI / 180.0) * std::max(target.width(), target.height()))) + 2 : 0;
        const auto deviceTarget = viewport.mapToDeviceCoordinatesAligned(target);
        const bool rounded = m_fanApertureShader && data.xScale() > 0 && data.yScale() > 0;
        m_fanApertureWindow = rounded ? window : nullptr;
        m_fanPaintSize = QSizeF(data.xScale(), data.yScale());
        m_fanApertureOrigin = QPointF((target.x() - logicalRegion.x()) * viewport.scale(),
                                      (target.y() - logicalRegion.y()) * viewport.scale());
        m_fanApertureSize = QSizeF(deviceTarget.size());
        m_fanApertureRadius = CardCornerRadius * viewport.scale();
        // Tilted, its corners reach past the box it was carried in: the
        // display, not that box, bounds them.
        const QRectF bounds = tiltReach > 0
            ? leaned.adjusted(-tiltReach, -tiltReach, tiltReach, tiltReach).intersected(QRectF(m_paintingOutput->geometry()))
            : plan->clip;
        const auto clip = deviceRegion
            & KWin::Region(viewport.mapToDeviceCoordinatesAligned(KWin::RectF(bounds)))
            & (tiltReach > 0 ? KWin::Region(viewport.mapToDeviceCoordinatesAligned(
                                   KWin::RectF(leaned.adjusted(-tiltReach, -tiltReach, tiltReach, tiltReach))))
               : rounded ? KWin::Region(deviceTarget) : roundedClip(deviceTarget, CardCornerRadius * viewport.scale()));
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
        paintKeyboardOutline(renderTarget, viewport, window, deviceRegion, data);
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
    // take is a filled box at the Stack's seam, just below the held card. Let
    // go, it fades under the card then in front.
    if (window == m_cardStage->selectedWindow() && m_paintingOutput == tablet && m_destinationShader) {
        const auto outline = m_cardStage->stackOutline();
        if (!outline.rect.isEmpty() && outline.opacity > 0.0) {
            paintCardSurface(m_destinationShader.get(), renderTarget, viewport,
                deviceRegion & KWin::Region(viewport.mapToDeviceCoordinatesAligned(tablet->geometry())),
                QRectF(outline.rect), 0.6, float(0.2 * outline.opacity), 0.0f,
                SurfaceTone::current().lineInk());
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
    if (!paintPose.visible) {
        return paintResult(true);
    }
    KWin::Rect visualTarget = target;
    // A card shaped unlike its slot, as a pane's card can be, is drawn whole
    // at its own shape, with no backing either side of it.
    if (!grabbedWindow && logicalRegion.width() > 0 && logicalRegion.height() > 0
        && visualTarget.width() > 0 && visualTarget.height() > 0) {
        const double shape = double(logicalRegion.width()) / logicalRegion.height();
        const double slotShape = double(visualTarget.width()) / visualTarget.height();
        if (std::abs(shape / slotShape - 1.0) > 0.06) {
            const QSizeF fitted = QSizeF(logicalRegion.width(), logicalRegion.height())
                .scaled(QSizeF(visualTarget.width(), visualTarget.height()), Qt::KeepAspectRatio);
            visualTarget = KWin::Rect(
                qRound(visualTarget.x() + (visualTarget.width() - fitted.width()) / 2.0),
                qRound(visualTarget.y() + (visualTarget.height() - fitted.height()) / 2.0),
                qRound(fitted.width()), qRound(fitted.height()));
        }
    }
    const bool rotatedFanCard = !qFuzzyIsNull(paintPose.rotation);
    {
        const KWin::Region outputFence(
            viewport.mapToDeviceCoordinatesAligned(m_paintingOutput->geometry()));
        paintCardSurface(m_destinationShader.get(), renderTarget, viewport,
            deviceRegion & outputFence, QRectF(visualTarget), paintPose.rotation,
            float(data.opacity()), 0.0f, SurfaceTone::current().cardBacking(), 1.0F);
    }
    m_cardLabelTargets.insert(window, QRectF(target));
    KWin::Effect::setPositionTransformations(
        data, logicalRegion, window, visualTarget,
        Qt::KeepAspectRatio);
    // KWin hands back where it will draw the window's frame, fitted inside
    // the card this frame; a note placed on the window stands on exactly
    // that, never on the slot the card is heading for.
    m_cardFaceRects.insert(window, QRectF(logicalRegion));
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

    // deviceRegion is KWin's actual renderer clip.
    const KWin::Rect apertureTarget = visualTarget;
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
    const double apertureRadius = CardCornerRadius * viewport.scale();
    const KWin::Region cardClip = (rotatedFanCard
        ? deviceRegion
        : deviceRegion & (useFanAperture ? KWin::Region(deviceAperture)
            : roundedClip(deviceAperture, apertureRadius)))
        & outputFence;
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

    const bool drawn = painted([&] { return KWin::effects->paintWindow(
        renderTarget, viewport, window, mask | PAINT_WINDOW_TRANSFORMED,
        cardClip, data); });

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
