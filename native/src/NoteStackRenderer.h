/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "StuckNotes.h"

#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <opengl/gltexture.h>
#include <opengl/glshadermanager.h>
#include <opengl/glshader.h>

#include <QCache>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainterPath>
#include <QImage>
#include <QPainter>
#include <QtMath>
#include <QVector4D>

#include <algorithm>

namespace Kadunce
{
// Gooseberry's notes on Spread's cards, drawn as the card labels are: each
// shape painted once into a texture and laid over the frame. Notes keep their
// own colours, and their words take Surface or Ghost White, whichever reads
// on that colour. A picture only; it never receives a window.
class NoteStackRenderer
{
public:
    // The stack at a card's corner, a mini note: the top note in its colour
    // with its first words and a soft shadow, the next note's edge peeking
    // above it, and the count quietly in its corner when there are several.
    void renderStack(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                     const QList<QRectF> &sheets, const QStringList &colours, const QString &title, int count)
    {
        if (sheets.isEmpty() || sheets.size() != colours.size()) return;
        const QRectF top = sheets.last();
        QRectF box;
        for (const QRectF &sheet : sheets) box |= sheet;
        // Room for the shadow below the note.
        box = box.adjusted(-1, -1, 1, 3);
        const QString key = QStringLiteral("stack") + QChar(0x1f) + colours.join(QChar(0x1e))
            + QChar(0x1f) + title + QChar(0x1f) + QString::number(count);
        auto *texture = m_textures.object(key);
        if (!texture) {
            QImage image = canvas(box.size());
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::TextAntialiasing);
            const double radius = NoteGeometry::MiniRadius;
            painter.setPen(Qt::NoPen);
            // The next note's edge, a shade deeper so the top note reads first.
            if (sheets.size() > 1) {
                painter.setBrush(noteColour(colours.first()).darker(108));
                painter.drawRoundedRect(sheets.first().translated(-box.topLeft()), radius, radius);
            }
            const QRectF sheet = top.translated(-box.topLeft());
            painter.setBrush(QColor(0, 0, 0, 60));
            painter.drawRoundedRect(sheet.translated(0, 2), radius, radius);
            const QColor colour = noteColour(colours.last());
            painter.setBrush(colour);
            painter.drawRoundedRect(sheet, radius, radius);
            const QColor words = wordsOn(colour);
            QFont font;
            font.setPixelSize(9);
            font.setWeight(QFont::DemiBold);
            painter.setFont(font);
            painter.setPen(words);
            const QRectF text = sheet.adjusted(6, 5, -6, -12);
            painter.drawText(text, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                             QFontMetrics(font).elidedText(title.simplified(), Qt::ElideRight, text.width() * 2 - 8));
            if (count > 1) {
                QFont small = font;
                small.setPixelSize(8);
                painter.setFont(small);
                QColor quiet = words;
                quiet.setAlphaF(0.65);
                painter.setPen(quiet);
                painter.drawText(sheet.adjusted(0, 0, -6, -3), Qt::AlignRight | Qt::AlignBottom,
                                 count > 99 ? QStringLiteral("99+") : QString::number(count));
            }
            painter.end();
            texture = KWin::GLTexture::upload(image).release();
            if (!texture) return;
            m_textures.insert(key, texture);
        }
        blit(renderTarget, viewport, texture, box, 1.0);
    }

    // One note fanned out over its card, or carried: its colour and its title.
    void renderNote(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                    const QRectF &box, const StuckNote &note, double opacity)
    {
        if (box.isEmpty() || opacity <= 0.0) return;
        const QString key = QStringLiteral("note") + QChar(0x1f) + note.title + QChar(0x1f)
            + note.colourHex + QChar(0x1f) + QString::number(qRound(box.width()))
            + QLatin1Char('x') + QString::number(qRound(box.height()));
        auto *texture = m_textures.object(key);
        if (!texture) {
            QImage image = canvas(box.size());
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::TextAntialiasing);
            const QRectF face(QPointF(0, 0), box.size());
            const QColor colour = noteColour(note.colourHex);
            painter.setPen(QPen(QColor(0, 0, 0, 70), 1.0));
            painter.setBrush(colour);
            painter.drawRoundedRect(face.adjusted(0.5, 0.5, -0.5, -0.5),
                                    NoteGeometry::Radius, NoteGeometry::Radius);
            QFont font;
            font.setPixelSize(14);
            painter.setFont(font);
            painter.setPen(wordsOn(colour));
            constexpr double padding = 10.0;
            const QRectF words = face.adjusted(padding, padding, -padding, -padding);
            // As many whole lines of the title as fit, the last one cut short.
            const QFontMetrics metrics(font);
            const int lines = std::max(1, int(words.height() / metrics.lineSpacing()));
            QString shown;
            QString rest = note.title.simplified();
            for (int line = 0; line < lines && !rest.isEmpty(); ++line) {
                if (line == lines - 1) {
                    shown += metrics.elidedText(rest, Qt::ElideRight, int(words.width()));
                    break;
                }
                int fit = rest.size();
                while (fit > 0 && metrics.horizontalAdvance(rest.left(fit)) > words.width()) {
                    const int space = rest.lastIndexOf(QLatin1Char(' '), fit - 1);
                    fit = space > 0 ? space : fit - 1;
                }
                fit = std::max(1, fit);
                shown += rest.left(fit).trimmed() + QLatin1Char('\n');
                rest = rest.mid(fit).trimmed();
            }
            painter.drawText(words, Qt::AlignLeft | Qt::AlignTop, shown.trimmed());
            painter.end();
            texture = KWin::GLTexture::upload(image).release();
            if (!texture) return;
            m_textures.insert(key, texture);
        }
        blit(renderTarget, viewport, texture, box, opacity);
    }

private:
    static QImage canvas(const QSizeF &size)
    {
        QImage image(qMax(1, qCeil(size.width())) * 2, qMax(1, qCeil(size.height())) * 2,
                     QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(2.0);
        image.fill(Qt::transparent);
        return image;
    }

    static QColor noteColour(const QString &hex)
    {
        const QColor colour(hex);
        return colour.isValid() ? colour : QColor(0xff, 0xe6, 0x80);
    }

    static QColor wordsOn(const QColor &colour)
    {
        const double light = 0.2126 * colour.redF() + 0.7152 * colour.greenF() + 0.0722 * colour.blueF();
        return light > 0.5 ? QColor(0x14, 0x14, 0x14) : QColor(0xf8, 0xf8, 0xff);
    }

    static void blit(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                     KWin::GLTexture *texture, const QRectF &box, double opacity)
    {
        KWin::ShaderBinder binder(KWin::ShaderTrait::MapTexture | KWin::ShaderTrait::Modulate);
        auto *shader = KWin::ShaderManager::instance()->getBoundShader();
        auto matrix = viewport.projectionMatrix();
        matrix.scale(viewport.scale(), viewport.scale());
        matrix.translate(box.x(), box.y());
        shader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
        // The texture is premultiplied, so every channel fades together.
        const float alpha = float(std::clamp(opacity, 0.0, 1.0));
        shader->setUniform(KWin::GLShader::Vec4Uniform::ModulationConstant,
                           QVector4D(alpha, alpha, alpha, alpha));
        shader->setColorspaceUniforms(KWin::ColorDescription::sRGB,
            renderTarget.colorDescription(), KWin::RenderingIntent::Perceptual);
        const bool blended = glIsEnabled(GL_BLEND);
        GLint sr, dr, sa, da;
        glGetIntegerv(GL_BLEND_SRC_RGB, &sr); glGetIntegerv(GL_BLEND_DST_RGB, &dr);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &sa); glGetIntegerv(GL_BLEND_DST_ALPHA, &da);
        glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        texture->bind();
        texture->render(QSizeF(qCeil(box.width()), qCeil(box.height())));
        texture->unbind();
        glBlendFuncSeparate(sr, dr, sa, da);
        if (!blended) glDisable(GL_BLEND);
    }

    QCache<QString, KWin::GLTexture> m_textures{64};
};
} // namespace Kadunce
