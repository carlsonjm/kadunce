#include "ActiveSettings.h"
#include "WindowStateRestore.h"
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
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
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
