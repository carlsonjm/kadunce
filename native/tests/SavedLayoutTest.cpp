/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of saved layouts: how a desk reads as groups, how a
// requested layout is cleaned up and matched against the windows open, how
// launched windows are claimed once, and the order the row is arranged in.
#include "SavedLayout.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce::SavedLayouts;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

QString s(const char *text) { return QString::fromLatin1(text); }

QVariantMap requested(const QStringList &apps, bool stack)
{
    return {{s("apps"), apps}, {s("stack"), stack}};
}
} // namespace

int main()
{
    // The desk reads entry by entry; each app once, in its first group.
    {
        const auto groups = deskGroups({
            {s("com.mitchellh.ghostty"), s("claude")},
            {s("zen")},
            {s("")},
            {s("zen"), s("org.kde.kate")},
            {s("claude.desktop"), s("")},
        });
        require(groups.size() == 3, "an empty entry and an entry of apps already seen are left out");
        require(groups.at(0) == Group{{s("com.mitchellh.ghostty"), s("claude")}, true}, "a Stack front first");
        require(groups.at(1) == Group{{s("zen")}, false}, "a lone card");
        require(groups.at(2) == Group{{s("org.kde.kate")}, false},
                "a Stack left with one new app stands alone");
        require(toVariant(groups.at(0)).value(s("stack")).toBool()
                    && toVariant(groups.at(0)).value(s("apps")).toStringList().size() == 2,
                "a group reads out as apps and stack");
    }

    // A request is cleaned: blanks and repeats go, a group of several apps
    // that is not a Stack stands each alone.
    {
        const auto groups = groupsFrom({
            requested({s("ghostty"), s(" claude "), s("ghostty")}, true),
            requested({s("zen"), s("org.kde.kate")}, false),
            requested({s(""), s("claude")}, true),
            requested({s("mail")}, true),
            {},
        });
        require(groups.size() == 4, "four groups remain");
        require(groups.at(0) == Group{{s("ghostty"), s("claude")}, true}, "a Stack keeps its order");
        require(groups.at(1) == Group{{s("zen")}, false} && groups.at(2) == Group{{s("org.kde.kate")}, false},
                "a group that is not a Stack stands each app alone");
        require(groups.at(3) == Group{{s("mail")}, false}, "a Stack of one stands alone");
        require(groupsFrom({requested({}, true)}).isEmpty(), "a request naming no app is empty");

        QStringList many;
        for (int i = 0; i < 80; ++i) many.append(s("app") + QString::number(i));
        int apps = 0;
        for (const auto &group : groupsFrom({requested(many, true)})) apps += int(group.apps.size());
        require(apps == MaximumApps, "a request is cut at the most a desk holds");
    }

    // Matching: an app open anywhere uses its front-most window; the rest
    // are launched.
    {
        const QList<Group> groups{{{s("ghostty"), s("claude")}, true}, {{s("zen")}, false}};
        const auto plan = planArrangement(groups, {
            {s("claude.desktop"), s("w-front")},
            {s("claude"), s("w-back")},
            {s("firefox"), s("w-other")},
        });
        require(plan.launch == QStringList{s("ghostty"), s("zen")}, "apps not running are launched");
        require(plan.alreadyOpen == QStringList{s("claude")}, "an app running is not");
        require(plan.places.size() == 3, "one slot per app");
        require(plan.places.at(1).windowId == QLatin1String("w-front")
                    && plan.places.at(1).state == Slot::State::Ready,
                "the front-most window of an open app goes to its place at once");
        require(plan.places.at(0).state == Slot::State::Waiting && plan.places.at(2).group == 1,
                "launched apps wait, each in its group");
    }

    // A run: launched windows are claimed once, placed, and the row comes
    // out group by group with each Stack front first.
    {
        const QList<Group> groups{
            {{s("ghostty"), s("claude")}, true},
            {{s("zen")}, false},
            {{s("mail")}, false},
        };
        LayoutRun run(s("t1"), planArrangement(groups, {{s("claude"), s("w-claude")}}));
        require(run.token() == QLatin1String("t1"), "the run keeps its token");
        require(run.ready().size() == 1, "the open app is ready at once");
        require(!run.claim(s("claude"), s("w-claude-2")), "an app already placed claims no second window");
        require(run.claim(s("org.mitchellh.ghostty"), s("w-ghost")) == false, "another app's window is not claimed");
        require(run.claim(s("ghostty.desktop"), s("w-ghost")), "the launched app's first window is claimed");
        require(!run.claim(s("ghostty"), s("w-ghost-2")), "and its second window is not");
        require(!run.claim(s("zen"), s("w-ghost")), "a window stands for one app only");
        require(run.ready().size() == 1, "a claimed window waits to be admitted");
        require(run.admitted(s("w-ghost")) && run.ready().size() == 2, "then it is ready");
        require(!run.admitted(s("w-ghost")), "admitted once");

        require(run.settle(s("w-ghost"), true), "ghostty is placed");
        require(!run.settle(s("w-ghost"), true), "and reported once");
        require(run.entries() == QList<QStringList>{{s("w-ghost")}}, "a Stack with one window so far is an entry of one");
        require(run.settle(s("w-claude"), true), "claude is placed");
        require(run.entries() == QList<QStringList>{{s("w-ghost"), s("w-claude")}},
                "the Stack forms in the listed order, whichever came first");
        require(!run.finished(), "zen and mail are still waited for");

        require(run.claim(s("zen"), s("w-zen")) && run.admitted(s("w-zen")), "zen arrives");
        run.expire();
        require(run.ready().size() == 1, "a window already ready still goes to its place after the wait");
        require(!run.claim(s("mail"), s("w-mail")), "nothing is claimed after the wait");
        require(run.settle(s("w-zen"), false), "zen could not be placed");
        require(run.finished(), "the run is over");
        require(run.placed() == 2 && run.missed() == 2, "two placed; one never came and one failed");
        require(run.entries() == QList<QStringList>{{s("w-ghost"), s("w-claude")}}, "only placed windows are arranged");
        require(run.slotFor(s("w-zen"))->appId == QLatin1String("zen"), "a window names its app");
    }

    // Lone apps keep the listed order; a run with everything open ends
    // as soon as every window has gone to its place.
    {
        const QList<Group> groups{{{s("b")}, false}, {{s("a")}, false}};
        LayoutRun run(s("t2"), planArrangement(groups, {{s("a"), s("w-a")}, {s("b"), s("w-b")}}));
        require(run.settle(s("w-a"), true) && run.settle(s("w-b"), true), "both placed");
        require(run.entries() == QList<QStringList>{{s("w-b")}, {s("w-a")}}, "the row follows the groups, not arrival");
        require(run.finished() && run.missed() == 0, "nothing missed");
    }

    // A cancelled run: what had not come is missed.
    {
        LayoutRun run(s("t3"), planArrangement({{{s("a"), s("b")}, true}}, {}));
        require(run.claim(s("a"), s("w-a")), "a arrives");
        run.expire();
        require(run.finished() && run.placed() == 0 && run.missed() == 2,
                "a claimed window not yet admitted is missed too");
    }

    std::cout << "saved layout checks passed\n";
    return 0;
}
