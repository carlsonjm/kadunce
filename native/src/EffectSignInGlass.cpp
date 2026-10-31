/* SPDX-License-Identifier: GPL-2.0-or-later */
// The glass a fresh sign-in starts behind: every display frosted as Settings
// frosts it, under the panels and the splash, until the apps the session
// reopens have settled and one is shown.
#include "Effect.h"
#include "EffectInternal.h"

#include <input.h>
#include <input_event.h>
#include <input_event_spy.h>

#include <QFile>
#include <QStandardPaths>

#include <unistd.h>

#include <algorithm>
#include <cmath>

namespace Kadunce
{
namespace
{
// How long this compositor has run, from the kernel's count of its start.
qint64 compositorAgeMs()
{
    QFile stat(QStringLiteral("/proc/self/stat"));
    QFile uptime(QStringLiteral("/proc/uptime"));
    if (!stat.open(QIODevice::ReadOnly) || !uptime.open(QIODevice::ReadOnly)) return -1;
    const QByteArray line = stat.readAll();
    // The fields after the command's closing bracket; the start is the 20th.
    const auto fields = line.mid(line.lastIndexOf(')') + 2).split(' ');
    if (fields.size() < 20) return -1;
    const long ticks = sysconf(_SC_CLK_TCK);
    const double startedS = fields.at(19).toDouble() / double(ticks > 0 ? ticks : 100);
    const double upS = uptime.readAll().split(' ').value(0).toDouble();
    return qint64((upS - startedS) * 1000.0);
}

// Whether this session starts with a splash: KDE's splash setting names an
// engine, "none" when the person switched it off.
bool splashRuns()
{
    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/ksplashrc");
    QFile file(config);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return true;
    bool inGroup = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(QLatin1Char('['))) inGroup = line == QLatin1String("[KSplash]");
        else if (inGroup && line.startsWith(QLatin1String("Engine=")))
            return line.mid(7).compare(QLatin1String("none"), Qt::CaseInsensitive) != 0;
    }
    return true;
}

bool isSplash(const KWin::EffectWindow *window)
{
    return window && (window->isSplash() || window->windowClass().contains(QLatin1String("ksplashqml"), Qt::CaseInsensitive));
}
} // namespace

// Any press, tap or key clears the glass at once, as a person reaching for
// the desktop has nothing to wait for.
struct Effect::SignInGlassSpy final : KWin::InputEventSpy {
    Effect *effect;
    explicit SignInGlassSpy(Effect *e) : effect(e) {}
    void clear() const
    {
        QMetaObject::invokeMethod(effect, [e = QPointer<Effect>(effect)] {
            if (e) e->clearSignInGlass("a touch, click or key");
        }, Qt::QueuedConnection);
    }
    void pointerButton(KWin::PointerButtonEvent *event) override
    {
        if (event->state == KWin::PointerButtonState::Pressed) clear();
    }
    void touchDown(KWin::TouchDownEvent *) override { clear(); }
    void keyboardKey(KWin::KeyboardKeyEvent *event) override
    {
        if (event->state == KWin::KeyboardKeyState::Pressed) clear();
    }
};

void Effect::beginSignInGlass(bool windowsOpen)
{
    // Only a session starting: a compositor just begun with nothing open. An
    // effect loaded again later in a session finds its windows, or an old
    // compositor, and leaves the desktop as it is.
    const qint64 age = compositorAgeMs();
    if (windowsOpen || age < 0 || age > SignInGlassFreshMs) return;
    m_signInGlass = SignInGlass{};
    m_signInGlass.on = true;
    m_signInGlass.lastStep.start();
    // The splash comes up from black over the sign-in screen's hand-off;
    // with no splash, the glass itself comes up from black, alike.
    if (!splashRuns()) m_signInGlass.fromBlack.start();
    m_signInGlassCap.setSingleShot(true);
    connect(&m_signInGlassCap, &QTimer::timeout, this, [this] { clearSignInGlass("its time ran out"); });
    m_signInGlassCap.start(SignInGlassLongestMs);
    // Once the splash has gone, a sign-in with nothing coming back clears
    // after the burst's own quiet.
    m_signInGlassQuiet.setSingleShot(true);
    connect(&m_signInGlassQuiet, &QTimer::timeout, this, [this] {
        if (m_signInHeld.isEmpty() && !m_signInBurst.open(steadyMs() - m_sessionStartedMs))
            clearSignInGlass("nothing was reopened");
    });
    connect(KWin::effects, &KWin::EffectsHandler::windowClosed, this, [this](KWin::EffectWindow *window) {
        if (!m_signInGlass.on || m_signInGlass.clearing || !isSplash(window)) return;
        m_signInGlass.splashGone = true;
        m_signInGlassQuiet.start(int(SignInBurst::QuietMs));
    });
    m_signInGlassSpy = std::make_shared<SignInGlassSpy>(this);
    KWin::input()->installInputEventSpy(m_signInGlassSpy.get());
    qInfo() << "Kadunce" << Revision << "starts the session behind glass;" << age << "ms after the compositor";
    KWin::effects->addRepaintFull();
}

void Effect::clearSignInGlass(const char *why)
{
    if (!m_signInGlass.on || m_signInGlass.clearing) return;
    m_signInGlass.clearing = true;
    m_signInGlass.clearingSince.start();
    m_signInGlassCap.stop();
    m_signInGlassQuiet.stop();
    dropSignInGlassSpy();
    qInfo() << "Kadunce" << Revision << "clears the sign-in glass:" << why;
    KWin::effects->addRepaintFull();
}

void Effect::dropSignInGlassSpy()
{
    if (!m_signInGlassSpy) return;
    KWin::input()->uninstallInputEventSpy(m_signInGlassSpy.get());
    m_signInGlassSpy.reset();
}

void Effect::revealSignInFront(KWin::EffectWindow *window)
{
    if (!m_signInGlass.on || !window) return;
    m_signInGlass.front = window;
    m_signInGlass.frontSince.start();
    clearSignInGlass("the reopened apps have settled");
}

void Effect::stepSignInGlass(KWin::ScreenPrePaintData &data)
{
    if (!m_signInGlass.on) return;
    const int dissolve = motionDuration(SignInGlassDissolveMs, KWin::effects->animationTimeFactor());
    const int grow = motionDuration(SignInFrontGrowMs, KWin::effects->animationTimeFactor());
    if (m_signInGlass.clearing && m_signInGlass.clearingSince.elapsed() >= dissolve
        && (!m_signInGlass.frontSince.isValid() || m_signInGlass.frontSince.elapsed() >= grow)
        && (!m_signInGlass.fromBlack.isValid()
            || m_signInGlass.fromBlack.elapsed() >= motionDuration(SignInFromBlackMs, KWin::effects->animationTimeFactor()))) {
        m_signInGlass = SignInGlass{};
        KWin::effects->addRepaintFull();
        return;
    }
    // A window that is none of the apps coming back, such as the wallet or
    // a password question, is asking something: the glass clears for it.
    if (!m_signInGlass.clearing && !m_sessionEnd.on) {
        for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
            if (!window || window->isDeleted() || !window->isVisible() || isSplash(window)) continue;
            if (!(window->isNormalWindow() || window->isDialog())) continue;
            if (rememberedAtSignOut(window)) continue;
            if (std::any_of(m_signInHeld.cbegin(), m_signInHeld.cend(),
                            [window](const auto &held) { return held.window == window; })) continue;
            clearSignInGlass("a window asks something");
            break;
        }
    }
    if (data.screen) data.paint |= KWin::Rect(QRectF(data.screen->geometryF()).toAlignedRect());
    m_continueRepaint = true;
}

void Effect::drawSignInBlack(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                             KWin::LogicalOutput *screen)
{
    if (!m_signInGlass.on || !m_signInGlass.fromBlack.isValid() || !screen) return;
    const int rise = motionDuration(SignInFromBlackMs, KWin::effects->animationTimeFactor());
    const double t = rise > 0 ? std::clamp(double(m_signInGlass.fromBlack.elapsed()) / rise, 0.0, 1.0) : 1.0;
    // Eased as the session's end darkens, the other way round.
    const double black = 1.0 - (1.0 - std::pow(1.0 - t, 3.0));
    if (black <= 0.0) return;
    const QRectF whole(screen->geometryF());
    QList<QRectF> parts;
    for (const auto &dirty : m_paintingRegion.rects()) {
        const QRectF part = QRectF(viewport.mapFromDeviceCoordinates(KWin::RectF(dirty))).intersected(whole);
        if (!part.isEmpty()) parts.append(part);
    }
    // The fill's box reaches well past the display, so its rounded corners
    // fall outside and black meets every edge.
    const qreal corner = 64.0;
    fillDestinationParts(renderTarget, viewport, parts, whole.adjusted(-corner, -corner, corner, corner),
                         float(black), 0.f, QVector3D(0.f, 0.f, 0.f));
}

float Effect::signInGlassStrength() const
{
    if (!m_signInGlass.on) return 0.f;
    if (!m_signInGlass.clearing) return 1.f;
    const int dissolve = motionDuration(SignInGlassDissolveMs, KWin::effects->animationTimeFactor());
    if (dissolve <= 0) return 0.f;
    return float(1.0 - std::clamp(double(m_signInGlass.clearingSince.elapsed()) / dissolve, 0.0, 1.0));
}

bool Effect::liesOverSignInGlass(KWin::EffectWindow *window) const
{
    // The panels and the splash are drawn over the glass as they are;
    // windows and the desktop are frosted under it.
    return window && (window->isDock() || isSplash(window));
}

bool Effect::waitsBehindSignInGlass(KWin::EffectWindow *window, KWin::WindowPaintData &data) const
{
    // A surface standing above the panels, such as the keyboard or a sheet,
    // would be drawn over glass laid under them: it waits unseen while the
    // glass stands, and comes in as the glass dissolves.
    if (!m_signInGlass.on || !window || !window->window() || window->isDesktop() || liesOverSignInGlass(window)
        || !window->window()->inherits("KWin::LayerShellV1Window")) return false;
    const float strength = signInGlassStrength();
    if (strength >= 1.f) return true;
    data.multiplyOpacity(1.0 - strength);
    return false;
}

void Effect::drawSignInGlass(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                             KWin::EffectWindow *under)
{
    KWin::LogicalOutput *screen = m_paintingOutput;
    if (!screen || m_signInGlassDrawnOn == screen || m_sessionEnd.on) return;
    const float strength = signInGlassStrength();
    if (strength <= 0.f) return;
    if (under && !liesOverSignInGlass(under)) return;
    m_signInGlassDrawnOn = screen;
    const QRectF whole(screen->geometryF());
    QList<QRectF> parts;
    for (const auto &dirty : m_paintingRegion.rects()) {
        const QRectF part = QRectF(viewport.mapFromDeviceCoordinates(KWin::RectF(dirty))).intersected(whole);
        if (!part.isEmpty()) parts.append(part);
    }
    if (m_settingsFrost.paint(renderTarget, viewport, parts, whole, 0.0, whole, strength))
        m_frostDrawn[screen->name()].append(whole);
}

void Effect::growSignInFront(KWin::EffectWindow *window, int &mask, KWin::WindowPaintData &data) const
{
    if (!m_signInGlass.on || window != m_signInGlass.front || !m_signInGlass.frontSince.isValid()) return;
    const int grow = motionDuration(SignInFrontGrowMs, KWin::effects->animationTimeFactor());
    const double t = grow > 0 ? std::clamp(double(m_signInGlass.frontSince.elapsed()) / grow, 0.0, 1.0) : 1.0;
    // Ease out: it arrives quickly and settles into place.
    const double eased = 1.0 - std::pow(1.0 - t, 3.0);
    const double scale = SignInFrontStartScale + (1.0 - SignInFrontStartScale) * eased;
    if (scale >= 1.0) return;
    const QRectF frame = window->frameGeometry();
    data.setXScale(data.xScale() * scale);
    data.setYScale(data.yScale() * scale);
    data.setXTranslation(data.xTranslation() + frame.width() * (1.0 - scale) / 2.0);
    data.setYTranslation(data.yTranslation() + frame.height() * (1.0 - scale) / 2.0);
    mask |= PAINT_WINDOW_TRANSFORMED;
}
} // namespace Kadunce
