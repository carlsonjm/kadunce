/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of the surface tone: Kadunce's own surfaces take the
// Plasma style's colours, not the window colours, and on a dark ground they
// are exactly the fixed values they always were.
#include "SurfaceTone.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

using namespace Kadunce;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

QString home;

void write(const QString &relative, const QByteArray &content)
{
    const QString path = home + QLatin1Char('/') + relative;
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "A fixture file could not be written");
    file.write(content);
}

void clear()
{
    QDir(home + QStringLiteral("/config")).removeRecursively();
    QDir(home + QStringLiteral("/data")).removeRecursively();
}

QByteArray scheme(const char *ground, const char *text, const char *accent, const char *accentText)
{
    return QByteArray("[Colors:Window]\nBackgroundNormal=") + ground + "\nForegroundNormal=" + text
        + "\n\n[Colors:Selection]\nBackgroundNormal=" + accent + "\nForegroundNormal=" + accentText + "\n";
}

const QByteArray Dark = scheme("28,28,28", "248,248,255", "248,248,255", "16,39,41");
const QByteArray Light = scheme("224,224,224", "16,39,41", "16,39,41", "248,248,255");

void requireFixedDark(const SurfaceTone &tone, const char *message)
{
    require(tone.dark(), message);
    require(tone.labelText(220) == QColor(248, 248, 255, 220), message);
    require(tone.labelText() == QColor(248, 248, 255), message);
    require(tone.pillFill(235) == QColor(20, 20, 20, 235), message);
    require(tone.lineInk() == QVector3D(0.88F, 0.88F, 0.88F), message);
}
} // namespace

int main()
{
    QTemporaryDir root;
    require(root.isValid(), "No temporary folder");
    home = root.path();
    qputenv("XDG_CONFIG_HOME", QString(home + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_DATA_HOME", QString(home + QStringLiteral("/data")).toUtf8());
    qputenv("XDG_CONFIG_DIRS", QString(home + QStringLiteral("/system-config")).toUtf8());
    qputenv("XDG_DATA_DIRS", QString(home + QStringLiteral("/system-data")).toUtf8());

    // Nothing set: the fixed dark values.
    requireFixedDark(SurfaceTone::read(), "With no colours set, the surfaces were not the fixed dark ones");

    // A dark scheme and a style without colours of its own: unchanged.
    write(QStringLiteral("config/kdeglobals"), Dark);
    write(QStringLiteral("config/plasmarc"), "[Theme]\nname=plain\n");
    {
        const SurfaceTone tone = SurfaceTone::read();
        requireFixedDark(tone, "A dark scheme changed the dark surfaces");
        require(tone.accent == QColor(248, 248, 255) && tone.accentText == QColor(16, 39, 41),
            "The highlight was not the scheme's");
    }

    // Light windows under a style that carries dark colours: the surfaces
    // stay dark, as the style's panels do.
    write(QStringLiteral("config/kdeglobals"), Light);
    write(QStringLiteral("config/plasmarc"), "[Theme]\nname=dark-panels\n");
    write(QStringLiteral("data/plasma/desktoptheme/dark-panels/colors"),
        scheme("28,28,28", "248,248,255", "61,174,233", "16,39,41"));
    {
        const SurfaceTone tone = SurfaceTone::read();
        requireFixedDark(tone, "A dark style beside light windows did not keep the surfaces dark");
        require(tone.accent == QColor(61, 174, 233), "The highlight was not the style's");
    }

    // A style without colours over a light scheme: light, in its colours.
    write(QStringLiteral("config/plasmarc"), "[Theme]\nname=plain\n");
    {
        const SurfaceTone tone = SurfaceTone::read();
        require(!tone.dark(), "A light scheme left the surfaces dark");
        require(tone.labelText(220) == QColor(16, 39, 41, 220), "A label did not take the scheme's text");
        require(tone.pillFill(235) == QColor(224, 224, 224, 235), "A pill did not take the scheme's ground");
        const QVector3D ink = tone.lineInk();
        require(qAbs(ink.x() - 16 / 255.0F) < 1e-4F && qAbs(ink.y() - 39 / 255.0F) < 1e-4F
                && qAbs(ink.z() - 41 / 255.0F) < 1e-4F, "An outline did not take the scheme's text");
        require(tone.accent == QColor(16, 39, 41) && tone.accentText == QColor(248, 248, 255),
            "The light highlight was not the scheme's");
    }

    // A light style over a dark scheme: the style wins.
    write(QStringLiteral("config/kdeglobals"), Dark);
    write(QStringLiteral("config/plasmarc"), "[Theme]\nname=light\n");
    write(QStringLiteral("data/plasma/desktoptheme/light/colors"), Light);
    require(!SurfaceTone::read().dark(), "A light style's own colours were ignored");

    // A colour the file cannot give keeps the dark default.
    write(QStringLiteral("data/plasma/desktoptheme/light/colors"),
        "[Colors:Window]\nBackgroundNormal=300,0,0\nForegroundNormal=not a colour\n");
    requireFixedDark(SurfaceTone::read(), "An unreadable colour was used");

    // Hex and named colours read as Qt reads them.
    write(QStringLiteral("data/plasma/desktoptheme/light/colors"),
        "[Colors:Window]\nBackgroundNormal=#f0f0f0\nForegroundNormal=black\n");
    {
        const SurfaceTone tone = SurfaceTone::read();
        require(tone.ground == QColor(240, 240, 240) && tone.text == QColor(0, 0, 0),
            "A hex or named colour was not read");
    }

    // Asked every frame, the tone follows a change of style.
    clear();
    write(QStringLiteral("config/plasmarc"), "[Theme]\nname=plain\n");
    write(QStringLiteral("config/kdeglobals"), Dark);
    require(SurfaceTone::current().dark(), "The current tone did not start dark");
    return 0;
}
