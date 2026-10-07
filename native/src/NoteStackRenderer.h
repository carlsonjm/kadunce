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
    // The stack at a card's corner: the top note in its colour with a folded
    // corner and a soft shadow, and the count in a dark badge on its top-right
    // corner when there is more than one.
    void renderStack(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                     const QList<QRectF> &sheets, const QStringList &colours, int count)
    {
        if (sheets.isEmpty() || sheets.size() != colours.size()) return;
        const QRectF top = sheets.last();
        const QRectF badge = count > 1 ? noteStackBadge(top) : QRectF();
        // Room for the shadow below the note.
        QRectF box = top.adjusted(-1, -1, 1, 3);
        if (!badge.isEmpty()) box |= badge.adjusted(-1, -1, 1, 1);
        const QString key = QStringLiteral("stack") + QChar(0x1f) + colours.join(QChar(0x1e))
            + QChar(0x1f) + QString::number(count) + QChar(0x1f) + QString::number(sheets.size());
        auto *texture = m_textures.object(key);
        if (!texture) {
            QImage image = canvas(box.size());
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            const QRectF sheet = top.translated(-box.topLeft());
            const double radius = NoteGeometry::Radius;
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 60));
            painter.drawRoundedRect(sheet.translated(0, 2), radius, radius);
            painter.setBrush(noteColour(colours.last()));
            painter.drawRoundedRect(sheet, radius, radius);
            // The folded corner, bottom-right, as a peeled sticky note.
            const double fold = 8.0;
            QPainterPath corner;
            corner.moveTo(sheet.right(), sheet.bottom() - fold);
            corner.lineTo(sheet.right() - fold, sheet.bottom());
            corner.lineTo(sheet.right() - fold, sheet.bottom() - fold + 3);
            corner.quadTo(sheet.right() - fold, sheet.bottom() - fold, sheet.right() - fold + 3,
                          sheet.bottom() - fold);
            corner.closeSubpath();
            painter.setBrush(QColor(0, 0, 0, 40));
            painter.drawPath(corner);
            if (!badge.isEmpty()) {
                const QRectF disc = badge.translated(-box.topLeft());
                painter.setBrush(QColor(0x14, 0x14, 0x14));
                painter.drawEllipse(disc);
                QFont font;
                font.setPixelSize(count > 9 ? 9 : 11);
                font.setWeight(QFont::DemiBold);
                painter.setFont(font);
                painter.setPen(QColor(0xf8, 0xf8, 0xff));
                painter.drawText(disc, Qt::AlignCenter,
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
