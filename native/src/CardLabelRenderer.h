/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "CardLabelPresentation.h"

#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <opengl/gltexture.h>
#include <opengl/glshadermanager.h>
#include <opengl/glshader.h>

#include <QFontMetrics>
#include <QCache>
#include <QImage>
#include <QPainter>

namespace Kadunce
{

class CardLabelRenderer
{
public:
    void render(const KWin::RenderTarget &renderTarget,
                const KWin::RenderViewport &viewport, const QRectF &card,
                const CardLabelPresentation &presentation)
    {
        if (card.isEmpty() || presentation.applicationName.isEmpty()) return;
        const int width = qMax(1, qRound(card.width()));
        const QString key = presentation.applicationName
            + QChar(0x1f) + presentation.stackPosition
            + QChar(0x1f) + QString::number(width);
        auto *texture = m_textures.object(key);
        if (!texture) {
            QImage image(width * 2, RowHeight * 2,
                         QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            image.setDevicePixelRatio(2.0);
            QPainter painter(&image);
            painter.setRenderHint(QPainter::TextAntialiasing);
            QFont font;
            font.setPixelSize(15);
            painter.setFont(font);
            painter.setPen(QColor(248, 248, 255, 220));
            const QFontMetrics metrics(font);
            const int positionWidth = presentation.stackPosition.isEmpty()
                ? 0 : metrics.horizontalAdvance(presentation.stackPosition);
            constexpr int edgePadding = 8;
            constexpr int centerGap = 8;
            const int centeredWidth = qMax(0, width - 2 *
                (positionWidth > 0 ? positionWidth + centerGap + edgePadding
                                   : edgePadding));
            const QString application = metrics.elidedText(
                presentation.applicationName, Qt::ElideRight, centeredWidth);
            painter.drawText(QRectF((width - centeredWidth) / 2.0, 0,
                                    centeredWidth, RowHeight),
                             Qt::AlignCenter, application);
            if (positionWidth > 0) {
                painter.drawText(QRectF(edgePadding, 0,
                                        width - edgePadding * 2, RowHeight),
                                 Qt::AlignRight | Qt::AlignVCenter,
                                 presentation.stackPosition);
            }
            painter.end();
            auto uploaded = KWin::GLTexture::upload(image);
            texture = uploaded.release();
            m_textures.insert(key, texture);
        }
        if (!texture) return;
        KWin::ShaderBinder binder(KWin::ShaderTrait::MapTexture);
        auto *shader = KWin::ShaderManager::instance()->getBoundShader();
        auto matrix = viewport.projectionMatrix();
        matrix.scale(viewport.scale(), viewport.scale());
        matrix.translate(card.x(), card.bottom() + LabelGap);
        shader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix,
                           matrix);
        shader->setColorspaceUniforms(KWin::ColorDescription::sRGB,
            renderTarget.colorDescription(), KWin::RenderingIntent::Perceptual);
        const bool blended = glIsEnabled(GL_BLEND);
        GLint sr, dr, sa, da;
        glGetIntegerv(GL_BLEND_SRC_RGB, &sr); glGetIntegerv(GL_BLEND_DST_RGB, &dr);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &sa); glGetIntegerv(GL_BLEND_DST_ALPHA, &da);
        glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        texture->bind();
        texture->render(QSizeF(width, RowHeight));
        texture->unbind();
        glBlendFuncSeparate(sr, dr, sa, da);
        if (!blended) glDisable(GL_BLEND);
    }

private:
    static constexpr int RowHeight = 24;
    static constexpr int LabelGap = 7;
    QCache<QString, KWin::GLTexture> m_textures{48};
};

} // namespace Kadunce
