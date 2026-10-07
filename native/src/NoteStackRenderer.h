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
    // The stack at a card's corner: its sheets the deepest first, each in its
    // note's colour, and the count on the top sheet when there is more than one.
    void renderStack(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                     const QList<QRectF> &sheets, const QStringList &colours, int count)
    {
        if (sheets.isEmpty() || sheets.size() != colours.size()) return;
        QRectF box;
        for (const QRectF &sheet : sheets) box |= sheet;
        const QString key = QStringLiteral("stack") + QChar(0x1f) + colours.join(QChar(0x1e))
            + QChar(0x1f) + QString::number(count) + QChar(0x1f) + QString::number(sheets.size());
        auto *texture = m_textures.object(key);
        if (!texture) {
            QImage image = canvas(box.size());
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            for (int index = 0; index < sheets.size(); ++index) {
                const QRectF sheet = sheets.at(index).translated(-box.topLeft());
                painter.setPen(QPen(QColor(0, 0, 0, 70), 1.0));
                painter.setBrush(noteColour(colours.at(index)));
                painter.drawRoundedRect(sheet.adjusted(0.5, 0.5, -0.5, -0.5),
                                        NoteGeometry::Radius, NoteGeometry::Radius);
            }
            if (count > 1) {
                QFont font;
                font.setPixelSize(14);
                font.setWeight(QFont::DemiBold);
                painter.setFont(font);
                painter.setPen(wordsOn(noteColour(colours.last())));
                painter.drawText(sheets.last().translated(-box.topLeft()), Qt::AlignCenter,
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
