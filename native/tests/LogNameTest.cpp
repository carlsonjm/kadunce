/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "LogName.h"

#include <QObject>

#include <cstdio>
#include <cstdlib>

using namespace Kadunce;

namespace {
void require(bool condition, const char *what)
{
    if (condition) return;
    std::fprintf(stderr, "log name: %s\n", what);
    std::exit(1);
}

const QUuid Id = QUuid::fromString(QStringLiteral("{1a2b3c4d-0000-4000-8000-000000000001}"));

// As KWin's EffectWindow names a window's class.
struct EffectLike : QObject {
    QString caption() const { return QStringLiteral("Bank statement - Firefox"); }
    QString windowClass() const { return QStringLiteral("navigator org.mozilla.firefox"); }
    QUuid internalId() const { return Id; }
};

// As KWin's Window names it.
struct WindowLike {
    QString caption() const { return QStringLiteral("notes.txt - Kate"); }
    QString resourceClass() const { return QStringLiteral("org.kde.kate"); }
    QUuid internalId() const { return Id; }
};
}

int main()
{
    EffectLike effect;
    const QString named = logNameOf(&effect);
    require(named == QStringLiteral("org.mozilla.firefox 1a2b3c4d"), "an effect window is its application and id");
    require(!named.contains(QStringLiteral("Bank")), "a title never reaches the log");

    WindowLike window;
    require(logNameOf(&window) == QStringLiteral("org.kde.kate 1a2b3c4d"), "a window is its application and id");
    require(!logNameOf(&window).contains(QStringLiteral("notes")), "a document's name never reaches the log");

    const QPointer<EffectLike> guarded(&effect);
    require(logNameOf(guarded) == named, "a guarded window logs the same");
    require(logNameOf(QPointer<EffectLike>()) == QStringLiteral("nothing"), "a window gone logs as nothing");
    require(logNameOf(static_cast<const WindowLike *>(nullptr)) == QStringLiteral("nothing"), "no window logs as nothing");
    require(logName(QString(), Id) == QStringLiteral("window 1a2b3c4d"), "a window with no class is still told apart");
    return 0;
}
