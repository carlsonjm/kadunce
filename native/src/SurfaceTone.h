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
// the placement outline and rails, the note pills and the card backings. They
// follow the system colour scheme, as the windows do, so a light look turns
// them light with everything else. On a dark ground they are Kadunce's fixed
// values, so a dark scheme looks as it always has; on a light one the text and
// ground are the scheme's, and every line and fill is that text laid over the
// ground.
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

    // A label's words: Ghost White on dark, the scheme's text on light.
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

    // The backing a card's window stands on, which shows wherever the window
    // does not fill its card: the fixed near black on dark, the ground on light.
    QVector3D cardBacking() const
    {
        if (dark()) return QVector3D(0.075F, 0.075F, 0.075F);
        return QVector3D(float(ground.redF()), float(ground.greenF()), float(ground.blueF()));
    }

    // The system colour scheme's window and selection colours. Anything
    // missing keeps the dark defaults above.
    static SurfaceTone read()
    {
        SurfaceTone tone;
        const KConfig config(QStringLiteral("kdeglobals"));
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

    // The tone now. It is read again at most once a second, and only when
    // the scheme's file has changed, so painting can ask every frame and a
    // change of look still shows within a second.
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
        const QFileInfo file(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                             + QStringLiteral("/kdeglobals"));
        return file.exists() ? QString::number(file.lastModified().toMSecsSinceEpoch()) : QString();
    }
};

} // namespace Kadunce
