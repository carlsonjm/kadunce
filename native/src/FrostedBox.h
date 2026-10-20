/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <core/rect.h>
#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <opengl/glframebuffer.h>
#include <opengl/glshader.h>
#include <opengl/glshadermanager.h>
#include <opengl/gltexture.h>
#include <opengl/glvertexbuffer.h>

#include <QList>
#include <QMatrix4x4>
#include <QRect>
#include <QRectF>
#include <QVector2D>
#include <QVector4D>

#include <memory>
#include <vector>

namespace Kadunce
{
// A box of frosted glass: what is already drawn under it, blurred, under a
// neutral grey veil, cut to its corner. Where a window will land reads by
// its texture rather than its colour, so it shows on a light wallpaper and a
// dark one alike. The blur is the dual Kawase blur KWin's own uses: the
// pixels under the box are taken, halved and doubled again in steps, and
// the last step is drawn on screen.
class FrostedBox
{
public:
    // Steps of halving, and how far each step reaches. Four steps of three
    // reach about as far as a 24 px blur, enough that nothing under the box
    // can be read through it.
    static constexpr int Steps = 4;
    static constexpr float Reach = 3.0F;
    // The veil: a mid grey at 24%, neither white nor black, so it suits a
    // light look and a dark one.
    static constexpr float VeilGrey = 0.5F;
    static constexpr float VeilOpacity = 0.24F;

    void create()
    {
        auto *manager = KWin::ShaderManager::instance();
        m_down = manager->generateCustomShader(KWin::ShaderTrait::MapTexture,
            QByteArray(Vertex), QByteArray(DownFragment));
        m_up = manager->generateCustomShader(KWin::ShaderTrait::MapTexture,
            QByteArray(Vertex), QByteArray(UpFragment));
        m_onscreen = manager->generateCustomShader(KWin::ShaderTrait::MapTexture,
            QByteArray(Vertex), QByteArray(OnscreenFragment));
        if (!ready()) {
            m_down.reset(); m_up.reset(); m_onscreen.reset();
        }
    }

    [[nodiscard]] bool ready() const { return m_down && m_up && m_onscreen; }

    // Draws `box` frosted, within `parts` and inside `bounds`, all in logical
    // pixels, over what `renderTarget` holds. Every pixel of the box inside
    // `bounds` must be drawn afresh this frame, or the glass would blur
    // itself. `strength` lays the glass part way over what is under it, as
    // it fades in. False when nothing could be drawn and a plain fill is to
    // stand in.
    bool paint(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
               const QList<QRectF> &parts, const QRectF &box, double radius, const QRectF &bounds,
               float strength = 1.0F)
    {
        if (!ready()) return false;
        const QRect area = box.intersected(bounds).toAlignedRect().intersected(bounds.toAlignedRect());
        if (area.width() >> Steps < 1 || area.height() >> Steps < 1) return false;
        QList<QVector2D> onscreen;
        for (const QRectF &part : parts) {
            if (part.width() <= 0 || part.height() <= 0) continue;
            onscreen << QVector2D(part.topLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomLeft())
                     << QVector2D(part.bottomLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomRight());
        }
        if (onscreen.isEmpty()) return false;
        const GLenum format = renderTarget.texture() ? renderTarget.texture()->internalFormat() : GL_RGBA8;
        if (!prepare(area.size(), format)) return false;
        if (!m_framebuffers.front()->blitFromRenderTarget(renderTarget, viewport, KWin::Rect(area),
                KWin::Rect(0, 0, area.width(), area.height()))) return false;

        const bool blended = glIsEnabled(GL_BLEND);
        const bool scissored = glIsEnabled(GL_SCISSOR_TEST);
        GLint srcRgb, dstRgb, srcAlpha, dstAlpha;
        glGetIntegerv(GL_BLEND_SRC_RGB, &srcRgb); glGetIntegerv(GL_BLEND_DST_RGB, &dstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
        glDisable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);

        // Halving and doubling draw one quad over the whole of each texture.
        const QList<QVector2D> whole{QVector2D(0, 0), QVector2D(area.width(), 0), QVector2D(0, area.height()),
            QVector2D(0, area.height()), QVector2D(area.width(), 0), QVector2D(area.width(), area.height())};
        QMatrix4x4 offscreen;
        offscreen.ortho(QRectF(0, 0, area.width(), area.height()));
        const QVector4D wholeArea(0, 0, area.width(), area.height());
        const auto pass = [&](KWin::GLShader *shader, size_t from, size_t to) {
            KWin::ShaderBinder binder(shader);
            shader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, offscreen);
            shader->setUniform("frostArea", wholeArea);
            shader->setUniform("offset", Reach);
            const KWin::GLTexture *read = m_textures[from].get();
            shader->setUniform("halfpixel", QVector2D(0.5F / read->width(), 0.5F / read->height()));
            m_textures[from]->bind();
            KWin::GLFramebuffer::pushFramebuffer(m_framebuffers[to].get());
            auto *buffer = KWin::GLVertexBuffer::streamingBuffer();
            buffer->reset(); buffer->setVertices(whole); buffer->render(GL_TRIANGLES);
            KWin::GLFramebuffer::popFramebuffer();
            m_textures[from]->unbind();
        };
        for (size_t step = 1; step < m_textures.size(); ++step) pass(m_down.get(), step - 1, step);
        for (size_t step = m_textures.size() - 1; step > 1; --step) pass(m_up.get(), step, step - 1);

        if (scissored) glEnable(GL_SCISSOR_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        {
            KWin::GLShader *shader = m_onscreen.get();
            KWin::ShaderBinder binder(shader);
            auto matrix = viewport.projectionMatrix();
            matrix.scale(viewport.scale(), viewport.scale());
            shader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
            shader->setUniform("frostArea", QVector4D(area.x(), area.y(), area.width(), area.height()));
            shader->setUniform("offset", Reach);
            const KWin::GLTexture *read = m_textures[1].get();
            shader->setUniform("halfpixel", QVector2D(0.5F / read->width(), 0.5F / read->height()));
            shader->setUniform("destinationBox", QVector4D(box.x(), box.y(), box.width(), box.height()));
            shader->setUniform("outlineRadius", float(radius));
            shader->setUniform("veil", QVector4D(VeilGrey, VeilGrey, VeilGrey, VeilOpacity));
            shader->setUniform("strength", strength);
            shader->setColorspaceUniforms(KWin::ColorDescription::sRGB,
                renderTarget.colorDescription(), KWin::RenderingIntent::Perceptual);
            m_textures[1]->bind();
            auto *buffer = KWin::GLVertexBuffer::streamingBuffer();
            buffer->reset(); buffer->setVertices(onscreen); buffer->render(GL_TRIANGLES);
            m_textures[1]->unbind();
        }
        glBlendFuncSeparate(srcRgb, dstRgb, srcAlpha, dstAlpha);
        if (!blended) glDisable(GL_BLEND);
        return true;
    }

private:
    // One texture per step, each half the last, kept while the box keeps
    // its size.
    bool prepare(const QSize &size, GLenum format)
    {
        if (!m_textures.empty() && m_textures.front()->size() == size
            && m_textures.front()->internalFormat() == format) return true;
        m_framebuffers.clear();
        m_textures.clear();
        for (int step = 0; step <= Steps; ++step) {
            auto texture = KWin::GLTexture::allocate(format, size / (1 << step));
            if (!texture) break;
            texture->setFilter(GL_LINEAR);
            texture->setWrapMode(GL_CLAMP_TO_EDGE);
            auto framebuffer = std::make_unique<KWin::GLFramebuffer>(texture.get());
            if (!framebuffer->valid()) break;
            m_textures.push_back(std::move(texture));
            m_framebuffers.push_back(std::move(framebuffer));
        }
        if (m_textures.size() == size_t(Steps + 1)) return true;
        m_framebuffers.clear();
        m_textures.clear();
        return false;
    }

    // Each position also says where it falls in the texture: GL's rows run
    // upwards, so the picture's top is the texture's last row.
    static constexpr auto Vertex = R"GLSL(#version 140
in vec4 position;
uniform mat4 modelViewProjectionMatrix;
uniform vec4 frostArea;
out vec2 point;
out vec2 uv;
void main() {
    point = position.xy;
    uv = vec2((position.x - frostArea.x) / frostArea.z, 1.0 - (position.y - frostArea.y) / frostArea.w);
    gl_Position = modelViewProjectionMatrix * position;
}
)GLSL";
    static constexpr auto DownFragment = R"GLSL(#version 140
uniform sampler2D texUnit;
uniform float offset;
uniform vec2 halfpixel;
in vec2 uv;
out vec4 fragColor;
void main() {
    vec4 sum = texture(texUnit, uv) * 4.0;
    sum += texture(texUnit, uv - halfpixel * offset);
    sum += texture(texUnit, uv + halfpixel * offset);
    sum += texture(texUnit, uv + vec2(halfpixel.x, -halfpixel.y) * offset);
    sum += texture(texUnit, uv - vec2(halfpixel.x, -halfpixel.y) * offset);
    fragColor = sum / 8.0;
}
)GLSL";
    static constexpr auto UpFragment = R"GLSL(#version 140
uniform sampler2D texUnit;
uniform float offset;
uniform vec2 halfpixel;
in vec2 uv;
out vec4 fragColor;
void main() {
    vec4 sum = texture(texUnit, uv + vec2(-halfpixel.x * 2.0, 0.0) * offset);
    sum += texture(texUnit, uv + vec2(-halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(0.0, halfpixel.y * 2.0) * offset);
    sum += texture(texUnit, uv + vec2(halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(halfpixel.x * 2.0, 0.0) * offset);
    sum += texture(texUnit, uv + vec2(halfpixel.x, -halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(0.0, -halfpixel.y * 2.0) * offset);
    sum += texture(texUnit, uv + vec2(-halfpixel.x, -halfpixel.y) * offset) * 2.0;
    fragColor = sum / 12.0;
}
)GLSL";
    // The last doubling, drawn on screen: the blur under the veil, inside the
    // box's rounded corners. The veil is sRGB, so it is brought into the
    // target's encoding the way every other Kadunce fill is.
    static constexpr auto OnscreenFragment = R"GLSL(#version 140
uniform sampler2D texUnit;
uniform float offset;
uniform vec2 halfpixel;
uniform vec4 destinationBox;
uniform float outlineRadius;
uniform vec4 veil;
uniform float strength;
in vec2 point;
in vec2 uv;
out vec4 fragColor;
#include "colormanagement.glsl"
void main() {
    vec4 sum = texture(texUnit, uv + vec2(-halfpixel.x * 2.0, 0.0) * offset);
    sum += texture(texUnit, uv + vec2(-halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(0.0, halfpixel.y * 2.0) * offset);
    sum += texture(texUnit, uv + vec2(halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(halfpixel.x * 2.0, 0.0) * offset);
    sum += texture(texUnit, uv + vec2(halfpixel.x, -halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(0.0, -halfpixel.y * 2.0) * offset);
    sum += texture(texUnit, uv + vec2(-halfpixel.x, -halfpixel.y) * offset) * 2.0;
    vec3 blurred = (sum / 12.0).rgb;
    vec3 grey = nitsToDestinationEncoding(sourceEncodingToNitsInDestinationColorspace(vec4(veil.rgb, 1.0))).rgb;
    vec2 halfSize = destinationBox.zw * 0.5;
    float radius = min(outlineRadius, min(halfSize.x, halfSize.y));
    vec2 q = abs(point - destinationBox.xy - halfSize) - (halfSize - vec2(radius));
    float d = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
    float feather = max(fwidth(d), 0.5);
    float inside = 1.0 - smoothstep(-feather, 0.0, d);
    fragColor = vec4(mix(blurred, grey, veil.a), 1.0) * inside * strength;
}
)GLSL";

    std::unique_ptr<KWin::GLShader> m_down;
    std::unique_ptr<KWin::GLShader> m_up;
    std::unique_ptr<KWin::GLShader> m_onscreen;
    std::vector<std::unique_ptr<KWin::GLTexture>> m_textures;
    std::vector<std::unique_ptr<KWin::GLFramebuffer>> m_framebuffers;
};
} // namespace Kadunce
