/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <KConfig>
#include <KConfigGroup>

#include <QColor>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>
#include <QVector3D>

namespace Kadunce
{

// The colours Kadunce paints its own surfaces with: Table, Spread's labels,
// the placement outline and rails, and the note pills. They come from the
// Plasma style, as a panel's do, not from the window colours, so a style that
// keeps its panels dark keeps these dark beside light windows. On a dark
// ground they are Kadunce's fixed values, so a dark style looks as it always
// has; on a light one the text and ground are the style's, and every line and
// fill is that text laid over the ground.
struct SurfaceTone
{
    QColor ground = QColor(20, 20, 20);
    QColor text = QColor(248, 248, 255);
    QColor accent = QColor(248, 248, 255);
    QColor accentText = QColor(16, 39, 41);

    // Light or dark as Kirigami judges a colour: by its luma.
    static bool isDark(const QColor &colour)
    {
        return 0.299 * colour.redF() + 0.587 * colour.greenF() + 0.114 * colour.blueF() <= 0.5;
    }
    bool dark() const { return isDark(ground); }

    // A label's words: Ghost White on dark, the style's text on light.
    QColor labelText(int alpha = 255) const
    {
        QColor colour = dark() ? QColor(248, 248, 255) : text;
        colour.setAlpha(alpha);
        return colour;
    }
    // A pill's fill under those words.
    QColor pillFill(int alpha) const
    {
        QColor colour = dark() ? QColor(20, 20, 20) : ground;
        colour.setAlpha(alpha);
        return colour;
    }
    // The outline and rail colour: a pale grey on dark, the text on light.
    QVector3D lineInk() const
    {
        if (dark()) return QVector3D(0.88F, 0.88F, 0.88F);
        return QVector3D(float(text.redF()), float(text.greenF()), float(text.blueF()));
    }

    // The Plasma style's colours as Plasma itself picks them: the style's own
    // colors file where it ships one, the system colour scheme where it does
    // not. Anything missing keeps the dark defaults above.
    static SurfaceTone read()
    {
        SurfaceTone tone;
        const QString colours = styleColoursFile();
        KConfig config(colours.isEmpty() ? QStringLiteral("kdeglobals") : colours,
                       colours.isEmpty() ? KConfig::FullConfig : KConfig::SimpleConfig);
        const KConfigGroup window(&config, QStringLiteral("Colors:Window"));
        const KConfigGroup selection(&config, QStringLiteral("Colors:Selection"));
        tone.ground = entry(window, "BackgroundNormal", tone.ground);
        tone.text = entry(window, "ForegroundNormal", tone.text);
        tone.accent = entry(selection, "BackgroundNormal", tone.accent);
        tone.accentText = entry(selection, "ForegroundNormal", tone.accentText);
        return tone;
    }

    // A colour as a scheme writes it, "r,g,b", or by name or #hex.
    static QColor entry(const KConfigGroup &group, const char *key, const QColor &fallback)
    {
        const QString value = group.readEntry(key, QString()).trimmed();
        const QStringList parts = value.split(QLatin1Char(','));
        if (parts.size() == 3 || parts.size() == 4) {
            int channels[4] = {0, 0, 0, 255};
            for (int i = 0; i < parts.size(); ++i) {
                bool ok = false;
                channels[i] = parts[i].trimmed().toInt(&ok);
                if (!ok || channels[i] < 0 || channels[i] > 255) return fallback;
            }
            return QColor(channels[0], channels[1], channels[2], channels[3]);
        }
        const QColor named = QColor::fromString(value);
        return named.isValid() ? named : fallback;
    }

    // The colors file of the Plasma style in use, or empty when it has none.
    static QString styleColoursFile()
    {
        const KConfig plasmarc(QStringLiteral("plasmarc"));
        const QString style = KConfigGroup(&plasmarc, QStringLiteral("Theme"))
            .readEntry("name", QStringLiteral("default"));
        return QStandardPaths::locate(QStandardPaths::GenericDataLocation,
            QStringLiteral("plasma/desktoptheme/%1/colors").arg(style));
    }

    // The tone now. It is read again at most once a second, and only when
    // one of the files it comes from has changed, so painting can ask every
    // frame and a change of style still shows within a second.
    static const SurfaceTone &current()
    {
        static SurfaceTone tone = read();
        static QElapsedTimer checked;
        static QString stamp = sourceStamp();
        if (!checked.isValid() || checked.elapsed() >= 1000) {
            checked.start();
            const QString now = sourceStamp();
            if (now != stamp) {
                stamp = now;
                tone = read();
            }
        }
        return tone;
    }

private:
    static QString sourceStamp()
    {
        const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        QString stamp;
        const QStringList paths{config + QStringLiteral("/plasmarc"),
                                config + QStringLiteral("/kdeglobals"), styleColoursFile()};
        for (const QString &path : paths) {
            const QFileInfo file(path);
            stamp += path + QLatin1Char('@')
                + (file.exists() ? QString::number(file.lastModified().toMSecsSinceEpoch()) : QString())
                + QLatin1Char(';');
        }
        return stamp;
    }
};

} // namespace Kadunce
