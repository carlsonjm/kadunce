/* SPDX-License-Identifier: GPL-2.0-or-later */
// Table.qml loads, with every piece it can show filled in, and says nothing
// while it does. A QML mistake (a name clashing with an Item property, a type
// that does not exist) fails only as the effect loads it at sign-in, where it
// leaves Table empty; this check fails it at build time instead.
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QTimer>
#include <QVariantMap>
#include <cstdio>
#include <memory>

namespace {
QStringList heard;
void listen(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    // Only what a QML file says counts: a platform note is not Table's.
    if (type != QtDebugMsg && type != QtInfoMsg && message.contains(QStringLiteral(".qml:"))) heard.append(message);
    std::fprintf(stderr, "%s\n", qPrintable(message));
}

QVariantMap piece(double x, double y, double width, double height, const QVariantMap &more = {})
{
    QVariantMap map{{QStringLiteral("x"), x}, {QStringLiteral("y"), y},
                    {QStringLiteral("width"), width}, {QStringLiteral("height"), height},
                    {QStringLiteral("application"), QStringLiteral("Files")},
                    {QStringLiteral("title"), QStringLiteral("Home")}, {QStringLiteral("textWidth"), 80.0}};
    for (auto it = more.cbegin(); it != more.cend(); ++it) map.insert(it.key(), it.value());
    return map;
}
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    qInstallMessageHandler(listen);
    QQmlEngine engine;
    for (const QString &path : qEnvironmentVariable("KADUNCE_QML_IMPORT").split(u':', Qt::SkipEmptyParts))
        engine.addImportPath(path);
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(KADUNCE_TABLE_QML)));
    std::unique_ptr<QObject> table(component.create());
    if (!table) {
        // A build machine without Kirigami cannot load Table; a desktop always has it.
        if (component.errorString().contains(QStringLiteral("org.kde.kirigami\" is not installed"))) {
            std::printf("Table QML load check skipped: Kirigami is not installed here\n");
            return 77;
        }
        std::fprintf(stderr, "Table.qml did not load: %s\n", qPrintable(component.errorString()));
        return 1;
    }
    const QVariantMap stack = piece(20, 80, 180, 56, {{QStringLiteral("stacked"), 2}, {QStringLiteral("chosen"), true},
                                                      {QStringLiteral("hung"), true}});
    const QVariantMap aimed = piece(220, 80, 180, 56, {{QStringLiteral("aimed"), true}, {QStringLiteral("asleep"), true}});
    QVariantList members;
    for (int i = 0; i < 3; ++i)
        members.append(piece(10 + 150 * i, 160, 140, 56, {{QStringLiteral("place"), i + 1}, {QStringLiteral("count"), 3},
                                                         {QStringLiteral("front"), i == 0}, {QStringLiteral("chosen"), i == 1},
                                                         {QStringLiteral("asleep"), i == 2}}));
    const QVariantMap model{
        {QStringLiteral("tabs"), QVariantList{piece(20, 20, 120, 32, {{QStringLiteral("number"), 1},
                                                                      {QStringLiteral("name"), QStringLiteral("Home")},
                                                                      {QStringLiteral("state"), QStringLiteral("locked")},
                                                                      {QStringLiteral("current"), true}})}},
        {QStringLiteral("plus"), piece(150, 20, 32, 32)},
        {QStringLiteral("cards"), QVariantList{stack, aimed}},
        {QStringLiteral("members"), members},
        {QStringLiteral("carried"), piece(240, 90, 180, 56)},
        {QStringLiteral("drop"), piece(240, 90, 180, 56, {{QStringLiteral("seq"), 1}, {QStringLiteral("toX"), 20.0},
                                                          {QStringLiteral("toY"), 80.0}})},
        {QStringLiteral("aimMs"), 250}, {QStringLiteral("carrying"), true}, {QStringLiteral("lean"), 0.5},
    };
    table->setProperty("width", 1200);
    table->setProperty("height", 800);
    table->setProperty("shown", true);
    table->setProperty("model", model);
    QTimer::singleShot(400, &app, [&table, model] {
        QVariantMap armed = model;
        QVariantList cards = model.value(QStringLiteral("cards")).toList();
        QVariantMap second = cards[1].toMap();
        second[QStringLiteral("armed")] = true;
        cards[1] = second;
        armed[QStringLiteral("cards")] = cards;
        table->setProperty("model", armed);
    });
    QTimer::singleShot(800, &app, &QCoreApplication::quit);
    app.exec();
    if (!heard.isEmpty()) {
        std::fprintf(stderr, "Table.qml spoke %lld times while showing\n", qlonglong(heard.size()));
        return 1;
    }
    std::printf("Table QML load checks passed\n");
    return 0;
}
