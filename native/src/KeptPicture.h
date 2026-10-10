/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <effect/effectwindow.h>
#include <effect/effect.h>
#include <opengl/glframebuffer.h>
#include <opengl/glshader.h>
#include <opengl/glshadermanager.h>
#include <opengl/gltexture.h>
#include <opengl/glvertexbuffer.h>

#include <QMatrix4x4>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <span>

namespace Kadunce
{
// One window's picture as it stood a moment ago, kept while the window is
// resized: an application's first frames at a new size can be empty. It is
// taken from the window itself and fitted whole inside where the window now
// stands, so it is the same window, never a drawn stand-in.
class KeptPicture
{
public:
    // Draws the window as it stands into the kept picture; `draw` paints it
    // into the target and viewport given.
    bool take(KWin::EffectWindow *window,
              const std::function<void(const KWin::RenderTarget &, const KWin::RenderViewport &)> &draw)
    {
        if (!window || !window->screen()) return false;
        const double scale = window->screen()->scale();
        const KWin::RectF logical = window->expandedGeometry();
        const QSize textureSize = (logical.size() * scale).toSize();
        if (textureSize.isEmpty()) return false;
        auto texture = KWin::GLTexture::allocate(GL_RGBA8, textureSize);
        if (!texture) return false;
        texture->setFilter(GL_LINEAR);
        texture->setWrapMode(GL_CLAMP_TO_EDGE);
        KWin::GLFramebuffer framebuffer(texture.get());
        KWin::RenderTarget target(&framebuffer);
        KWin::RenderViewport viewport(logical, scale, target, QPoint());
        KWin::GLFramebuffer::pushFramebuffer(&framebuffer);
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);
        draw(target, viewport);
        KWin::GLFramebuffer::popFramebuffer();
        m_texture = std::move(texture);
        m_size = logical.size();
        return true;
    }

    [[nodiscard]] bool taken() const { return m_texture && !m_size.isEmpty(); }

    // The kept picture where the window now stands, fitted whole and centred,
    // with the window's own paint transform, opacity and saturation.
    bool draw(const KWin::RenderViewport &viewport, const KWin::RenderTarget &renderTarget,
              KWin::EffectWindow *window, const KWin::Region &deviceRegion,
              const KWin::WindowPaintData &data) const
    {
        if (!taken() || !window) return false;
        const QPointF origin(window->x(), window->y());
        const QRectF box = QRectF(window->expandedGeometry()).translated(-origin);
        if (box.isEmpty()) return false;
        const double fit = std::min(box.width() / m_size.width(), box.height() / m_size.height());
        const QSizeF fitted = m_size * fit;
        const QRectF place(box.center() - QPointF(fitted.width() / 2, fitted.height() / 2), fitted);
        const double scale = viewport.scale();
        const QRectF device(place.topLeft() * scale, place.size() * scale);

        KWin::GLShader *shader = KWin::ShaderManager::instance()->shader(KWin::ShaderTrait::MapTexture
            | KWin::ShaderTrait::Modulate | KWin::ShaderTrait::AdjustSaturation
            | KWin::ShaderTrait::TransformColorspace);
        KWin::ShaderBinder binder(shader);
        KWin::GLVertexBuffer *vbo = KWin::GLVertexBuffer::streamingBuffer();
        vbo->reset();
        vbo->setAttribLayout(std::span(KWin::GLVertexBuffer::GLVertex2DLayout), sizeof(KWin::GLVertex2D));
        const QMatrix4x4 coords = m_texture->matrix(KWin::NormalizedCoordinates);
        const auto uv = [&](float u, float v) { return QVector2D(coords.map(QPointF(u, v))); };
        const std::array<KWin::GLVertex2D, 6> quad = {{
            {QVector2D(device.topLeft()), uv(0, 0)},
            {QVector2D(device.topRight()), uv(1, 0)},
            {QVector2D(device.bottomLeft()), uv(0, 1)},
            {QVector2D(device.bottomLeft()), uv(0, 1)},
            {QVector2D(device.topRight()), uv(1, 0)},
            {QVector2D(device.bottomRight()), uv(1, 1)},
        }};
        const auto map = vbo->map<KWin::GLVertex2D>(quad.size());
        if (!map) return false;
        for (size_t i = 0; i < quad.size(); ++i) (*map)[i] = quad[i];
        vbo->unmap();
        vbo->bindArrays();

        const qreal rgb = data.brightness() * data.opacity();
        const qreal alpha = data.opacity();
        QMatrix4x4 mvp = viewport.projectionMatrix();
        mvp.translate(std::round(window->x() * scale), std::round(window->y() * scale));
        const auto toXYZ = renderTarget.colorDescription()->containerColorimetry().toXYZ();
        shader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, mvp * data.toMatrix(scale));
        shader->setUniform(KWin::GLShader::Vec4Uniform::ModulationConstant, QVector4D(rgb, rgb, rgb, alpha));
        shader->setUniform(KWin::GLShader::FloatUniform::Saturation, data.saturation());
        shader->setUniform(KWin::GLShader::Vec3Uniform::PrimaryBrightness,
            QVector3D(toXYZ(1, 0), toXYZ(1, 1), toXYZ(1, 2)));
        shader->setUniform(KWin::GLShader::IntUniform::TextureWidth, m_texture->width());
        shader->setUniform(KWin::GLShader::IntUniform::TextureHeight, m_texture->height());
        shader->setColorspaceUniforms(KWin::ColorDescription::sRGB, renderTarget.colorDescription(),
            KWin::RenderingIntent::Perceptual);
        const bool clipping = deviceRegion != KWin::Region::infinite();
        const KWin::Region clip = clipping
            ? viewport.transform().map(deviceRegion, renderTarget.transformedSize())
            : KWin::Region::infinite();
        if (clipping) glEnable(GL_SCISSOR_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        m_texture->bind();
        vbo->draw(clip, GL_TRIANGLES, 0, quad.size(), clipping);
        m_texture->unbind();
        glDisable(GL_BLEND);
        if (clipping) glDisable(GL_SCISSOR_TEST);
        vbo->unbindArrays();
        return true;
    }

private:
    std::unique_ptr<KWin::GLTexture> m_texture;
    QSizeF m_size;
};
} // namespace Kadunce
