/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <opengl/gltexture.h>
#include <opengl/glshadermanager.h>
#include <opengl/glshader.h>
#include <QPainter>
#include <QObject>
namespace Kadunce {
// A fixed-size text HUD only. Never receives a window or produces a card image.
class DesktopExitLabel {
public:
    void render(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                const QRectF &box, const QString &text = QObject::tr("Return to desktop")) {
            if (!m_texture || m_text != text) {
                QImage label(480, 72, QImage::Format_ARGB32_Premultiplied);
                label.fill(Qt::transparent);
                QPainter painter(&label);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setBrush(QColor(20, 20, 20, 235)); painter.setPen(Qt::NoPen);
                painter.drawRoundedRect(label.rect(), 24, 24);
                QFont font; font.setPixelSize(28); painter.setFont(font);
                painter.setPen(Qt::white);
                painter.drawText(label.rect(), Qt::AlignCenter, text);
                painter.end();
                m_texture = KWin::GLTexture::upload(label);
                m_text = text;
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
    std::unique_ptr<KWin::GLTexture> m_texture;
};
}
