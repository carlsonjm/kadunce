/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "GestureNote.h"
#include "SurfaceTone.h"

#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <opengl/gltexture.h>
#include <opengl/glshadermanager.h>
#include <opengl/glshader.h>

#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QVector4D>

#include <memory>

namespace Kadunce
{
// The refusal note's pill, drawn as the bottom edge's "return to desktop" label
// is: rounded, one line of words, in the Plasma style's colours (SurfaceTone.h). A text HUD only; it never
// receives a window.
class GestureNoteLabel
{
public:
    static constexpr double Height = 36.0;

    // The size the note needs for its text, in logical pixels.
    QSizeF size(const QString &text) const
    {
        QFont font;
        font.setPixelSize(FontPixels);
        return QSizeF(QFontMetrics(font).horizontalAdvance(text) + 2 * Padding, Height);
    }

    void render(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                const QRectF &box, const QString &text, double opacity)
    {
        if (box.isEmpty() || text.isEmpty() || opacity <= 0.0) return;
        const QSize pixels(qMax(1, qRound(box.width())), qMax(1, qRound(box.height())));
        const SurfaceTone &tone = SurfaceTone::current();
        const QColor fill = tone.pillFill(235);
        const QColor words = tone.labelText();
        if (!m_texture || m_text != text || m_pixels != pixels || m_fill != fill || m_words != words) {
            QImage image(pixels * 2, QImage::Format_ARGB32_Premultiplied);
            image.setDevicePixelRatio(2.0);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setBrush(fill);
            painter.setPen(Qt::NoPen);
            const QRectF pill(QPointF(0, 0), QSizeF(pixels));
            painter.drawRoundedRect(pill, 8, 8);
            QFont font;
            font.setPixelSize(FontPixels);
            painter.setFont(font);
            painter.setPen(words);
            const QString shown = QFontMetrics(font).elidedText(
                text, Qt::ElideRight, qMax(0, pixels.width() - 2 * Padding));
            painter.drawText(pill, Qt::AlignCenter, shown);
            painter.end();
            m_texture = KWin::GLTexture::upload(image);
            m_text = text;
            m_pixels = pixels;
            m_fill = fill;
            m_words = words;
        }
        if (!m_texture) return;
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
        m_texture->bind(); m_texture->render(box.size()); m_texture->unbind();
        glBlendFuncSeparate(sr, dr, sa, da);
        if (!blended) glDisable(GL_BLEND);
    }

private:
    static constexpr int FontPixels = 14;
    static constexpr int Padding = 16;
    QString m_text;
    QSize m_pixels;
    QColor m_fill;
    QColor m_words;
    std::unique_ptr<KWin::GLTexture> m_texture;
};
} // namespace Kadunce
