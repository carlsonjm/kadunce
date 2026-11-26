/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <KLocalizedString>
#include "SurfaceTone.h"
#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <opengl/gltexture.h>
#include <opengl/glshadermanager.h>
#include <opengl/glshader.h>
#include <QFontMetricsF>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <memory>
namespace Kadunce {
// CARD-LIFECYCLE.md §11, Connected: the middle place of a pane says what it
// does. Resting there fills a ring for as long as the rest takes, its words
// reading "Hold to stack", and once full, "Stack". A text HUD only, in the
// colour scheme's colours (SurfaceTone.h); it never draws a window.
class StackHoldLabel {
public:
    // `filled` runs 0 to 1; drawn centred on `centre`, in logical pixels.
    void render(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                QPointF centre, double filled) {
        const int step = int(std::round(std::clamp(filled, 0.0, 1.0) * Steps));
        const SurfaceTone &tone = SurfaceTone::current();
        const QColor words = tone.labelText();
        const double scale = viewport.scale();
        if (!m_texture || m_step != step || m_words != words || m_scale != scale) {
            // Drawn one texel to a screen pixel at this display's scale, and
            // smoothed, so the ring's edge is never stepped. Its shapes are
            // laid out at twice the logical size.
            QImage label(QSize(std::ceil(Width * scale), std::ceil(Height * scale)),
                         QImage::Format_ARGB32_Premultiplied);
            label.fill(Qt::transparent);
            QPainter painter(&label);
            painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
            painter.scale(scale / 2.0 * Grow, scale / 2.0 * Grow);
            // A plate behind ring and words, so they read over any window,
            // fitted to them with one margin all round.
            QFont font; font.setPixelSize(26); font.setWeight(QFont::DemiBold);
            const QString text = step >= Steps ? i18n("Stack") : i18n("Hold to stack");
            const QFontMetricsF metrics(font);
            const double wordsWidth = metrics.horizontalAdvance(text), wordsHeight = metrics.height();
            const double plateWidth = std::min(Laid, std::max(Ring, wordsWidth) + 2 * Margin);
            const double plateHeight = Margin + Ring + Gap + wordsHeight + Margin;
            const double top = (LaidHeight - plateHeight) / 2;
            painter.setPen(Qt::NoPen);
            painter.setBrush(tone.pillFill(210));
            painter.drawRoundedRect(QRectF((Laid - plateWidth) / 2, top, plateWidth, plateHeight), 28, 28);
            const QRectF ring(Laid / 2 - Ring / 2, top + Margin, Ring, Ring);
            QColor track = words; track.setAlphaF(0.22);
            painter.setPen(QPen(track, 6)); painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(ring);
            if (step > 0) {
                painter.setPen(QPen(words, 6, Qt::SolidLine, Qt::RoundCap));
                painter.drawArc(ring, 90 * 16, -int(360 * 16 * step / Steps));
            }
            painter.setFont(font); painter.setPen(words);
            painter.drawText(QRectF(0, ring.bottom() + Gap, Laid, wordsHeight), Qt::AlignCenter, text);
            painter.end();
            m_texture = KWin::GLTexture::upload(label);
            if (m_texture) m_texture->setFilter(GL_LINEAR);
            m_step = step;
            m_scale = scale;
            m_words = words;
        }
        if (!m_texture) return;
        KWin::ShaderBinder binder(KWin::ShaderTrait::MapTexture);
        auto *shader = KWin::ShaderManager::instance()->getBoundShader();
        auto matrix = viewport.projectionMatrix();
        matrix.scale(viewport.scale(), viewport.scale());
        matrix.translate(centre.x() - Width / 2.0, centre.y() - Height / 2.0);
        shader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
        shader->setColorspaceUniforms(KWin::ColorDescription::sRGB,
            renderTarget.colorDescription(), KWin::RenderingIntent::Perceptual);
        const bool blended = glIsEnabled(GL_BLEND);
        GLint sr, dr, sa, da;
        glGetIntegerv(GL_BLEND_SRC_RGB, &sr); glGetIntegerv(GL_BLEND_DST_RGB, &dr);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &sa); glGetIntegerv(GL_BLEND_DST_ALPHA, &da);
        glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        m_texture->bind(); m_texture->render(QSizeF(Width, Height)); m_texture->unbind();
        glBlendFuncSeparate(sr, dr, sa, da);
        if (!blended) glDisable(GL_BLEND);
    }
private:
    // Laid out at twice a 160 by 72 label, drawn a quarter larger: a ring and
    // its words, a margin round them and a gap between.
    static constexpr double Grow = 1.25, Laid = 320.0, LaidHeight = 144.0, Ring = 52.0, Margin = 22.0, Gap = 10.0;
    static constexpr int Width = 200, Height = 90, Steps = 40;
    int m_step = -1;
    double m_scale = 0.0;
    QColor m_words;
    std::unique_ptr<KWin::GLTexture> m_texture;
};
}
