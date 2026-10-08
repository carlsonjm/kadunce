/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "SurfaceTone.h"
#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <opengl/gltexture.h>
#include <opengl/glshadermanager.h>
#include <opengl/glshader.h>
#include <QPainter>
#include <QObject>
namespace Kadunce {
// A fixed-size text HUD only, in the colour scheme's colours (SurfaceTone.h). Never receives a window or produces a card image.
class DesktopExitLabel {
public:
    void render(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                const QRectF &box, const QString &text = QObject::tr("return to desktop")) {
            const SurfaceTone &tone = SurfaceTone::current();
            const QColor fill = tone.pillFill(235);
            const QColor words = tone.labelText();
            if (!m_texture || m_text != text || m_fill != fill || m_words != words) {
                QImage label(480, 72, QImage::Format_ARGB32_Premultiplied);
                label.fill(Qt::transparent);
                QPainter painter(&label);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setBrush(fill); painter.setPen(Qt::NoPen);
                painter.drawRoundedRect(label.rect(), 16, 16); // 8 px as shown, at half size
                QFont font; font.setPixelSize(28); painter.setFont(font);
                painter.setPen(words);
                painter.drawText(label.rect(), Qt::AlignCenter, text);
                painter.end();
                m_texture = KWin::GLTexture::upload(label);
                m_text = text;
                m_fill = fill;
                m_words = words;
            }
            if (m_texture) {
                KWin::ShaderBinder binder(KWin::ShaderTrait::MapTexture);
                auto *shader = KWin::ShaderManager::instance()->getBoundShader();
                auto matrix = viewport.projectionMatrix();
                matrix.scale(viewport.scale(), viewport.scale());
                matrix.translate(box.center().x() - 120, box.top() + 12);
                shader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
                shader->setColorspaceUniforms(KWin::ColorDescription::sRGB,
                    renderTarget.colorDescription(), KWin::RenderingIntent::Perceptual);
                const bool blended = glIsEnabled(GL_BLEND);
                GLint sr, dr, sa, da;
                glGetIntegerv(GL_BLEND_SRC_RGB, &sr); glGetIntegerv(GL_BLEND_DST_RGB, &dr);
                glGetIntegerv(GL_BLEND_SRC_ALPHA, &sa); glGetIntegerv(GL_BLEND_DST_ALPHA, &da);
                glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                m_texture->bind(); m_texture->render(QSizeF(240, 36)); m_texture->unbind();
                glBlendFuncSeparate(sr, dr, sa, da);
                if (!blended) glDisable(GL_BLEND);
            }
    }
private:
    QString m_text;
    QColor m_fill;
    QColor m_words;
    std::unique_ptr<KWin::GLTexture> m_texture;
};
}
