#include "ActiveSettings.h"
#include "CarryRow.h"
#include "PaneStackAim.h"
#include "SpreadLayout.h"
#include "DisplayHandoffPolicy.h"
#include "WindowStateRestore.h"
#include "NativePlacement.h"
#include "DeferredCommandGuard.h"
#include "RestoreOutputPlan.h"
#include "NativeEdgePolicy.h"
#include "KeyboardOverlayPolicy.h"
#include "PerOutputDesktopsPolicy.h"
#include "KeyboardRoom.h"
#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
#include <cstdlib>
#include <source_location>
using namespace Qt::StringLiterals;

void require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) { fprintf(stderr, "Window handling assertion failed at line %u\n", location.line()); std::exit(1); }
}
struct Snapshot {
    KWin::RectF floatingGeometry{10, 20, 500, 400};
    KWin::RectF fullscreenRestoreGeometry{30, 40, 600, 450};
    KWin::QuickTileMode quickTileMode{};
    KWin::MaximizeMode maximizeMode = KWin::MaximizeFull;
    bool fullScreen = true;
};
struct Client {
    QStringList calls;
    KWin::RectF maximizeRestore;
    bool isFullScreen() const { return true; }
    KWin::MaximizeMode maximizeMode() const { return KWin::MaximizeFull; }
    KWin::QuickTileMode quickTileMode() const { return {}; }
    KWin::RectF frameGeometry() const { return {0, 0, 1000, 800}; }
    void setFullScreen(bool value) { calls << (value ? u"fullscreen-on"_s : u"fullscreen-off"_s); }
    void maximize(KWin::MaximizeMode value, const KWin::RectF &rect = {}) {
        calls << (value == KWin::MaximizeRestore ? u"unmaximize"_s : u"maximize"_s);
        maximizeRestore = rect;
    }
    void setQuickTileMode(KWin::QuickTileMode, const QPointF &) { calls << u"tile"_s; }
    void moveResize(const KWin::RectF &) { calls << u"geometry"_s; }
    void setGeometryRestore(const KWin::RectF &) { calls << u"floating-restore"_s; }
    void setFullscreenGeometryRestore(const KWin::RectF &) { calls << u"fullscreen-restore"_s; }
    void setMinimized(bool value) { calls << (value ? u"minimize"_s : u"unminimize"_s); }
};
struct PlacementClient : Client {
    int steps = 0;
    int invalidateAt = 0;
    Kadunce::DeferredCommandGuard *guard;
    void hit() { if (++steps == invalidateAt) guard->invalidate(); }
    void sendToOutput(int *) { hit(); }
    void setMinimized(bool) { hit(); }
    void setFullScreen(bool) { hit(); }
    void maximize(KWin::MaximizeMode) { hit(); }
    KWin::QuickTileMode quickTileMode() const { return KWin::QuickTileFlag::Left; }
    void setQuickTileMode(KWin::QuickTileMode, const QPointF &) { hit(); }
    void moveResize(const KWin::RectF &) { hit(); }
};
struct RestoreClient : Client {
    int steps = 0, invalidateAt = 0;
    bool alive = true;
    void hit() { if (++steps == invalidateAt) alive = false; }
    void setFullScreen(bool) { hit(); }
    void maximize(KWin::MaximizeMode, const KWin::RectF & = {}) { hit(); }
    void setMinimized(bool) { hit(); }
    void moveResize(const KWin::RectF &) { hit(); }
    void setGeometryRestore(const KWin::RectF &) { hit(); }
    void setFullscreenGeometryRestore(const KWin::RectF &) { hit(); }
};
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    struct EdgeOptions {
        bool tiling = true, maximizing = true;
        bool electricBorderTiling() const { return tiling; }
        bool electricBorderMaximize() const { return maximizing; }
        void setElectricBorderTiling(bool value) { tiling = value; }
        void setElectricBorderMaximize(bool value) { maximizing = value; }
    };
    for (bool tiling : {false, true}) for (bool maximizing : {false, true}) {
        EdgeOptions options{tiling, maximizing};
        {
            Kadunce::NativeEdgePolicy guard(&options);
            require(!options.tiling && !options.maximizing);
        }
        require(options.tiling == tiling && options.maximizing == maximizing);
        {
            Kadunce::NativeEdgePolicy guard(&options);
            options.tiling = !tiling; options.maximizing = !maximizing;
            guard.refresh();
            require(!options.tiling && !options.maximizing);
        }
        require(options.tiling == !tiling && options.maximizing == !maximizing);
    }
    struct KeyboardOptions {
        bool overlay = false;
        bool overlayVirtualKeyboardOnWindows() const { return overlay; }
        void setOverlayVirtualKeyboardOnWindows(bool value) { overlay = value; }
    };
    for (bool overlay : {false, true}) {
        KeyboardOptions options{overlay};
        {
            Kadunce::KeyboardOverlayPolicy guard(&options);
            require(options.overlay);
        }
        require(options.overlay == overlay);
        {
            Kadunce::KeyboardOverlayPolicy guard(&options);
            options.overlay = !overlay;
            guard.refresh();
            require(options.overlay);
        }
        require(options.overlay == !overlay);
    }
    struct DesktopOptions {
        bool perOutput = false;
        bool isPerOutputVirtualDesktops() const { return perOutput; }
        void setPerOutputVirtualDesktops(bool value) { perOutput = value; }
    };
    for (bool perOutput : {false, true}) {
        DesktopOptions options{perOutput};
        {
            Kadunce::PerOutputDesktopsPolicy guard(&options);
            require(!options.perOutput);
        }
        require(options.perOutput == perOutput);
        {
            Kadunce::PerOutputDesktopsPolicy guard(&options);
            options.perOutput = !perOutput;
            guard.refresh();
            require(!options.perOutput);
        }
        require(options.perOutput == !perOutput);
    }
    // A card the keys do not reach keeps its height; one they reach ends a
    // gutter above them, and the room never grows a card past its own height
    // or below nothing.
    const auto room = [](double top, double height, double keys) {
        return Kadunce::keyboardRoomHeight(top, height, keys, 10);
    };
    require(room(10, 780, 900) == 780);
    require(room(10, 780, 800) == 780);
    require(room(10, 780, 797) == 777);
    require(room(10, 780, 397) == 377);
    require(room(10, 780, 15) == 0);
    // A client that cannot be that short rises instead, by what its least
    // height leaves over; one that fits, or that the keys do not reach, stays.
    require(Kadunce::keyboardRoomTop(10, 377, 397, 10) == 10);
    require(Kadunce::keyboardRoomTop(10, 780, 900, 10) == 10);
    require(Kadunce::keyboardRoomTop(10, 600, 397, 10) == -213);
    // A client is asked for a size once a motion, not on every frame of it:
    // all the room the keys give back at once, the room they take once they
    // rest, and nothing while they rise or stand still.
    const auto ask = Kadunce::keyboardRoomAsk;
    require(!ask(780, 600, std::nullopt, 780, false));
    require(!ask(377, 377, std::nullopt, 780, false));
    require(ask(377, 400, std::nullopt, 780, false) == 780);
    require(ask(377, 400, 780, 780, false) == 780);
    require(ask(377, 400, 500, 780, false) == 500);
    require(!ask(377, 520, 500, 780, false));
    require(!ask(377, 381, 377, 780, false));
    require(ask(377, 520, 600, 780, false) == 600);
    require(ask(780, 377, std::nullopt, 780, true) == 377);
    require(ask(377, 450, std::nullopt, 780, true) == 450);
    require(!ask(377, 377, 780, 780, true));
    // Rising keys ask their client its drawing time before they rest, so it
    // lands with them; one not yet timed is asked halfway, and keys that
    // arrive at once ask at once.
    const auto askDelay = Kadunce::keyboardRoomAskDelay;
    require(askDelay(240, std::nullopt) == 120);
    require(askDelay(240, 30) == 210);
    require(askDelay(240, 220) == 20);
    require(askDelay(240, 400) == 0);
    require(askDelay(240, -5) == 240);
    require(askDelay(0, 220) == 0);
    require(askDelay(-1, std::nullopt) == 0);
    require(Kadunce::keyboardRoomDrawTime(std::nullopt, 220) == 220);
    require(Kadunce::keyboardRoomDrawTime(200, 100) == 150);
    require(Kadunce::keyboardRoomDrawTime(std::nullopt, -3) == 0);
    using Kadunce::RestoreResult;
    const QList<int> outputs{1,2,3};
    QList<int> attempted;
    require(Kadunce::restoreOnSurvivingOutput(outputs, [](int id) { return id != 2; },
        [&](int id) { attempted.append(id); return id == 1 ? RestoreResult::OutputLost : RestoreResult::Completed; })
        == RestoreResult::Completed);
    require(attempted == QList<int>{1,3});
    attempted.clear();
    require(Kadunce::restoreOnSurvivingOutput(outputs, [](int) { return true; },
        [&](int id) { attempted.append(id); return RestoreResult::Aborted; }) == RestoreResult::Aborted);
    require(attempted == QList<int>{1});
    attempted.clear();
    require(Kadunce::restoreOnSurvivingOutput(outputs, [](int) { return true; },
        [&](int id) { attempted.append(id); return RestoreResult::OutputLost; }) == RestoreResult::OutputLost);
    require(attempted == outputs);
    require(Kadunce::restoreOnSurvivingOutput(outputs, [](int) { return false; },
        [&](int) { require(false); return RestoreResult::Completed; }) == RestoreResult::OutputLost);
    for (int stop = 1; stop <= 9; ++stop) {
        RestoreClient client; client.invalidateAt = stop;
        require(!Kadunce::restoreWindowStateChecked(&client, Snapshot{}, {0,0,500,400},
            true, true, true, [&] { return client.alive; }));
        require(client.steps == stop);
    }
    RestoreClient complete;
    require(Kadunce::restoreWindowStateChecked(&complete, Snapshot{}, {0,0,500,400},
        true, true, true, [&] { return complete.alive; }));
    require(complete.steps == 9);
    // Inject cancellation at each real placement boundary. No later setter
    // may run; a fresh generation must still complete normally afterward.
    for (int stop = 1; stop <= 6; ++stop) {
        Kadunce::DeferredCommandGuard guard;
        const auto token = guard.issue();
        PlacementClient client;
        client.guard = &guard; client.invalidateAt = stop;
        int output = 0;
        require(!Kadunce::applyNativePlacement(&client, &output, {0,0,500,400},
            [&] { return guard.accepts(token); }));
        require(client.steps == stop);
        const auto fresh = guard.issue();
        client.steps = 0; client.invalidateAt = 0;
        require(Kadunce::applyNativePlacement(&client, &output, {0,0,500,400},
            [&] { return guard.accepts(fresh); }));
        require(client.steps == 6);
        client.steps = 0;
        require(!Kadunce::applyNativePlacement(&client, &output, {0,0,500,400}, [] { return false; }));
        require(client.steps == 0);
    }
    using Kadunce::CardPaintRoute;
    using Kadunce::cardPaintRoute;
    for (bool admitted : {false, true}) {
        require(cardPaintRoute(false, true, admitted, false) == CardPaintRoute::Hidden);
        require(cardPaintRoute(true, false, admitted, false) == CardPaintRoute::Hidden);
        require(cardPaintRoute(false, false, admitted, false) == CardPaintRoute::Native);
        for (bool outputIsTablet : {false, true}) {
            for (bool ownedByTablet : {false, true}) {
                require(cardPaintRoute(outputIsTablet, ownedByTablet, admitted, true)
                        == CardPaintRoute::Native);
            }
        }
    }
    require(cardPaintRoute(true, true, true, false) == CardPaintRoute::Card);
    require(cardPaintRoute(true, true, false, false) == CardPaintRoute::Hidden);
    for (bool paintingTablet : {false, true}) {
        require(cardPaintRoute(paintingTablet, true, true, false, true)
                == CardPaintRoute::Card);
        require(cardPaintRoute(paintingTablet, true, false, false, true)
                == CardPaintRoute::Hidden);
    }
    if (app.arguments().size() == 5) {
        auto config = KSharedConfig::openConfig(app.arguments()[2], KConfig::SimpleConfig);
        KConfigGroup(config, QStringLiteral("Effect-kadunce")).writeEntry(
            app.arguments()[3].toUtf8().constData(), app.arguments()[4], KConfig::Notify);
        config->sync();
        QTest::qWait(100);
        return 0;
    }
    if (app.arguments().size() == 4) {
        auto config = KSharedConfig::openConfig(app.arguments()[2], KConfig::SimpleConfig);
        KConfigGroup(config, QStringLiteral("Effect-kadunce")).writeEntry(
            "ActiveCardGutter", app.arguments()[3].toInt(), KConfig::Notify);
        config->sync();
        QTest::qWait(100); // Let the writer's queued bus notification leave the process.
        return 0;
    }
    Snapshot snapshot;
    const KWin::RectF destination{0, 0, 1000, 800};
    Client active;
    Kadunce::restoreWindowState(&active, snapshot, destination, true);
    require(active.calls == QStringList{u"fullscreen-off"_s, u"unmaximize"_s, u"geometry"_s,
        u"maximize"_s, u"fullscreen-on"_s, u"floating-restore"_s, u"fullscreen-restore"_s});
    require(active.maximizeRestore == snapshot.floatingGeometry);
    Client removedOutput;
    Kadunce::restoreWindowState(&removedOutput, snapshot, destination, false, true, true);
    require(removedOutput.calls == QStringList{u"fullscreen-off"_s, u"unmaximize"_s, u"unminimize"_s,
        u"geometry"_s, u"maximize"_s, u"fullscreen-on"_s, u"minimize"_s});
    require(removedOutput.maximizeRestore == destination);
    snapshot.quickTileMode = KWin::QuickTileFlag::Left;
    Client tiled;
    Kadunce::restoreWindowState(&tiled, snapshot, destination, true);
    require(tiled.calls.contains(u"tile"_s) && !tiled.calls.contains(u"maximize"_s));

    QTemporaryDir directory;
    require(directory.isValid());
    // KConfigWatcher watches named configs, not absolute paths. Isolate the
    // standard config directory while using the same naming as production.
    qputenv("XDG_CONFIG_HOME", directory.path().toUtf8());
    const QString path = QStringLiteral("kadunce-test-settings");
    Kadunce::ActiveSettings settings(KSharedConfig::openConfig(path, KConfig::SimpleConfig));
    require(settings.gutter() == 10);
    // No gutter set: Bento's panes keep their own spacing.
    require(Kadunce::bentoPaneGap() == Kadunce::DefaultBentoPaneGap);
    int changes = 0;
    settings.onGutterChanged([&changes] { ++changes; });
    QTest::qWait(50);
    for (int value : {36, 0, 200, 10}) {
        const int before = changes;
        require(QProcess::execute(app.applicationFilePath(),
            {QStringLiteral("--write"), path, QString::number(value)}) == 0);
        for (int attempt = 0; attempt < 100 && settings.gutter() != std::clamp(value, 4, 48); ++attempt)
            QTest::qWait(10);
        require(settings.gutter() == std::clamp(value, 4, 48));
        // Set, it is the space between Bento's panes too, and announced, even
        // where the gutter itself stays 10.
        require(Kadunce::bentoPaneGap() == std::clamp(value, 4, 48));
        for (int attempt = 0; attempt < 100 && changes == before; ++attempt) QTest::qWait(10);
        require(changes > before);
    }
    require(Kadunce::makePixelBentoLayout({{0.0, 0.0, 0.5, 1.0}, {0.5, 0.0, 0.5, 1.0}}, 0, 0, 1000, 800)[1].x
            - 500 == 5);

    // Nothing tuned, every card setting stands at Kadunce's own.
    require(Kadunce::cardTuning() == Kadunce::CardTuning{});
    require(Kadunce::carryJoinDwell() == Kadunce::CarryJoinDwell);
    require(Kadunce::paneStackDwell() == Kadunce::PaneStackDwell);
    require(std::abs(Kadunce::spreadGutterShare(Kadunce::cardTuning().spreadPeek) - 0.056) < 1e-9);
    int tunings = 0;
    settings.onTuningChanged([&tunings] { ++tunings; });
    const auto tune = [&](const char *key, const char *value, auto reached) {
        const int before = tunings;
        require(QProcess::execute(app.applicationFilePath(),
            {QStringLiteral("--write-key"), path, QString::fromUtf8(key), QString::fromUtf8(value)}) == 0);
        for (int attempt = 0; attempt < 100 && !reached(); ++attempt) QTest::qWait(10);
        require(reached());
        require(tunings > before);
    };
    tune("CardCornerRadius", "12", [] { return Kadunce::cardTuning().cornerRadius == 12.0; });
    tune("CardCornerRadius", "40", [] { return Kadunce::cardTuning().cornerRadius == 15.0; });
    tune("SleepingDimmed", "false", [] { return Kadunce::cardTuning().sleepingDimmed == std::optional<bool>(false); });
    tune("StackPauseMs", "900", [] { return Kadunce::cardTuning().stackPauseMs == 800; });
    // Spread's own shorter rest keeps its share of the pause.
    require(Kadunce::paneStackDwell() == 800);
    require(Kadunce::carryJoinDwell() == std::lround(Kadunce::CarryJoinDwell * 800.0 / 450.0));
    tune("StackBehind", "none", [] { return !Kadunce::cardTuning().stackBehindShown; });
    require(!Kadunce::makeOpenStackPose(0, 3, 1, 400).visible && Kadunce::makeOpenStackPose(1, 3, 1, 400).visible);
    require(!Kadunce::makeClosedStackPose(0, 3, 1024).visible && Kadunce::makeClosedStackPose(2, 3, 1024).visible);
    tune("SpreadPeek", "1", [] { return Kadunce::cardTuning().spreadPeek == 1.0; });
    // A wider peek narrows the space between Spread's cards, the cards
    // themselves keeping their size.
    const auto wide = Kadunce::makeSpreadLayout(0, 0, 1000, 800);
    require(std::abs(wide.gutter - 20.0) < 1e-9 && std::abs(wide.cards[1].width - 540.0) < 1e-9);
    tune("SpreadPeek", "0", [] { return Kadunce::cardTuning().spreadPeek == 0.0; });
    require(std::abs(Kadunce::makeSpreadLayout(0, 0, 1000, 800).gutter - 120.0) < 1e-9);
    // Unset, no card casts a shadow; set, it reaches furthest below the card.
    require(!Kadunce::cardShadowShape(Kadunce::CardTuning{}.shadow).cast());
    tune("CardShadow", "3", [] { return Kadunce::cardTuning().shadow == 1.0; });
    const auto deep = Kadunce::cardShadowShape(Kadunce::cardTuning().shadow);
    require(deep.cast() && deep.reachBelow() > deep.reachSides() && deep.reachSides() > deep.reachAbove());
    tune("CardShadow", "0.5", [] { return Kadunce::cardTuning().shadow == 0.5; });
    require(Kadunce::cardShadowShape(0.5).opacity < deep.opacity);

    // The keys installed for a settings program are the keys read.
    QFile installed(QStringLiteral(KADUNCE_SETTINGS_KEYS));
    require(installed.open(QIODevice::ReadOnly | QIODevice::Text));
    require(QString::fromUtf8(installed.readAll()).split(u'\n', Qt::SkipEmptyParts) == Kadunce::settingsKeys());
    return 0;
}
