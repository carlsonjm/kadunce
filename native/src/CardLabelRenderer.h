/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "CardLabelPresentation.h"
#include "SurfaceTone.h"

#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <opengl/gltexture.h>
#include <opengl/glshadermanager.h>
#include <opengl/glshader.h>

#include <QFontMetrics>
#include <QCache>
#include <QImage>
#include <QPainter>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace Kadunce
{

class CardLabelRenderer
{
public:
    // Where a card's label is drawn: a row just below the card.
    static QRectF labelRect(const QRectF &card)
    {
        return QRectF(card.x(), card.bottom() + LabelGap, card.width(), RowHeight);
    }

    void render(const KWin::RenderTarget &renderTarget,
                const KWin::RenderViewport &viewport, const QRectF &card,
                const CardLabelPresentation &presentation)
    {
        if (card.isEmpty() || presentation.applicationName.isEmpty()) return;
        const int width = qMax(1, qRound(card.width()));
        const QColor words = SurfaceTone::current().labelText(220);
        const QString key = words.name(QColor::HexArgb) + QChar(0x1f)
            + presentation.applicationName
            + QChar(0x1f) + presentation.stackPosition
            + QChar(0x1f) + QString::number(width);
        // Drawn at the display's own pixels, so the words land on them
        // exactly, at any scale.
        const double ratio = std::max(1.0, viewport.scale());
        const QString scaled = key + QChar(0x1f) + QString::number(ratio);
        auto *texture = m_textures.object(scaled);
        if (!texture) {
            QImage image(qCeil(width * ratio), qCeil(RowHeight * ratio),
                         QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            image.setDevicePixelRatio(ratio);
            QPainter painter(&image);
            painter.setRenderHint(QPainter::TextAntialiasing);
            QFont font;
            font.setPixelSize(15);
            painter.setFont(font);
            painter.setPen(words);
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
            if (uploaded) uploaded->setFilter(GL_LINEAR);
            texture = uploaded.release();
            m_textures.insert(scaled, texture);
        }
        if (!texture) return;
        KWin::ShaderBinder binder(KWin::ShaderTrait::MapTexture);
        auto *shader = KWin::ShaderManager::instance()->getBoundShader();
        auto matrix = viewport.projectionMatrix();
        matrix.scale(viewport.scale(), viewport.scale());
        // On whole display pixels, so the words stay sharp.
        const double scale = viewport.scale();
        matrix.translate(std::round(card.x() * scale) / scale,
                         std::round((card.bottom() + LabelGap) * scale) / scale);
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
