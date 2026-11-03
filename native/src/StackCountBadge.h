/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "StackCountBadgeBox.h"
#include "SurfaceTone.h"

#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <opengl/gltexture.h>
#include <opengl/glshadermanager.h>
#include <opengl/glshader.h>

#include <QCache>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace Kadunce
{
// How many cards a Stack holds, on the corner of its front card in Spread: a
// small round counter, the label's words for its fill and the pill's fill for
// its figure, so it reads as the Stack's own tag rather than part of the
// window. A text HUD only; it never receives a window.
class StackCountBadge
{
public:
    static constexpr double Diameter = 26.0;

    // Where it stands on a front card: StackCountBadgeBox.h.
    static QRectF boxFor(const QRectF &face, double rotation, const QRectF &display)
    {
        return stackCountBadgeBox(face, rotation, display, Diameter);
    }

    void render(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                const QRectF &box, int count)
    {
        if (box.isEmpty() || count < 2) return;
        const SurfaceTone &tone = SurfaceTone::current();
        const QColor fill = tone.labelText(240);
        const QColor figure = tone.pillFill(255);
        const QString text = count > 99 ? QStringLiteral("99+") : QString::number(count);
        const double ratio = std::max(1.0, viewport.scale());
        const QString key = fill.name(QColor::HexArgb) + QChar(0x1f) + figure.name(QColor::HexArgb)
            + QChar(0x1f) + text + QChar(0x1f) + QString::number(ratio);
        auto *texture = m_textures.object(key);
        if (!texture) {
            QImage image(qCeil(Diameter * ratio), qCeil(Diameter * ratio),
                         QImage::Format_ARGB32_Premultiplied);
            image.setDevicePixelRatio(ratio);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::TextAntialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            const QRectF round(0.0, 0.0, Diameter, Diameter);
            painter.drawRoundedRect(round, Diameter / 2.0, Diameter / 2.0);
            QFont font;
            font.setPixelSize(text.size() > 2 ? 10 : 14);
            font.setWeight(QFont::DemiBold);
            painter.setFont(font);
            painter.setPen(figure);
            painter.drawText(round, Qt::AlignCenter, text);
            painter.end();
            auto uploaded = KWin::GLTexture::upload(image);
            if (uploaded) uploaded->setFilter(GL_LINEAR);
            texture = uploaded.release();
            if (!texture) return;
            m_textures.insert(key, texture);
        }
        KWin::ShaderBinder binder(KWin::ShaderTrait::MapTexture);
        auto *shader = KWin::ShaderManager::instance()->getBoundShader();
        auto matrix = viewport.projectionMatrix();
        matrix.scale(viewport.scale(), viewport.scale());
        // On whole display pixels, so the figure stays sharp.
        const double scale = viewport.scale();
        matrix.translate(std::round(box.x() * scale) / scale, std::round(box.y() * scale) / scale);
        shader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
        shader->setColorspaceUniforms(KWin::ColorDescription::sRGB,
            renderTarget.colorDescription(), KWin::RenderingIntent::Perceptual);
        const bool blended = glIsEnabled(GL_BLEND);
        GLint sr, dr, sa, da;
        glGetIntegerv(GL_BLEND_SRC_RGB, &sr); glGetIntegerv(GL_BLEND_DST_RGB, &dr);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &sa); glGetIntegerv(GL_BLEND_DST_ALPHA, &da);
        glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        texture->bind();
        texture->render(QSizeF(Diameter, Diameter));
        texture->unbind();
        glBlendFuncSeparate(sr, dr, sa, da);
        if (!blended) glDisable(GL_BLEND);
    }

private:
    QCache<QString, KWin::GLTexture> m_textures{16};
};
} // namespace Kadunce
