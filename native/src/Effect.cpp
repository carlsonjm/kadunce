/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KLocalizedString>
#include "EffectInternal.h"

namespace Kadunce
{

namespace
{
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
// The shadow a card casts: a soft rounded box under the card, fallen a little
// below it, drawn only outside the card so its own trimmed corners stay clear.
constexpr auto ShadowFragment = R"GLSL(#version 140
in vec2 point;
out vec4 fragColor;
uniform vec4 cardBox;
uniform float cardRadius;
uniform float shadowBlur;
uniform float shadowOffset;
uniform float shadowOpacity;
#include "colormanagement.glsl"
float roundedBox(vec2 p, vec4 box, float radius) {
    vec2 halfSize = box.zw * 0.5;
    float r = min(radius, min(halfSize.x, halfSize.y));
    vec2 q = abs(p - box.xy - halfSize) - (halfSize - vec2(r));
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}
void main() {
    float card = roundedBox(point, cardBox, cardRadius);
    float fallen = roundedBox(point - vec2(0.0, shadowOffset), cardBox, cardRadius);
    float t = clamp((fallen + 0.25 * shadowBlur) / (1.25 * shadowBlur), 0.0, 1.0);
    float alpha = shadowOpacity * (1.0 - t) * (1.0 - t) * smoothstep(-0.5, 0.5, card);
    fragColor = nitsToDestinationEncoding(sourceEncodingToNitsInDestinationColorspace(vec4(0.0, 0.0, 0.0, alpha)));
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
}

Effect::Effect()
{
    connect(KWin::effects, &KWin::EffectsHandler::windowDeleted, this,
        [this](KWin::EffectWindow *window) {
            m_previewSourceBounds.remove(window);
            if (const auto trace = m_shadowTraces.constFind(window); trace != m_shadowTraces.cend()) {
                KWin::effects->addRepaint(KWin::RectF(trace->drawn.united(trace->expected)).toAlignedRect());
                m_shadowTraces.erase(trace);
            }
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
        m_shadowShader = KWin::ShaderManager::instance()->generateCustomShader(
            KWin::ShaderTrait::UniformColor, QByteArray(DestinationVertex), QByteArray(ShadowFragment));
        m_keyboardOutlineShader = KWin::ShaderManager::instance()->generateCustomShader(
            KWin::ShaderTrait::UniformColor, QByteArray(DestinationVertex), QByteArray(KeyboardOutlineFragment));
        m_keyboardEdgeShader = KWin::ShaderManager::instance()->generateCustomShader(
            KWin::ShaderTrait::MapTexture, QByteArray(KeyboardEdgeVertex), QByteArray(KeyboardEdgeFragment));
        m_frostedBox.create();
        m_settingsFrost.create();
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
        m_showSpreadAction = new QAction(i18n("Show Spread"), this);
        m_showSpreadAction->setObjectName(
            QStringLiteral("Kadunce Show Spread"));
        connect(m_showSpreadAction, &QAction::triggered,
                this, &Effect::showCardLine);
        KWin::effects->registerTouchBorder(
            KWin::ElectricBottom, m_showSpreadAction);

        m_showActiveAction = new QAction(i18n("Show Active card"), this);
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
    m_sessionGoesOn.setSingleShot(true);
    connect(&m_sessionGoesOn, &QTimer::timeout, this, [this] {
        if (KWin::effects->sessionState() == KWin::SessionState::Normal) endSessionEndGlass();
    });
    connect(KWin::effects, &KWin::EffectsHandler::screenRemoved,
            this, &Effect::handleScreenRemoved);
    connect(KWin::effects, &KWin::EffectsHandler::screenAdded, this,
            [this](KWin::LogicalOutput *output) {
                m_displaysChanged.start();
                noteDisplayJoined(output);
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
        if (m_lineNotice && m_lineNotice->shown.elapsed() >= GestureNoteTiming::TotalMs) m_lineNotice.reset();
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
        ++m_carriesBegun;
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
                // The lock screen takes the releases from here on. A card held
                // in Spread is put back by the stage that holds it.
                if (m_inputRouter && m_currentSession) {
                    SessionScope stroke(this, m_currentSession,
                        m_strokeContacts.isEmpty() ? m_scopedKey : m_strokeKey);
                    m_inputRouter->cancelForLock();
                }
                m_strokeContacts.clear();
                if (m_touchWitness) (void)m_touchWitness->touchCancel();
            });
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        connectManagedWindow(window);
    }

    // A saved layout's groups arrive as an array of dictionaries.
    qDBusRegisterMetaType<QList<QVariantMap>>();
    if (!QDBusConnection::sessionBus().registerObject(
            QStringLiteral("/Kadunce"),
            QStringLiteral("co.goodinput.Kadunce"), this,
            QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals)) {
        qWarning() << "Kadunce" << Revision
                   << "could not publish the workspace context interface";
    }
    // Before any window of this sign-in arrives, so the first one opened by
    // hand starts the gathering of apps opened together.
    startWorkspaceMemory();

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
        m_touchWitness->touchedAt = [this](const QPointF &position) { noteEdgeContact(position); };
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
            && window->window() && window->window()->moveResizeOutput() == tablet
            && !sessionHolding(window) && window->window()->readyForPainting()
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
    // A window a saved layout or a bring-back waits for is claimed before it
    // is admitted.
    if (!claimLayoutWindow(window)) claimBroughtBackWindow(window);
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
            completeBringBackForWindow(candidate);
            completeLayoutForWindow(candidate);
            return;
        }
        // §3: a display that can own cards and holds none takes the arrival
        // as its Active card, with every other window there as a card.
        if (!m_cardStage->isActive() && startTabletInCards(candidate)) {
            completePlacementForWindow(candidate);
            completeBringBackForWindow(candidate);
            completeLayoutForWindow(candidate);
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
        completeBringBackForWindow(candidate);
        completeLayoutForWindow(candidate);
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
                // The pane keeps the focus, on its next card, whatever KWin
                // hands it to as the window goes: what it picks is the window
                // used before, often a card behind another pane, and taking
                // that would change the other pane under the person.
                const bool inUse = window == m_lastActiveApplication.data()
                    || KWin::effects->activeWindow() == window || !KWin::effects->activeWindow();
                if (inUse) {
                    m_closeFocusTo = next;
                    activateShownPane(next);
                    QTimer::singleShot(250, this, [this, next = QPointer(next)] {
                        if (m_closeFocusTo == next) m_closeFocusTo.clear();
                    });
                }
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
    // A closing pane's next card keeps the focus KWin handed elsewhere.
    if (m_closeFocusTo && window != m_closeFocusTo && isApplicationWindow(window)
        && !m_closeFocusTo->isDeleted() && m_desktopStage->managesWindow(m_closeFocusTo)) {
        qInfo() << "Kadunce" << Revision << "kept the focus on" << logNameOf(m_closeFocusTo)
                << "in the closed pane's place, not" << logNameOf(window);
        KWin::effects->activateWindow(m_closeFocusTo);
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
    const bool ending = KWin::effects->sessionState() != KWin::SessionState::Normal;
    // Where every window stands is remembered before the cards are given
    // back (WorkspaceMemory.h); nothing is tidied while the session may end.
    if (ending && !m_memoryEndWritten) {
        m_memoryEndWritten = true;
        rememberWorkspaceAtSessionEnd();
    }
    m_sessionHold.noteState(!ending, steadyMs());
    if (!ending && m_sessionHold.ending())
        QTimer::singleShot(SessionEndHold::GoesOnMs + 50, this, [this] {
            if (!m_sessionHold.holds(steadyMs())) {
                resumeWorkspaceMemory();
                scheduleDissolve();
            }
        });
    // The power question just answered: the session ends behind its glass.
    if (ending && (m_powerQuestionSeen.isValid() && m_powerQuestionSeen.elapsed() < 2000))
        beginSessionEndGlass(owning);
    if (owning && ending) {
        qInfo() << "Kadunce" << Revision
                << "restoring managed windows before session save or shutdown";
        release();
    }
    if (ending) m_sessionGoesOn.stop();
    else if (m_sessionEnd.on && !m_sessionGoesOn.isActive()) m_sessionGoesOn.start(SessionGoesOnMs);
}

} // namespace Kadunce
