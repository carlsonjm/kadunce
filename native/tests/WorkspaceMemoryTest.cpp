/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of workspace memory: what a record keeps and leaves
// out, which sign-in's record is offered, how a bring-back falls back when a
// display or workspace is gone, how pairs come back together, and how sets of
// apps opened together are counted.
#include "WorkspaceMemory.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce::Memory;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

RememberedWindow card(const QString &app, const QString &output, const QString &desktop, int order,
                      bool active = false)
{
    RememberedWindow window;
    window.appId = app;
    window.output = output;
    window.desktopId = desktop;
    window.role = Role::Card;
    window.order = order;
    window.size = QStringLiteral("full");
    window.active = active;
    return window;
}

RememberedWindow pane(const QString &app, const QString &output, const QString &desktop, const QString &side)
{
    RememberedWindow window;
    window.appId = app;
    window.output = output;
    window.desktopId = desktop;
    window.role = Role::Pane;
    window.side = side;
    window.size = QStringLiteral("half");
    return window;
}

const QString Tablet = QStringLiteral("eDP-1");
const QString Monitor = QStringLiteral("DP-2");
const QString One = QStringLiteral("desk-1");
const QString Two = QStringLiteral("desk-2");
} // namespace

int main()
{
    // Size classes and sides read from geometry.
    {
        require(sizeClass(600, 1920) == QLatin1String("third"), "a third");
        require(sizeClass(960, 1920) == QLatin1String("half"), "a half");
        require(sizeClass(1280, 1920) == QLatin1String("twoThirds"), "two thirds");
        require(sizeClass(1920, 1920) == QLatin1String("full"), "full");
        require(sizeClass(100, 0) == QLatin1String("full"), "no display reads as full");
        const QRectF area(0, 0, 1920, 1080);
        require(paneSide(QRectF(0, 0, 960, 1080), area) == QLatin1String("left"), "left pane");
        require(paneSide(QRectF(960, 0, 960, 1080), area) == QLatin1String("right"), "right pane");
        require(paneSide(QRectF(0, 0, 1920, 540), area) == QLatin1String("top"), "top pane");
        require(paneSide(QRectF(0, 0, 1920, 1080), area) == QLatin1String("full"), "full pane");
    }

    // A record keeps only what places a window, and reads back the same.
    {
        RememberedSession session;
        session.sessionId = QStringLiteral("3");
        session.savedAt = 1700000000;
        session.clean = true;
        session.windows = {card(QStringLiteral("org.kde.dolphin"), Tablet, One, 1, true),
                           pane(QStringLiteral("firefox"), Monitor, Two, QStringLiteral("left"))};
        MemoryFile file;
        file.current = session;
        const QByteArray bytes = file.encode();
        for (const char *word : {"title", "caption", "name", "path", "document"})
            require(!bytes.contains(word), "nothing names a window's contents or a workspace");
        const MemoryFile back = MemoryFile::decode(bytes);
        require(back.current == session, "the record reads back as written");
        require(!back.offered, "nothing is offered until another sign-in begins");
        require(MemoryFile::decode("{\"version\":99}").current.windows.isEmpty(),
                "a newer file is treated as none");
        require(MemoryFile::decode("not json").current.windows.isEmpty(), "a broken file is treated as none");
    }

    // Each app once, by its desktop id.
    {
        RememberedSession session;
        session.windows = {card(QStringLiteral("org.kde.kate"), Tablet, One, 1),
                           card(QStringLiteral("org.kde.kate.desktop"), Tablet, One, 2),
                           card(QStringLiteral("firefox"), Tablet, One, 3)};
        require(session.apps() == QStringList({QStringLiteral("org.kde.kate"), QStringLiteral("firefox")}),
                "two windows of one app count once");
    }

    // A new sign-in offers the last one's record; Cards switched off and on
    // in the same sign-in offers nothing new and keeps what it has.
    {
        MemoryFile file;
        file.begin(QStringLiteral("3"));
        require(!file.hasOffer(), "a first sign-in offers nothing");
        file.current.windows = {card(QStringLiteral("firefox"), Tablet, One, 1)};
        file.begin(QStringLiteral("3"));
        require(!file.hasOffer() && file.current.windows.size() == 1, "the same sign-in keeps its own record");
        file.begin(QStringLiteral("4"));
        require(file.hasOffer() && file.offered->windows.size() == 1, "the next sign-in offers the last record");
        require(file.current.windows.isEmpty() && file.current.sessionId == QLatin1String("4"),
                "and starts its own");
        file.begin(QStringLiteral("5"));
        require(!file.hasOffer(), "a sign-in with no apps offers nothing at the next");
    }

    // Plan: gone displays and workspaces fall back; running apps stay put.
    {
        RememberedSession session;
        session.windows = {card(QStringLiteral("org.kde.dolphin"), Monitor, Two, 1),
                           card(QStringLiteral("org.kde.konsole"), Tablet, QStringLiteral("gone"), 2, true),
                           card(QStringLiteral("firefox"), Tablet, One, 3)};
        const auto plan = planBringBack(session, {Tablet}, {One, Two}, Tablet, One,
                                        {QStringLiteral("firefox.desktop")});
        require(plan.launch == QStringList({QStringLiteral("org.kde.dolphin"), QStringLiteral("org.kde.konsole")}),
                "every app not running is launched, once");
        require(plan.alreadyOpen == QStringList({QStringLiteral("firefox")}), "a running app stays where it is");
        require(plan.steps.at(0).output == Tablet, "a display that is gone falls back to the one in use");
        require(plan.steps.at(0).desktopId == Two, "a workspace still there keeps its app");
        require(plan.steps.at(1).desktopId == One, "a workspace that is gone falls back to the current one");
        require(plan.steps.at(1).active, "the card in use is known");
        require(plan.desktops == QSet<QString>({One, Two}), "the workspaces it fills are kept");
    }

    // A pane whose partner does not come back stands as a card.
    {
        RememberedSession session;
        session.windows = {pane(QStringLiteral("org.kde.kate"), Tablet, One, QStringLiteral("left")),
                           pane(QStringLiteral("firefox"), Tablet, One, QStringLiteral("right"))};
        const auto plan = planBringBack(session, {Tablet}, {One}, Tablet, One, {QStringLiteral("firefox")});
        require(plan.steps.size() == 1 && plan.steps.at(0).role == Role::Card && plan.steps.at(0).group.isEmpty(),
                "a lone pane comes back as a card");
    }

    // A pair on a workspace not shown waits there as two cards.
    {
        RememberedSession session;
        session.windows = {pane(QStringLiteral("org.kde.kate"), Tablet, Two, QStringLiteral("left")),
                           pane(QStringLiteral("firefox"), Tablet, Two, QStringLiteral("right"))};
        const auto plan = planBringBack(session, {Tablet}, {One, Two}, Tablet, One, {});
        require(plan.steps.size() == 2 && plan.steps.at(0).role == Role::Card && plan.steps.at(1).role == Role::Card
                    && plan.steps.at(0).group.isEmpty() && plan.steps.at(0).desktopId == Two,
                "panes on another workspace come back there as cards");
    }

    // A run: cards go as they come; a pair waits for both, left first.
    {
        RememberedSession session;
        session.windows = {card(QStringLiteral("org.kde.dolphin"), Tablet, One, 1),
                           pane(QStringLiteral("firefox"), Tablet, One, QStringLiteral("right")),
                           pane(QStringLiteral("org.kde.kate"), Tablet, One, QStringLiteral("left"))};
        const auto plan = planBringBack(session, {Tablet}, {One}, Tablet, One, {});
        BringBackRun run(QStringLiteral("t1"), plan.steps);
        require(!run.peek(QStringLiteral("org.kde.konsole")), "an app not remembered is not waited for");

        const auto dolphin = run.claim(QStringLiteral("org.kde.dolphin"));
        require(dolphin.has_value(), "a remembered app's first window is claimed");
        require(!run.claim(QStringLiteral("org.kde.dolphin")), "its second window is not");
        const auto ready = run.arrived(*dolphin, QStringLiteral("w1"));
        require(ready.size() == 1 && ready.at(0).asCard && ready.at(0).windowId == QLatin1String("w1"),
                "a card goes at once");

        const auto firefox = run.claim(QStringLiteral("firefox"));
        const auto kate = run.claim(QStringLiteral("org.kde.kate"));
        require(firefox && kate, "both panes are claimed");
        require(run.arrived(*firefox, QStringLiteral("w2")).isEmpty(),
                "a pane waits for its partner, even one already claimed");
        const auto pair = run.arrived(*kate, QStringLiteral("w3"));
        require(pair.size() == 2, "the pair goes together");
        require(pair.at(0).step.side == QLatin1String("left") && pair.at(0).asCard,
                "the left pane goes first and stands alone");
        require(pair.at(1).step.side == QLatin1String("right") && !pair.at(1).asCard,
                "the right pane pairs with it");
        for (int i = 0; i < 3; ++i) run.notePlaced(true);
        require(run.finished() && run.placed() == 3 && run.missed() == 0, "the run ends with every app back");
        require(run.desktops().isEmpty(), "and keeps no workspace any longer");
    }

    // A run that times out: what never came is missed, a pane held for a
    // partner that never came stands as a card.
    {
        RememberedSession session;
        session.windows = {pane(QStringLiteral("org.kde.kate"), Tablet, Two, QStringLiteral("left")),
                           pane(QStringLiteral("firefox"), Tablet, Two, QStringLiteral("right")),
                           card(QStringLiteral("org.kde.dolphin"), Tablet, One, 1)};
        const auto plan = planBringBack(session, {Tablet}, {One, Two}, Tablet, Two, {});
        BringBackRun run(QStringLiteral("t2"), plan.steps);
        require(run.desktops() == QSet<QString>({One, Two}), "workspaces still awaited are kept");
        const auto firefox = run.claim(QStringLiteral("firefox"));
        require(run.arrived(*firefox, QStringLiteral("w2")).isEmpty(), "the pane waits");
        const auto rest = run.expire();
        require(rest.size() == 1 && rest.at(0).asCard, "the lone pane stands as a card");
        run.notePlaced(true);
        require(run.finished() && run.placed() == 1 && run.missed() == 2, "two apps never came");
    }

    // A claimed window that could not be admitted counts as missed.
    {
        RememberedSession session;
        session.windows = {card(QStringLiteral("org.kde.dolphin"), Tablet, One, 1)};
        BringBackRun run(QStringLiteral("t3"), planBringBack(session, {Tablet}, {One}, Tablet, One, {}).steps);
        const auto dolphin = run.claim(QStringLiteral("org.kde.dolphin"));
        run.lost(*dolphin);
        require(run.finished() && run.missed() == 1, "a lost window is missed");
    }

    // Each place is aimed where the placement path reads it back.
    {
        const QRectF tablet(0, 0, 1920, 1200);
        const QRectF monitor(1920, 0, 2560, 1440);
        const QList<Kadunce::PlacementOutput> outputs{{Tablet, tablet, true}, {Monitor, monitor, false}};
        const auto aimFor = [&](const QString &side, bool asCard, const QRectF &area, bool owns) {
            ReadyPlacement ready;
            ready.step.role = side.isEmpty() ? Role::Card : Role::Pane;
            ready.step.side = side;
            ready.asCard = asCard;
            return Kadunce::placementAim(outputs, bringBackPoint(ready, area, owns));
        };
        using Kadunce::PlacementAimKind;
        require(aimFor({}, true, tablet, true).kind == PlacementAimKind::Card, "a card comes back as a card");
        require(aimFor(QStringLiteral("left"), false, tablet, true).kind == PlacementAimKind::Left,
                "a left pane pairs on the left");
        require(aimFor(QStringLiteral("right"), false, tablet, true).kind == PlacementAimKind::Right,
                "a right pane pairs on the right");
        require(aimFor(QStringLiteral("left"), true, tablet, true).kind == PlacementAimKind::Card,
                "the first of a pair stands alone first");
        require(aimFor(QStringLiteral("top"), false, tablet, true).kind == PlacementAimKind::Card,
                "a top pane on the card display comes back as a card");
        require(aimFor(QStringLiteral("top"), false, monitor, false).kind == PlacementAimKind::Top,
                "a top pane on a monitor takes the top");
        require(aimFor({}, true, monitor, false).kind == PlacementAimKind::Display
                    && aimFor({}, true, monitor, false).output == Monitor,
                "a window on a monitor opens there");
    }

    // Apps opened together after sign-in.
    {
        SignInGathering gathering;
        gathering.begin(true);
        gathering.noteOpened(QStringLiteral("firefox"), 0);
        require(!gathering.gathering() && gathering.close().isEmpty(),
                "Cards switched on with windows open gathers nothing");

        gathering.begin(false);
        gathering.noteOpened(QStringLiteral("org.kde.kate.desktop"), 10'000);
        gathering.noteOpened(QStringLiteral("firefox"), 20'000);
        gathering.noteOpened(QStringLiteral("firefox"), 30'000);
        gathering.noteOpened(QStringLiteral("org.kde.dolphin"), 10'000 + SignInGathering::SpanMs + 1);
        require(!gathering.due(10'000 + SignInGathering::SpanMs), "it gathers for the whole span");
        require(gathering.due(10'000 + SignInGathering::SpanMs + 1), "and then it is due");
        const QStringList set = gathering.close();
        require(set == QStringList({QStringLiteral("firefox"), QStringLiteral("org.kde.kate")}),
                "the set is the apps opened within the span, each once, sorted");

        gathering.begin(false);
        gathering.noteOpened(QStringLiteral("firefox"), 0);
        require(gathering.close().isEmpty(), "one app alone is no set");

        OpenTogetherLog log;
        log.add(set, 100);
        log.add({QStringLiteral("org.kde.kate"), QStringLiteral("firefox.desktop")}, 200);
        log.add({QStringLiteral("firefox")}, 300);
        require(log.sets().size() == 1 && log.sets().at(0).count == 2 && log.sets().at(0).lastSeen == 200,
                "the same set again counts twice, whatever order it came in");
        const OpenTogetherLog back = OpenTogetherLog::decode(log.encode());
        require(back.sets().size() == 1 && back.sets().at(0).count == 2
                    && back.sets().at(0).apps == log.sets().at(0).apps,
                "counts read back as written");
        for (int i = 0; i < OpenTogetherLog::MaximumSets + 4; ++i)
            log.add({QStringLiteral("a%1").arg(i), QStringLiteral("b")}, 1000 + i);
        require(log.sets().size() == OpenTogetherLog::MaximumSets, "only so many sets are kept");
        require(std::none_of(log.sets().cbegin(), log.sets().cend(),
                             [](const auto &s) { return s.lastSeen == 200; }),
                "the least recently seen goes first");
    }

    std::cout << "workspace-memory: ok\n";
    return 0;
}
