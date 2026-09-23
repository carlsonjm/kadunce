#include "ActiveSettings.h"
#include "DisplayHandoffPolicy.h"
#include "WindowStateRestore.h"
#include "NativePlacement.h"
#include "DeferredCommandGuard.h"
#include "RestoreOutputPlan.h"
#include "NativeEdgePolicy.h"
#include "KeyboardOverlayPolicy.h"
#include "KeyboardReveal.h"
#include "KeyboardTap.h"
#include <QCoreApplication>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
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
    // A cursor already a gutter clear of the keys moves nothing; one the keys
    // cover rises exactly to a gutter above them, however far that takes the
    // card's own top off the display; the cursor itself never leaves it.
    const auto lift = [](double top, double bottom, double keys, double displayTop) {
        return Kadunce::keyboardRevealLift(top, bottom, keys, 10, displayTop);
    };
    require(lift(361, 380, 397, 0) == 0);
    require(lift(368, 387, 397, 0) == 0);
    require(lift(751, 770, 397, 0) == 383);
    require(lift(751, 770, 397, 500) == 251);
    require(lift(-5, 770, 397, 0) == 0);
    // A card gives up exactly the height that puts its bottom a gutter above
    // the keys, nothing when the keys miss it, and always keeps a gutter.
    require(Kadunce::keyboardRoom(10, 838, 500, 10) == 348);
    require(Kadunce::keyboardRoom(10, 480, 500, 10) == 0);
    require(Kadunce::keyboardRoom(10, 838, 15, 10) == 818);
    {
        using namespace std::chrono_literals;
        Kadunce::KeyboardTap tap;
        tap.down(1, {100, 100}, 0us);
        tap.motion(1, {108, 104});
        const auto at = tap.up(1, 200ms);
        require(at && *at == QPointF(108, 104));
        tap.down(1, {100, 100}, 0us);
        tap.motion(1, {100, 140});
        require(!tap.up(1, 100ms));
        tap.down(1, {100, 100}, 0us);
        require(!tap.up(1, 800ms));
        tap.down(1, {100, 100}, 0us);
        tap.down(2, {300, 100}, 10ms);
        require(!tap.up(2, 50ms));
        require(!tap.up(1, 60ms));
        tap.down(3, {50, 50}, 0us);
        require(tap.up(3, 50ms).has_value());
    }
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
    QTest::qWait(50);
    for (int value : {36, 0, 200}) {
        require(QProcess::execute(app.applicationFilePath(),
            {QStringLiteral("--write"), path, QString::number(value)}) == 0);
        for (int attempt = 0; attempt < 100 && settings.gutter() != std::clamp(value, 6, 48); ++attempt)
            QTest::qWait(10);
        require(settings.gutter() == std::clamp(value, 6, 48));
    }
    return 0;
}
