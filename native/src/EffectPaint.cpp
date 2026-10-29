/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Painting: each frame's preparation, the screen, and every window drawn as a
// card.
#include "EffectInternal.h"
#include "CardTuning.h"

namespace Kadunce
{

namespace
{
// The paper tier every window is cut to, as a person has set it (CardTuning.h).
double cardCornerRadius()
{
    return cardTuning().cornerRadius;
}
// Where the shadow of a card drawn at `card` reaches, the card included.
QRectF shadowExtent(const QRectF &card)
{
    const CardShadowShape shape = cardShadowShape(cardTuning().shadow);
    if (!shape.cast() || card.isEmpty()) return {};
    return card.adjusted(-shape.reachSides(), -shape.reachAbove(), shape.reachSides(), shape.reachBelow());
}
constexpr qint64 FrameStallMs = 50;
// The keyboard outline fades in over the Visual sheet's micro transition.
constexpr int KeyboardOutlineFadeMs = 140;
// The frost behind Shuffle Settings fades in over the same step.
constexpr int SettingsFrostFadeMs = 140;
constexpr int SettingsFrostDissolveMs = 300;
// The glass a session ends behind darkens to black over a second, or in a
// short change of opacity where motion is instant.
constexpr int SessionEndDarkenMs = 1000;
constexpr int SessionEndDarkenInstantMs = 150;

// The renderer that draws a view's scene. From KWin 6.8 each render device
// has its own; earlier KWin has one.
template<typename SceneT, typename View>
KWin::ItemRenderer *sceneRenderer(SceneT *scene, View *view)
{
    if constexpr (requires { scene->renderer(view->renderDevice()); }) return scene->renderer(view->renderDevice());
    else return scene->renderer();
}

// Draws the pointer again over what Kadunce painted after the scene, so no
// label, note or outline ever covers it. KWin draws the pointer inside the
// scene when it has no cursor plane, such as with a software cursor, and then
// anything an effect paints after the scene lands on top of it. The pointer
// is drawn again only where Kadunce painted, `overlaid` in logical pixels, and
// only when KWin composited it into this display's scene, so a pointer on its
// own hardware plane, or one nothing covered, is never doubled. False when
// drawing failed and painting is to stop.
bool paintPointerAbove(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                       const KWin::Region &deviceRegion, KWin::LogicalOutput *screen,
                       const QList<QRectF> &overlaid)
{
    if (overlaid.isEmpty()) return true;
    auto *scene = KWin::kwinApp() ? KWin::kwinApp()->scene() : nullptr;
    KWin::Item *pointer = scene ? scene->cursorItem() : nullptr;
    if (!screen || !pointer || !pointer->isVisible()) return true;
    const KWin::RectF bounds = pointer->mapToScene(pointer->boundingRect());
    if (bounds.isEmpty() || !QRectF(bounds).intersects(QRectF(screen->geometry()))) return true;
    KWin::Region covered;
    for (const QRectF &rect : overlaid) {
        const QRectF under = rect.intersected(QRectF(bounds));
        if (!under.isEmpty()) covered |= KWin::Region(viewport.mapToDeviceCoordinatesAligned(KWin::RectF(under)));
    }
    if (covered.isEmpty()) return true;
    KWin::SceneView *drawnIn = nullptr;
    for (KWin::RenderView *view : scene->views()) {
        auto *sceneView = qobject_cast<KWin::SceneView *>(view);
        if (!sceneView || sceneView->logicalOutput() != screen) continue;
        // On its own plane for any view of this display: not in the scene.
        if (!sceneView->shouldRenderItem(pointer)) return true;
        if (!drawnIn) drawnIn = sceneView;
    }
    if (!drawnIn) return true;
    KWin::ItemRenderer *renderer = sceneRenderer(scene, drawnIn);
    if (!renderer) return true;
    const KWin::Region region = deviceRegion & covered;
    if (region.isEmpty()) return true;
    return painted([&] {
        return renderer->renderItem(renderTarget, viewport, pointer, KWin::Scene::PAINT_SCREEN_TRANSFORMED,
            region, KWin::WindowPaintData{},
            [drawnIn](KWin::Item *item) { return !drawnIn->shouldRenderItem(item); },
            [](KWin::Item *) { return false; });
    });
}

// Card backing and vacant seam share one rigid transform and neutral material.
void paintCardSurface(KWin::GLShader *shader, const KWin::RenderTarget &renderTarget,
    const KWin::RenderViewport &viewport, const KWin::Region &clip,
    const QRectF &box, double angle, float opacity, float outline,
    const QVector3D &fillColor = SurfaceTone::current().cardBacking(),
    float fillOpacityScale = 1.0F)
{
    if (!shader || box.isEmpty()) return;
    QList<QVector2D> vertices;
    vertices << QVector2D(box.topLeft()) << QVector2D(box.topRight()) << QVector2D(box.bottomLeft())
             << QVector2D(box.bottomLeft()) << QVector2D(box.topRight()) << QVector2D(box.bottomRight());
    KWin::ShaderBinder binder(shader);
    auto matrix = viewport.projectionMatrix();
    matrix.scale(viewport.scale(), viewport.scale());
    matrix.translate(box.right(), box.bottom());
    matrix.rotate(float(angle), 0, 0, 1);
    matrix.translate(-box.right(), -box.bottom());
    shader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
    shader->setUniform("destinationBox", QVector4D(box.x(), box.y(), box.width(), box.height()));
    shader->setUniform("outlineRadius", float(cardCornerRadius()));
    shader->setUniform("surfaceFill", QVector4D(fillColor,
        opacity * fillOpacityScale));
    shader->setUniform("outlineOpacity", outline);
    // Card surfaces draw no ring; this keeps the shared shader's ink defined.
    shader->setUniform("outlineInk", QVector3D(0.88F, 0.88F, 0.88F));
    shader->setColorspaceUniforms(KWin::ColorDescription::sRGB,
        renderTarget.colorDescription(), KWin::RenderingIntent::Perceptual);
    const bool blended = glIsEnabled(GL_BLEND);
    const bool scissored = glIsEnabled(GL_SCISSOR_TEST);
    GLint previousScissor[4];
    glGetIntegerv(GL_SCISSOR_BOX, previousScissor);
    GLint sr, dr, sa, da;
    glGetIntegerv(GL_BLEND_SRC_RGB, &sr); glGetIntegerv(GL_BLEND_DST_RGB, &dr);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &sa); glGetIntegerv(GL_BLEND_DST_ALPHA, &da);
    glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    auto *buffer = KWin::GLVertexBuffer::streamingBuffer();
    glEnable(GL_SCISSOR_TEST);
    buffer->reset(); buffer->setVertices(vertices); buffer->render(clip, GL_TRIANGLES, true);
    glScissor(previousScissor[0], previousScissor[1], previousScissor[2], previousScissor[3]);
    if (!scissored) glDisable(GL_SCISSOR_TEST);
    glBlendFuncSeparate(sr, dr, sa, da);
    if (!blended) glDisable(GL_BLEND);
}

KWin::Region roundedClip(const KWin::Rect &rect, double radius)
{
    QPainterPath path;
    path.addRoundedRect(
        QRectF(rect.x(), rect.y(), rect.width(), rect.height()),
        radius, radius);
    return KWin::Region(QRegion(
        path.toFillPolygon().toPolygon(), Qt::WindingFill));
}
}

void Effect::prePaintScreen(KWin::ScreenPrePaintData &data)
{
    // §11, Every display: each display is framed for its own cards.
    SessionScope scope(this, m_currentSession,
        isCardDisplay(data.screen) ? stageKeyFor(data.screen) : m_scopedKey);
    if (m_tablePresenter && m_tablePresenter->visible() && data.screen == m_tablePresenterOutput)
        m_tablePresenter->prePaint(data.frame);
    // The offer lasts while the room it fills stays open as it was.
    if (m_roomOffer && m_roomOffer->output == data.screen) {
        // §11, Solo: beside the Active card, while it stays Active.
        const auto room = m_roomOffer->pairSide ? std::optional<QRectF>(pairOfferRoom(data.screen, *m_roomOffer->pairSide))
                                                : m_desktopStage->openRoomOn(data.screen);
        bool stands = room && *room == m_roomOffer->room
            && (m_roomOffer->pairSide
                ? m_roomOffer->pairWith && m_cardStage->activeCardIdentity() == m_roomOffer->pairWith
                    && m_cardStage->presentation() == CardPresentation::Active
                : m_cardStage->presentsBento());
        for (const auto &card : std::as_const(m_roomOffer->cards))
            stands = stands && card && !card->isDeleted();
        if (!stands) QTimer::singleShot(0, this, [this] {
            if (!m_roomOffer) return;
            qInfo() << "Kadunce" << Revision << "leaves the offer: the room it filled changed";
            leaveRoomOffer();
        });
    }
    m_framingTablet = isTabletOutput(data.screen);
    if (m_framingTablet) noteTabletFrameStart();
    // A released row, a returning card or a thrown one moves once per frame.
    if (isTabletOutput(data.screen)) m_cardStage->advanceMotion();
    // Notes and Search follow the Spread of the display in use, the one a
    // person reaches for last.
    if (isTabletOutput(data.screen) && isInUseDisplay(data.screen)) {
        const bool wasShown = std::exchange(m_notesSpreadShown, m_cardStage->isActive()
            && m_cardStage->presentation() == CardPresentation::Spread);
        if (!m_notesSpreadShown) m_stuckNotes.cancel();
        // Spread opening reads every entry again, so a missed signal cannot
        // leave a card's fan disagreeing with its window.
        if (m_notesSpreadShown && !wasShown && m_stuckNotesWatcher) m_stuckNotesWatcher->refresh();
        if (m_notesSpreadShown && !wasShown) m_notesAskedFor = {};
        syncNotesHandover(wasShown && !m_notesSpreadShown);
        syncNotesPause();
        syncSpokenLine();
        // Every way Spread opens ends in a frame of it formed; a swipe still
        // under the fingers may yet spring back.
        const bool spreadOpen = m_notesSpreadShown && !m_cardStage->spreadOpening();
        if (spreadOpen && !std::exchange(m_spreadOpenForLauncher, true)) inviteFloatingLauncher();
        if (!spreadOpen) m_spreadOpenForLauncher = false;
    }
    showSleepingCardsInSpread();
    // Presentation changes repaint, so a frame is where a dependent's
    // application is first seen to come to the front or leave it.
    for (const auto &window : std::as_const(m_dependents)) {
        if (!window || window->isDeleted() || !window->window()) continue;
        const bool shown = dependentShown(dependentLead(window));
        if ((!shown && !window->window()->isHidden())
            || (shown && m_heldDependents.contains(window))) {
            scheduleDependentSync();
            break;
        }
    }
    if (!m_heldDependents.isEmpty() || !m_drawnDialogs.empty()) updateDrawnDialogs();
    if (isInUseDisplay(data.screen) || !m_cardsOnEveryDisplay) prepareNeighbors(data.screen);
    m_continueRepaint = false;

    // Each frame looks again for Gooseberry's surface over the Active card.
    if (m_notesHandover) m_continueRepaint = true;
    if (m_cardStage->launcherGuestActive() && m_guestNeighborMotion.isValid()
        && m_guestNeighborMotion.elapsed() < motion(220)) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        m_continueRepaint = true;
    }
    for (auto it = m_bentoMotions.begin(); it != m_bentoMotions.end();) {
        if (!bentoMotionRect(it->window)) {
            if (it->window && !it->window->isDeleted()) unredirect(it->window);
            it = m_bentoMotions.erase(it);
        } else {
            // An arrival moves once its client draws the pane's shape or has
            // had long enough to; this frame draws it at its start, and its
            // clock runs from when the frame is done (postPaintScreen).
            if (it->arrival && !it->drawnMoving && it->output == data.screen) {
                const auto frame = it->window->frameGeometry();
                it->drawnMoving = !paneArrivalWaits(
                    paneArrivalShaped(frame.width(), frame.height(), it->to.width(), it->to.height()),
                    it->asked.elapsed());
            }
            data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
            m_continueRepaint = true;
            ++it;
        }
    }
    if (m_settlingWindow) {
        if (!dropSettleRect()) clearDropSettle();
        else {
            // A settle on the card display moves once its client draws the
            // shape it settles into or has had long enough to.
            if (!m_dropSettleTimer.isValid() && !m_settleDrawnMoving) {
                const auto frame = m_settlingWindow->frameGeometry();
                m_settleDrawnMoving = !paneArrivalWaits(
                    paneArrivalShaped(frame.width(), frame.height(), m_settleTo.width(), m_settleTo.height()),
                    m_settleAsked.elapsed());
            }
            data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS; m_continueRepaint = true;
        }
    }
    for (auto it = m_yieldingPanes.begin(); it != m_yieldingPanes.end();) {
        if (!yieldingPaneOpacity(it->window)) {
            if (it->window && !it->window->isDeleted()) unredirect(it->window);
            it = m_yieldingPanes.erase(it);
        } else {
            data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
            m_continueRepaint = true;
            ++it;
        }
    }
    if (m_carriedWindow) data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    if (m_gestureNote) {
        if (!m_gestureNote->output
            || m_gestureNote->shown.elapsed() >= GestureNoteTiming::TotalMs) {
            KWin::effects->addRepaint(KWin::RectF(m_gestureNote->box).toAlignedRect());
            m_gestureNote.reset();
        } else {
            m_continueRepaint = true;
        }
    }
    if (m_cardStage->isActive()
        && (isTabletOutput(data.screen) || m_cardStage->cardGrabActive())) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        if (m_cardStage->animationsRunning()) {
            m_continueRepaint = true;
        }
    }
    if (data.screen) {
        const auto whole = [](const QRectF &rect) { return KWin::Rect(rect.toAlignedRect()); };
        // Glass drawn last frame, and any about to be, is drawn afresh whole,
        // so the blur only ever reads what lies under it.
        for (const QRectF &rect : m_frostDrawn.value(data.screen->name())) data.paint |= whole(rect);
        if (const auto box = heldDestinationOn(data.screen)) data.paint |= whole(*box);
        // Settings' frost reads the whole display, so the whole display is
        // drawn afresh from the frame it first stands there.
        // Once Settings has gone, the glass dissolves, more slowly than it came.
        const int frostFade = motionDuration(SettingsFrostFadeMs, KWin::effects->animationTimeFactor());
        const int frostDissolve = motionDuration(SettingsFrostDissolveMs, KWin::effects->animationTimeFactor());
        if (m_sessionEnd.on) {
            // The session ends behind glass drawn over the whole display.
            stepSessionEndGlass();
            m_settingsFrostSince.remove(data.screen->name());
            m_settingsFrostLeaving.remove(data.screen->name());
            data.paint |= whole(QRectF(data.screen->geometry()));
            m_continueRepaint = true;
        } else if (KWin::EffectWindow *settings = settingsSurfaceOn(data.screen)) {
            if (isPowerQuestion(settings)) m_powerQuestionSeen.start();
            auto &since = m_settingsFrostSince[data.screen->name()];
            if (!since.isValid()) since.start();
            m_settingsFrostLeaving.remove(data.screen->name());
            data.paint |= whole(QRectF(data.screen->geometry()));
            if (since.elapsed() < frostFade) m_settingsFrostFading.append(data.screen->geometry());
        } else {
            if (m_settingsFrostSince.remove(data.screen->name()) > 0) {
                m_settingsFrostLeaving[data.screen->name()].start();
                qInfo() << "Kadunce" << Revision << "dissolves the frost on" << data.screen->name()
                        << "over" << frostDissolve << "ms";
            }
            const auto leaving = m_settingsFrostLeaving.constFind(data.screen->name());
            if (leaving != m_settingsFrostLeaving.constEnd() && leaving->elapsed() < frostDissolve) {
                data.paint |= whole(QRectF(data.screen->geometry()));
                m_settingsFrostFading.append(data.screen->geometry());
            } else {
                m_settingsFrostLeaving.remove(data.screen->name());
                m_settingsFrostSaid.remove(data.screen->name());
            }
        }
        if (m_placementPreview && m_placementPreviewOutput == data.screen)
            data.paint |= whole(QRectF(*m_placementPreview));
        // The keyboard's window, outlined: where the outline leaves and
        // arrives repaints, and it fades in over the next frames.
        KWin::EffectWindow *outlined = keyboardOutlineWindow(data.screen);
        auto &outline = m_keyboardOutlines[data.screen->name()];
        if (outline.window != outlined) {
            if (outline.window && !outline.window->isDeleted())
                data.paint |= whole(QRectF(outline.window->frameGeometry()));
            outline.window = outlined;
            outline.since.start();
        }
        if (outlined && outline.since.elapsed()
                < motionDuration(KeyboardOutlineFadeMs, KWin::effects->animationTimeFactor())) {
            data.paint |= whole(QRectF(outlined->frameGeometry()));
            m_keyboardOutlineFading.append(QRectF(outlined->frameGeometry()).toAlignedRect());
        }
    }
    traceShadows(data);
    KWin::effects->prePaintScreen(data);
}

void Effect::prepareNeighbors(KWin::LogicalOutput *screen)
{
    const auto neighbors = m_cardStage->preparationNeighbors();
    for (const auto &window : std::as_const(m_preparationNeighbors)) {
        if (window && !neighbors.contains(window)
            && (!m_cardStage->isActive()
                || m_cardStage->presentation() != CardPresentation::Spread
                || m_cardStage->paintSlot(window) == 99)) unredirect(window);
    }
    if (neighbors != m_preparationNeighbors) m_neighborPreparationFrames = neighbors.size();
    m_preparationNeighbors = neighbors;
    if (isTabletOutput(screen)) m_neighborPreparedThisFrame = false;
}

void Effect::postPaintScreen()
{
    // KWin consumes current layer damage after prePaintScreen. Request the
    // NEXT frame only after paint. Latch pre-paint activity so an animation
    // expiring during this frame still gets one exact final-state frame.
    const bool continueRepaint = m_continueRepaint;
    m_continueRepaint = false;
    for (const QRect &rect : std::exchange(m_keyboardOutlineFading, {})) KWin::effects->addRepaint(KWin::Rect(rect));
    for (const QRect &rect : std::exchange(m_settingsFrostFading, {})) KWin::effects->addRepaint(KWin::Rect(rect));
    for (auto &m : m_bentoMotions)
        if (m.drawnMoving && !m.timer.isValid()) m.timer.start();
    if (m_settlingWindow && m_settleDrawnMoving && !m_dropSettleTimer.isValid()) m_dropSettleTimer.start();
    KWin::effects->postPaintScreen();
    if (std::exchange(m_framingTablet, false)) noteTabletFrameEnd(continueRepaint);
    // Spread, Bento and the rest open and close along many paths, and every
    // one ends in a frame, so the context is announced here whenever the
    // presentation it reports changes: the dock's Spread button and the
    // Context Bar follow at once.
    QString presentation;
    eachCardDisplay([&](KWin::LogicalOutput *output) {
        presentation += output->name() + QLatin1Char(' ')
            + QString::number(m_cardStage->isActive() ? int(m_cardStage->presentation()) : -1) + QLatin1Char(' ');
    });
    // What a context line says changes the same way, while one is claimed.
    const QString line = m_contextLineClaims.isEmpty() ? QString() : contextLineSignature();
    const bool lineChanged = line != std::exchange(m_reportedContextLine, line);
    if (presentation != std::exchange(m_reportedPresentation, presentation) || lineChanged)
        Q_EMIT workspaceContextChanged();
    if (continueRepaint) {
        KWin::effects->addRepaintFull();
    }
    // A kept picture is looked at again each frame until it is let go.
    if (!m_heldPictures.empty()) KWin::effects->addRepaintFull();
    // A shadow drawn somewhere new is cleared from where it was next frame.
    if (!m_shadowRepaint.isEmpty()) KWin::effects->addRepaint(std::exchange(m_shadowRepaint, KWin::Region()));
}

void Effect::noteTabletFrameStart()
{
    if (!m_frameClock.isValid()) m_frameClock.start();
    const qint64 now = m_frameClock.elapsed();
    const qint64 gap = m_frameStartedAt < 0 ? 0 : now - m_frameStartedAt;
    // A frame is due about every 16 ms while motion runs; a gap of three is
    // one a person sees. Only gaps after a frame that asked for the next count.
    if (m_frameWanted && gap >= FrameStallMs) {
        qInfo() << "Kadunce" << Revision << "frame stall of" << gap << "ms on the card display:"
                << "the frame before spent" << m_frameWork << "ms painting and made"
                << m_framePreviewsMade << "new previews;"
                << (!m_cardStage->isActive() ? "cards off"
                    : m_cardStage->presentation() == CardPresentation::Spread ? "Spread"
                    : m_cardStage->presentation() == CardPresentation::Bento ? "Bento"
                    : m_cardStage->presentation() == CardPresentation::Desktop ? "desktop" : "Active")
                << (m_cardStage->cardGrabActive() ? "with a card held" : "")
                << (m_carriedWindow ? "with a window carried" : "");
    }
    m_frameStartedAt = now;
    m_framePreviewsMade = 0;
}

void Effect::noteTabletFrameEnd(bool wanted)
{
    if (m_frameStartedAt >= 0) m_frameWork = m_frameClock.elapsed() - m_frameStartedAt;
    m_frameWanted = wanted;
}

// CARD-LIFECYCLE.md §2: a sleeping card is chosen in Spread to wake it, so
// Spread keeps it drawn, dimmed, from the last frame its window showed. KWin
// paints no minimized window unless an effect holds it visible.
void Effect::showSleepingCardsInSpread()
{
    // Every display's Spread keeps its own sleeping cards drawn.
    QSet<KWin::EffectWindow *> sleeping;
    eachCardDisplay([&](KWin::LogicalOutput *) {
        if (!m_cardStage->isActive() || m_cardStage->presentation() != CardPresentation::Spread) return;
        for (const auto &card : m_cardStage->liveCards())
            if (card && !card->isDeleted() && card->isMinimized()) sleeping.insert(card.data());
    });
    for (auto it = m_sleepingShown.begin(); it != m_sleepingShown.end();) {
        if (sleeping.contains(it.key())) ++it;
        else it = m_sleepingShown.erase(it);
    }
    for (KWin::EffectWindow *window : std::as_const(sleeping))
        if (!m_sleepingShown.contains(window))
            m_sleepingShown.insert(window, KWin::EffectWindowVisibleRef(window, KWin::EffectWindow::PAINT_DISABLED_BY_MINIMIZE));
}

void Effect::prePaintWindow(KWin::RenderView *view,
                            KWin::EffectWindow *window,
                            KWin::WindowPrePaintData &data)
{
    // Asked of the stage holding the window, else its display's.
    SessionScope scope(this, m_currentSession, stageKeyOf(window));
    if (window == m_carriedWindow || (window == m_settlingWindow && dropSettleRect()) || bentoMotionRect(window)
        || yieldingPaneOpacity(window) || m_stackScrubShown.contains(window)
        || m_roomOfferShown.contains(window)) {
        data.setTransformed(); data.setTranslucent();
    }
    // Left undrawn, so it must not hide what is under it from the scene.
    if (m_previewDesktop ? hiddenByDesktopPreview(window) : hiddenOnOtherDesktop(window)) data.setTranslucent();
    // Left undrawn while Spread draws its notes on the card.
    if (notesSurfaceStepsAside(window)) data.setTranslucent();
    if (steppingInUnseen(window)) data.setTranslucent();
    // Drawn with its card, not where it stands, so it hides nothing there.
    if (m_drawnDialogs.contains(window) || drawnWithCarriedCard(window)) data.setTranslucent();
    // Its trimmed corners show what is under them, so that must be drawn.
    if (trimsCorners(window)) data.setTranslucent();
    // Its shadow falls outside it, and KWin draws a window only inside its own
    // bounds unless it is told the window is drawn elsewhere too.
    if (m_shadowTraces.contains(window)) data.setTransformed();
    // A card a display holds but does not show, such as one waiting behind
    // the Active card or a layout in its place, is left undrawn, so it must
    // not hide what is under it either: that is what those corners show.
    if (m_cardStage->isActive() && window != m_nativeCarry && isCardWindow(window)
        && !m_desktopStage->managesWindow(window)
        && m_cardStage->presentation() != CardPresentation::Desktop
        && m_cardStage->paintSlot(window) == 99) data.setTranslucent();
    // The Active card's strip draws its two cards off their places, so neither
    // hides the other, and each is drawn beyond its own bounds.
    if (m_cardStage->activeSlideShift(window)) {
        data.setTransformed();
        data.setTranslucent();
    }
    if (m_cardStage->isActive()
        && window != m_nativeCarry
        && m_cardStage->presentation() == CardPresentation::Spread
        && m_cardStage->paintSlot(window) != 99) {
        data.setTransformed();
        // A rotated opaque client needs compositor blending for the
        // fractional coverage emitted by the r21 fan aperture.
        data.setTranslucent();
    }
    KWin::OffscreenEffect::prePaintWindow(view, window, data);
}

std::optional<QRectF> Effect::heldDestinationOn(KWin::LogicalOutput *screen)
{
    const auto &reservation = m_carriedWindow ? m_carryDestination : m_lineDestination;
    const auto &preview = m_carriedWindow ? m_carryPreview : m_linePreview;
    const auto cardEntry = m_carriedWindow ? m_carryCardEntryOutput : m_lineCardEntryOutput;
    const auto cardExit = m_carriedWindow ? m_carryCardExitOutput : QPointer<KWin::LogicalOutput>();
    // A window leaning into a Stack is its own preview; no box is drawn.
    if (!m_destinationShader || !screen || !preview || (m_carriedWindow && m_carryStackOnto)
        || !(m_carriedWindow || m_cardStage->cardGrabActive())) return std::nullopt;
    if (cardEntry == screen || (cardExit && cardExit == screen)
        || (reservation && !cardEntry && reservation->showsPlacementOutline()
            && reservation->destinationOutput() == screen
            && m_desktopStage->cardDropValid(*reservation)))
        return QRectF(*preview);
    return std::nullopt;
}

void Effect::drawHeldBoxUnder(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport)
{
    // The box sits under the window in hand, so the glass frosts the windows
    // and wallpaper beneath it and never the window itself.
    KWin::LogicalOutput *screen = m_paintingOutput;
    if (!screen || m_heldBoxDrawnOn == screen) return;
    if (const auto box = heldDestinationOn(screen)) {
        drawDestinationOutline(renderTarget, viewport, m_paintingRegion, screen, *box);
        m_heldBoxDrawnOn = screen;
    }
}

// The paint path's own test: a window Card Stage does not draw as a card.
bool Effect::paintedAsItStands(KWin::EffectWindow *window)
{
    return !m_cardStage->isActive() || !(isCardWindow(window) || m_sleepingShown.contains(window))
        || m_desktopStage->managesWindow(window)
        || (m_cardStage->presentation() == CardPresentation::Desktop
            && m_cardStage->liveCardIndex(window) < 0);
}

KWin::EffectWindow *Effect::keyboardOutlineWindow(KWin::LogicalOutput *screen)
{
    if (!m_showsKeyboardOutline || !m_keyboardOutlineShader || !m_keyboardEdgeShader || !screen || m_previewDesktop)
        return nullptr;
    // Nothing is outlined while a window is in hand, or while Spread, Table or
    // an open room's offer stands on the display.
    if (m_carriedWindow || m_cardStage->cardGrabActive()) return nullptr;
    if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread) return nullptr;
    if (m_tablePresenter && m_tablePresenter->visible() && m_tablePresenterOutput == screen) return nullptr;
    if (m_roomOffer && m_roomOffer->output == screen) return nullptr;
    KWin::EffectWindow *active = KWin::effects->activeWindow();
    if (!active || active->isDeleted() || active->isMinimized() || active->isFullScreen()
        || !(active->isNormalWindow() || active->isDialog()) || active->screen() != screen
        || !active->isOnCurrentDesktop() || !paintedAsItStands(active)) return nullptr;
    // A lone window needs no marker: it is outlined only while another shows
    // beside it on the display.
    const QRectF frame(active->frameGeometry());
    const auto order = KWin::effects->stackingOrder();
    for (KWin::EffectWindow *other : order) {
        if (!other || other == active || other->isDeleted() || other->isMinimized()
            || !(other->isNormalWindow() || other->isDialog()) || other->screen() != screen
            || !other->isOnCurrentDesktop() || !paintedAsItStands(other)) continue;
        // A dialog and the window it belongs to are one thing.
        if (other->mainWindows().contains(active) || active->mainWindows().contains(other)) continue;
        if (frame.contains(QRectF(other->frameGeometry()))) continue;
        return active;
    }
    return nullptr;
}

float Effect::keyboardOutlineOpacity(KWin::EffectWindow *window, const KWin::WindowPaintData &data) const
{
    if (!m_paintingOutput || window->screen() != m_paintingOutput) return 0.0F;
    const auto found = m_keyboardOutlines.constFind(m_paintingOutput->name());
    if (found == m_keyboardOutlines.cend() || found->window != window) return 0.0F;
    const int duration = motionDuration(KeyboardOutlineFadeMs, KWin::effects->animationTimeFactor());
    const double progress = std::clamp(double(found->since.elapsed()) / duration, 0.0, 1.0);
    return float((1.0 - std::pow(1.0 - progress, 3.0)) * data.opacity());
}

QColor Effect::keyboardOutlineInk()
{
    // The accent, or the scheme's text where the accent would vanish into its
    // ground.
    const SurfaceTone &tone = SurfaceTone::current();
    const auto luma = [](const QColor &c) { return 0.299 * c.redF() + 0.587 * c.greenF() + 0.114 * c.blueF(); };
    return std::abs(luma(tone.accent) - luma(tone.ground)) < 0.2 ? tone.text : tone.accent;
}

void Effect::paintKeyboardOutline(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
    KWin::EffectWindow *window, const KWin::Region &deviceRegion, const KWin::WindowPaintData &data)
{
    // A window that keeps its own corners has its outline drawn with its
    // picture (drawWindow); one Kadunce trims gets it here, on the trim.
    if (!m_keyboardOutlineShader || !trimsCorners(window)) return;
    const float opacity = keyboardOutlineOpacity(window, data);
    if (opacity <= 0.0F) return;
    const QRectF frame(window->frameGeometry());
    const QRectF box(frame.x() + data.xTranslation(), frame.y() + data.yTranslation(),
                     frame.width() * data.xScale(), frame.height() * data.yScale());
    if (box.isEmpty()) return;
    QList<QVector2D> vertices;
    for (const auto &dirty : deviceRegion.rects()) {
        const auto part = QRectF(viewport.mapFromDeviceCoordinates(KWin::RectF(dirty))).intersected(box);
        if (part.isEmpty()) continue;
        vertices << QVector2D(part.topLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomLeft())
                 << QVector2D(part.bottomLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomRight());
    }
    if (vertices.isEmpty()) return;
    const QColor ink = keyboardOutlineInk();
    KWin::ShaderBinder binder(m_keyboardOutlineShader.get());
    auto matrix = viewport.projectionMatrix();
    matrix.scale(viewport.scale(), viewport.scale());
    m_keyboardOutlineShader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
    m_keyboardOutlineShader->setUniform("destinationBox", QVector4D(box.x(), box.y(), box.width(), box.height()));
    m_keyboardOutlineShader->setUniform("outlineRadius", float(cardCornerRadius()));
    m_keyboardOutlineShader->setUniform("outlineInk", QVector3D(ink.redF(), ink.greenF(), ink.blueF()));
    m_keyboardOutlineShader->setUniform("outlineOpacity", opacity);
    m_keyboardOutlineShader->setColorspaceUniforms(KWin::ColorDescription::sRGB,
        renderTarget.colorDescription(), KWin::RenderingIntent::Perceptual);
    const bool blended = glIsEnabled(GL_BLEND);
    GLint srcRgb, dstRgb, srcAlpha, dstAlpha;
    glGetIntegerv(GL_BLEND_SRC_RGB, &srcRgb); glGetIntegerv(GL_BLEND_DST_RGB, &dstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    auto *buffer = KWin::GLVertexBuffer::streamingBuffer();
    buffer->reset(); buffer->setVertices(vertices); buffer->render(GL_TRIANGLES);
    glBlendFuncSeparate(srcRgb, dstRgb, srcAlpha, dstAlpha);
    if (!blended) glDisable(GL_BLEND);
}

void Effect::drawDestinationOutline(const KWin::RenderTarget &renderTarget,
    const KWin::RenderViewport &viewport, const KWin::Region &deviceRegion,
    KWin::LogicalOutput *screen, const QRectF &box, const QRectF &hole)
{
    QRectF clip = QRectF(KWin::effects->clientArea(KWin::MaximizeArea, screen))
        .intersected(QRectF(screen->geometry()));
    clip.setBottom(std::min(clip.bottom(), double(screen->geometry().bottom()) - 48.0));
    QList<QRectF> parts;
    for (const auto &dirty : deviceRegion.rects()) {
        const auto whole = QRectF(viewport.mapFromDeviceCoordinates(KWin::RectF(dirty)))
            .intersected(clip).intersected(box);
        if (whole.isEmpty()) continue;
        // Whatever of `hole` the box covers is left undrawn: the bands above,
        // below and beside it.
        const QRectF cut = whole.intersected(hole);
        if (cut.isEmpty()) {
            parts.append(whole);
            continue;
        }
        parts << QRectF(whole.left(), whole.top(), whole.width(), cut.top() - whole.top())
              << QRectF(whole.left(), cut.bottom(), whole.width(), whole.bottom() - cut.bottom())
              << QRectF(whole.left(), cut.top(), cut.left() - whole.left(), cut.height())
              << QRectF(cut.right(), cut.top(), whole.right() - cut.right(), cut.height());
    }
    // A destination is a box of frosted glass, so it reads as a place over
    // any window or wallpaper under it, light or dark. Without the blur it
    // is a filled box at the Visual sheet's hover step.
    if (m_frostedBox.paint(renderTarget, viewport, parts, box, cardCornerRadius(), clip)) {
        m_frostDrawn[screen->name()].append(box.intersected(clip));
        return;
    }
    fillDestinationParts(renderTarget, viewport, parts, box, 0.13f, 0.f);
}

bool Effect::isPowerQuestion(KWin::EffectWindow *window)
{
    return window && window->window()
        && window->window()->resourceName() == QLatin1String("ksmserver-logout-greeter");
}

KWin::EffectWindow *Effect::settingsSurfaceOn(KWin::LogicalOutput *screen) const
{
    if (!screen || m_previewDesktop) return nullptr;
    // A layer surface has no title, and Settings and the keyboard sheet are
    // the same program, so Settings is known by the process that asked for
    // frost behind it. The question before a restart, shut down or sign out
    // is KDE's own program, whose surface is frosted alike.
    const auto stack = KWin::effects->stackingOrder();
    for (KWin::EffectWindow *window : stack) {
        if (!window || window->isDeleted() || !window->window() || !window->isVisible()
            || window->screen() != screen
            || !window->window()->inherits("KWin::LayerShellV1Window")) continue;
        if (isPowerQuestion(window)) return window;
        for (const qint64 pid : m_frostBehind)
            if (qint64(window->pid()) == pid) return window;
    }
    return nullptr;
}

bool Effect::fadeOverSettingsFrost(KWin::EffectWindow *window) const
{
    return window && !window->isDeleted() && window->window() && window->isVisible() && m_paintingOutput
        && window->screen() == m_paintingOutput && window->isOnScreenDisplay()
        && window->window()->inherits("KWin::LayerShellV1Window")
        && window->window()->layer() == KWin::BelowLayer
        && (settingsSurfaceOn(m_paintingOutput) || m_settingsFrostLeaving.contains(m_paintingOutput->name()));
}

void Effect::drawSettingsFrostUnder(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                                    KWin::EffectWindow *window)
{
    KWin::LogicalOutput *screen = m_paintingOutput;
    if (!window || !screen || m_sessionEnd.on || m_settingsFrostDrawnOn == screen || !window->window()
        || !(window->isDock() || window->window()->inherits("KWin::LayerShellV1Window"))) return;
    KWin::EffectWindow *settings = settingsSurfaceOn(screen);
    const bool leaving = !settings && m_settingsFrostLeaving.contains(screen->name());
    if (!settings && !leaving) return;
    // The glass is laid under the first of this display's panels to be
    // drawn, or under Settings itself where there are none, so the panels
    // and their fades stand over it as they are, and nothing ends at an edge.
    const QRectF whole(screen->geometry());
    if (window != settings && !(window->isDock() && QRectF(window->frameGeometry()).intersects(whole))) return;
    m_settingsFrostDrawnOn = screen;
    QList<QRectF> parts;
    for (const auto &dirty : m_paintingRegion.rects()) {
        const QRectF part = QRectF(viewport.mapFromDeviceCoordinates(KWin::RectF(dirty))).intersected(whole);
        if (!part.isEmpty()) parts.append(part);
    }
    const int fade = motionDuration(SettingsFrostFadeMs, KWin::effects->animationTimeFactor());
    const int dissolve = motionDuration(SettingsFrostDissolveMs, KWin::effects->animationTimeFactor());
    float strength = 1.0F;
    if (leaving) {
        strength = float(1.0 - std::clamp(double(m_settingsFrostLeaving.value(screen->name()).elapsed()) / dissolve, 0.0, 1.0));
    } else if (const auto since = m_settingsFrostSince.constFind(screen->name());
               since != m_settingsFrostSince.constEnd() && fade > 0) {
        strength = float(std::clamp(double(since->elapsed()) / fade, 0.0, 1.0));
    }
    const bool drawn = m_settingsFrost.paint(renderTarget, viewport, parts, whole, 0.0, whole, strength);
    if (drawn) m_frostDrawn[screen->name()].append(whole);
    // The fades a panel lays under the windows, the Context Panel's tail
    // among them, are drawn over the glass, so the panel ends as it always
    // does rather than at the edge of a bar.
    int fades = 0;
    for (KWin::EffectWindow *fade : KWin::effects->stackingOrder()) {
        if (!fadeOverSettingsFrost(fade)) continue;
        ++fades;
        KWin::WindowPaintData fadeData;
        fadeData.setOpacity(fade->opacity());
        (void)painted([&] { return KWin::effects->drawWindow(renderTarget, viewport, fade,
            PAINT_WINDOW_TRANSLUCENT, KWin::Region::infinite(), fadeData); });
    }
    if (!m_settingsFrostSaid.contains(screen->name())) {
        m_settingsFrostSaid.insert(screen->name());
        qInfo() << "Kadunce" << Revision << "frosts" << screen->name()
                << (isPowerQuestion(settings) ? "under the power question, laid under" : "under Shuffle Settings, laid under")
                << (window == settings ? "Settings" : "a panel")
                << (drawn ? "as glass" : "but the glass could not be drawn") << "with" << fades
                << "panel fades over it";
    }
}

void Effect::beginSessionEndGlass(bool cardsGivenBack)
{
    if (m_sessionEnd.on) {
        m_sessionEnd.cardsGivenBack = m_sessionEnd.cardsGivenBack || cardsGivenBack;
        return;
    }
    m_sessionEnd = SessionEndGlass{};
    m_sessionEnd.on = true;
    m_sessionEnd.cardsGivenBack = cardsGivenBack;
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder())
        if (window) m_sessionEnd.known.insert(window);
    m_sessionEnd.since.start();
    m_sessionEnd.lastStep.start();
    qInfo() << "Kadunce" << Revision << "keeps the glass up while the session ends";
    KWin::effects->addRepaintFull();
}

void Effect::endSessionEndGlass()
{
    if (!m_sessionEnd.on) return;
    const bool cardsGivenBack = m_sessionEnd.cardsGivenBack;
    m_sessionEnd = SessionEndGlass{};
    // Cancelled: the glass dissolves as it does when the question goes, and
    // the cards given back for the session's end are taken up again.
    for (KWin::LogicalOutput *screen : KWin::effects->screens()) {
        if (!screen) continue;
        m_settingsFrostLeaving[screen->name()].start();
        m_settingsFrostSaid.insert(screen->name());
    }
    qInfo() << "Kadunce" << Revision << "the session goes on: the glass dissolves"
            << (cardsGivenBack ? "and the cards come back" : "");
    if (cardsGivenBack) {
        m_letGo = false;
        QTimer::singleShot(0, this, [this] { (void)holdDisplaysInCards(nullptr); });
    }
    KWin::effects->addRepaintFull();
}

void Effect::stepSessionEndGlass()
{
    // Once a frame, whichever display paints first.
    const qint64 step = m_sessionEnd.lastStep.restart();
    if (step <= 0) return;
    // A window that was not there when the session began ending is one
    // asking something, a save question or KDE's word that something did not
    // close; the glass clears while it stands.
    bool asking = false;
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        if (!window || window->isDeleted() || !window->isVisible() || m_sessionEnd.known.contains(window)) continue;
        if (window->isNormalWindow() || window->isDialog() || window->isNotification()
            || window->isCriticalNotification())
            asking = true;
    }
    if (asking != m_sessionEnd.asking) {
        m_sessionEnd.asking = asking;
        qInfo() << "Kadunce" << Revision
                << (asking ? "clears the glass for a window asking something" : "brings the glass back");
    }
    // Plasma turns the power off a moment after the windows are asked to
    // close, so the glass darkens at once, and lifts for anything asking.
    const bool darken = !asking;
    if (darken && !m_sessionEnd.darkening)
        qInfo() << "Kadunce" << Revision << "darkens the glass to black";
    m_sessionEnd.darkening = darken;
    const double factor = KWin::effects->animationTimeFactor();
    const auto toward = [step](double &value, double target, int duration) {
        const double delta = double(step) / std::max(1, duration);
        value = target > value ? std::min(target, value + delta) : std::max(target, value - delta);
    };
    toward(m_sessionEnd.frost, asking ? 0.0 : 1.0,
           motionDuration(asking ? SettingsFrostDissolveMs : SettingsFrostFadeMs, factor));
    toward(m_sessionEnd.black, darken ? 1.0 : 0.0,
           motionInstant(factor) ? SessionEndDarkenInstantMs
                                 : motionDuration(darken ? SessionEndDarkenMs : SettingsFrostDissolveMs, factor));
}

void Effect::drawSessionEndGlass(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                                 KWin::LogicalOutput *screen)
{
    if (!screen) return;
    const QRectF whole(screen->geometry());
    QList<QRectF> parts;
    for (const auto &dirty : m_paintingRegion.rects()) {
        const QRectF part = QRectF(viewport.mapFromDeviceCoordinates(KWin::RectF(dirty))).intersected(whole);
        if (!part.isEmpty()) parts.append(part);
    }
    // Over everything on the display, its panels too, as nothing is left to use.
    if (m_sessionEnd.frost > 0.0) {
        const bool drawn = m_settingsFrost.paint(renderTarget, viewport, parts, whole, 0.0, whole,
                                                 float(m_sessionEnd.frost));
        if (drawn) m_frostDrawn[screen->name()].append(whole);
    }
    // The fill's box reaches past the display, so its rounded corners fall
    // outside and black meets every edge.
    const qreal corner = 2.0 * cardCornerRadius();
    if (m_sessionEnd.black > 0.0)
        fillDestinationParts(renderTarget, viewport, parts, whole.adjusted(-corner, -corner, corner, corner),
                             float(m_sessionEnd.black), 0.f, QVector3D(0.f, 0.f, 0.f));
}

void Effect::fillDestinationParts(const KWin::RenderTarget &renderTarget,
    const KWin::RenderViewport &viewport, const QList<QRectF> &parts, const QRectF &box,
    float fill, float ring, std::optional<QVector3D> fillInk)
{
    QList<QVector2D> vertices;
    for (const auto &part : parts) {
        if (part.width() <= 0 || part.height() <= 0) continue;
        vertices << QVector2D(part.topLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomLeft())
                 << QVector2D(part.bottomLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomRight());
    }
    if (!m_destinationShader || vertices.isEmpty()) return;
    {
        KWin::ShaderBinder binder(m_destinationShader.get());
        auto matrix = viewport.projectionMatrix();
        matrix.scale(viewport.scale(), viewport.scale());
        m_destinationShader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
        m_destinationShader->setUniform("destinationBox", QVector4D(box.x(), box.y(), box.width(), box.height()));
        m_destinationShader->setUniform("outlineRadius", float(cardCornerRadius()));
        const QVector3D ink = fillInk.value_or(SurfaceTone::current().lineInk());
        m_destinationShader->setUniform("surfaceFill", QVector4D(ink, fill));
        m_destinationShader->setUniform("outlineOpacity", ring);
        m_destinationShader->setUniform("outlineInk", ink);
        m_destinationShader->setColorspaceUniforms(KWin::ColorDescription::sRGB,
            renderTarget.colorDescription(), KWin::RenderingIntent::Perceptual);
        const bool blended = glIsEnabled(GL_BLEND);
        GLint srcRgb, dstRgb, srcAlpha, dstAlpha;
        glGetIntegerv(GL_BLEND_SRC_RGB, &srcRgb); glGetIntegerv(GL_BLEND_DST_RGB, &dstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        auto *buffer = KWin::GLVertexBuffer::streamingBuffer();
        buffer->reset(); buffer->setVertices(vertices); buffer->render(GL_TRIANGLES);
        glBlendFuncSeparate(srcRgb, dstRgb, srcAlpha, dstAlpha);
        if (!blended) glDisable(GL_BLEND);
    }
}

PaintResult Effect::paintScreen(const KWin::RenderTarget &renderTarget,
                         const KWin::RenderViewport &viewport,
                         int mask,
                         const KWin::Region &deviceRegion,
                         KWin::LogicalOutput *screen)
{
    // §11, Every display: each display paints its own cards.
    SessionScope scope(this, m_currentSession, isCardDisplay(screen) ? stageKeyFor(screen) : m_scopedKey);
    m_paintingOutput = screen;
    m_paintingRegion = deviceRegion;
    m_heldBoxDrawnOn = nullptr;
    m_settingsFrostDrawnOn = nullptr;
    if (screen) m_frostDrawn.remove(screen->name());
    if (!m_heldPictures.empty()) captureHeldPictures();
    if (screen && screen == tabletOutput()) {
        // Only this display's: another's are read until it paints again.
        for (auto it = m_cardLabelTargets.begin(); it != m_cardLabelTargets.end();) {
            if (!it.key() || it.key()->isDeleted() || it.key()->screen() == screen
                || m_cardStage->liveCardIndex(it.key()) >= 0) it = m_cardLabelTargets.erase(it);
            else ++it;
        }
        for (auto it = m_cardFaceRects.begin(); it != m_cardFaceRects.end();) {
            if (!it.key() || it.key()->isDeleted() || it.key()->screen() == screen
                || m_cardStage->liveCardIndex(it.key()) >= 0) it = m_cardFaceRects.erase(it);
            else ++it;
        }
    }
    if (!painted([&] { return KWin::effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen); })) return paintResult(false);
    if (m_sessionEnd.on) drawSessionEndGlass(renderTarget, viewport, screen);
    // §11: an offered card's picture is kept once the scene has drawn it this
    // frame, so a sleeping window's newest content is what is kept; taken
    // before, a window just woken for the offer can still have none.
    if (m_roomOffer && screen == m_roomOffer->output) captureOfferPictures();
    // Labels, rails and outlines belong to the current desktop's presentation.
    if (m_previewDesktop) return paintResult(true);
    // Where anything below is painted over the scene, and so over the
    // pointer when KWin composites it.
    QList<QRectF> overlaid;
    // A display's dividers are drawn while it shows its layout, its cards
    // waiting behind it, as they are taken (beginRailFromInput).
    if (m_destinationShader && screen
        && (screen != tabletOutput() || !m_cardStage->isActive() || m_cardStage->presentsBento())
        && !m_carriedWindow && !m_cardStage->cardGrabActive()) {
        QList<QRectF> pills;
        for (const auto &rail : m_desktopStage->grabRails())
            if (rail.output == screen->name()) pills.append(rail.pill);
        const auto previews = m_desktopStage->railPreview(screen->name());
        const auto drawRailShape = [&](const QRectF &box, bool pill) {
            QList<QVector2D> vertices;
            for (const auto &dirty : deviceRegion.rects()) {
                const auto part = QRectF(viewport.mapFromDeviceCoordinates(KWin::RectF(dirty)))
                    .intersected(QRectF(screen->geometry())).intersected(box);
                if (part.isEmpty()) continue;
                vertices << QVector2D(part.topLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomLeft())
                    << QVector2D(part.bottomLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomRight());
            }
            if (vertices.isEmpty()) return;
            KWin::ShaderBinder binder(m_destinationShader.get());
            auto matrix = viewport.projectionMatrix();
            matrix.scale(viewport.scale(), viewport.scale());
            m_destinationShader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
            m_destinationShader->setUniform("destinationBox", QVector4D(box.x(),box.y(),box.width(),box.height()));
            m_destinationShader->setUniform("outlineRadius", pill ? 2.f : float(cardCornerRadius()));
            const QVector3D ink = SurfaceTone::current().lineInk();
            m_destinationShader->setUniform("surfaceFill", QVector4D(ink, pill ? .85f : .035f));
            m_destinationShader->setUniform("outlineOpacity", pill ? 0.f : .8f);
            m_destinationShader->setUniform("outlineInk", ink);
            m_destinationShader->setColorspaceUniforms(KWin::ColorDescription::sRGB,
                renderTarget.colorDescription(), KWin::RenderingIntent::Perceptual);
            const bool blended = glIsEnabled(GL_BLEND);
            GLint sr,dr,sa,da;
            glGetIntegerv(GL_BLEND_SRC_RGB,&sr); glGetIntegerv(GL_BLEND_DST_RGB,&dr);
            glGetIntegerv(GL_BLEND_SRC_ALPHA,&sa); glGetIntegerv(GL_BLEND_DST_ALPHA,&da);
            glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
            auto *buffer = KWin::GLVertexBuffer::streamingBuffer();
            buffer->reset(); buffer->setVertices(vertices); buffer->render(GL_TRIANGLES);
            glBlendFuncSeparate(sr,dr,sa,da);
            if (!blended) glDisable(GL_BLEND);
        };
        for (const auto &box : previews) drawRailShape(box,false);
        if (!previews.isEmpty()) for (const auto &pill : pills) drawRailShape(pill,true);
        if (!previews.isEmpty()) overlaid << previews << pills;
    }
    // Draw only from a still-owned reservation. No layout solve, timers, native
    // outline window, or input grab belongs in the paint pass.
    const auto &reservation = m_carriedWindow ? m_carryDestination : m_lineDestination;
    bool detachShown = false;
    const auto cardEntry = m_carriedWindow ? m_carryCardEntryOutput : m_lineCardEntryOutput;
    const auto cardExit = m_carriedWindow ? m_carryCardExitOutput : QPointer<KWin::LogicalOutput>();
    if (const auto held = heldDestinationOn(screen)) {
        const QRectF box = *held;
        // Drawn under the window in hand when it painted; otherwise here.
        if (m_heldBoxDrawnOn != screen) drawDestinationOutline(renderTarget, viewport, deviceRegion, screen, box);
        overlaid.append(box);
        // The outline above is drawn for a Bento reservation, a Card Stage
        // entry or a card leaving at the bottom edge. §10 gives the bottom edge
        // the only detaching release, so a pane leaving its layout and a card
        // leaving its display are labelled, and an entry never is. A display
        // whose context line is claimed has the words said there instead.
        if ((cardExit && cardExit == screen)
            || (reservation && !cardEntry && reservation->detachesToDesktop())) {
            detachShown = true;
            if (!contextLineClaimed(screen)) m_detachLabel.render(renderTarget, viewport, box);
        }
    }
    if (screen && detachShown != (m_detachShownOn == screen))
        m_detachShownOn = detachShown ? screen : nullptr;
    drawStackLadderOutline(renderTarget, viewport, deviceRegion, screen);
    drawRoomOffer(renderTarget, viewport, deviceRegion, screen, overlaid);
    // Where a placement request is aimed, as a carried card's destination is.
    if (m_destinationShader && screen && m_placementPreview && m_placementPreviewOutput == screen) {
        drawDestinationOutline(renderTarget, viewport, deviceRegion, screen, QRectF(*m_placementPreview));
        overlaid.append(QRectF(*m_placementPreview));
    }
    if (screen && screen == tabletOutput() && m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Spread) {
        const auto &model = m_cardStage->model();
        const auto &windows = m_cardStage->liveCards();
        // A held card is above the row, so no name of the row shows through it.
        auto *held = m_cardStage->cardGrabActive() ? m_cardStage->selectedWindow() : nullptr;
        const QRectF heldRect = held ? m_cardLabelTargets.value(held) : QRectF();
        // The names, and a Stack's place, are said on a claimed context line.
        const bool namesSaid = contextLineClaimed(screen);
        for (int index = 0; !namesSaid && index < windows.size(); ++index) {
            auto *window = windows.at(index).data();
            if (!window || window->isDeleted() || m_cardStage->paintSlot(window) == 99)
                continue;
            const int cardId = index + 1;
            if (model.stackPositionForId(cardId)
                != model.stackActivePositionForId(cardId)) continue;
            const QString applicationName = spreadCardName(window, cardId);
            const int count = model.stackSizeForId(cardId);
            const auto target = m_cardLabelTargets.constFind(window);
            if (target == m_cardLabelTargets.cend()) continue;
            if (window != held && !heldRect.isEmpty()
                && CardLabelRenderer::labelRect(*target).intersects(heldRect)) continue;
            m_cardLabelRenderer.render(renderTarget, viewport,
                *target,
                {applicationName, stackPositionLabel(
                    m_cardStage->shownStackPosition(cardId), count)});
            overlaid.append(CardLabelRenderer::labelRect(*target));
        }
        if (isInUseDisplay(screen) || !m_cardsOnEveryDisplay)
            paintStuckNotes(renderTarget, viewport, deviceRegion, screen, heldRect, overlaid);
    }
    // Spread just closed onto the Active card: its notes as Spread drew them,
    // now where Gooseberry draws them, until Gooseberry's own are there.
    if (m_notesHandover && screen && screen == tabletOutput() && (isInUseDisplay(screen) || !m_cardsOnEveryDisplay))
        paintNotesHandover(renderTarget, viewport, overlaid);
    // A refused gesture's note, over everything on the display it was let go on.
    if (m_gestureNote && screen && m_gestureNote->output == screen && !contextLineClaimed(screen)) {
        m_gestureNoteLabel.render(renderTarget, viewport, m_gestureNote->box, m_gestureNote->text,
                                  gestureNoteOpacity(m_gestureNote->shown.elapsed()));
        overlaid.append(m_gestureNote->box);
    }
    // The pointer stays above everything Kadunce draws.
    const bool pointerPainted = paintPointerAbove(renderTarget, viewport, deviceRegion, screen, overlaid);
    m_paintingOutput = nullptr;
    return paintResult(pointerPainted);
}

int Effect::liveCardIndex(const KWin::EffectWindow *window) const
{
    return m_cardStage->liveCardIndex(window);
}

int Effect::visibleSlot(const KWin::EffectWindow *window) const
{
    return m_cardStage->visibleSlot(window);
}

KWin::Rect Effect::cardTargetForSlot(KWin::LogicalOutput *output,
                                    int slot) const
{
    return m_cardStage->cardTargetForSlot(output, slot);
}

KWin::Rect Effect::activeTarget(KWin::LogicalOutput *output) const
{
    return m_cardStage->activeTarget(output);
}

bool Effect::drawsOutsideFrame(const KWin::EffectWindow *window) const
{
    // A fractional scale can leave what KWin draws of a window a hair past
    // its frame with nothing there; a shadow lies a whole pixel or more out.
    constexpr double Slack = 0.5;
    const QRectF frame(window->frameGeometry());
    const QRectF drawn(window->expandedGeometry());
    return drawn.left() < frame.left() - Slack || drawn.top() < frame.top() - Slack
        || drawn.right() > frame.right() + Slack || drawn.bottom() > frame.bottom() + Slack;
}

bool Effect::trimsCorners(KWin::EffectWindow *window) const
{
    if (!roundsCorners()) return false;
    // Application windows and their dialogs, with or without a title bar;
    // panels, pop-ups and the keys keep their own shapes.
    if (!(window->isNormalWindow() || window->isDialog()) || window->isFullScreen()) return false;
    // A shadow lies outside the frame, and the trim would cut it away. A
    // window that draws its own shadow draws its own corners too.
    if (drawsOutsideFrame(window)) return false;
    // A maximized window meets the display's edges, which stay square.
    const KWin::Window *client = window->window();
    return !(client && client->maximizeMode() == KWin::MaximizeFull);
}

bool Effect::castsShadow(KWin::EffectWindow *window)
{
    if (!m_shadowShader || !cardShadowShape(cardTuning().shadow).cast()) return false;
    if (!window || window->isDeleted() || !window->isVisible() || !trimsCorners(window)) return false;
    if (m_table.isOpen()) return false;
    // Each display has its own Spread, and only its own cards stand in it.
    SessionScope scope(this, m_currentSession, stageKeyOf(window));
    return !(m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread);
}

void Effect::traceShadows(KWin::ScreenPrePaintData &data)
{
    // A window that comes to cast one is traced from its first frame.
    if (cardShadowShape(cardTuning().shadow).cast()) {
        const auto windows = KWin::effects->stackingOrder();
        for (KWin::EffectWindow *window : windows)
            if (!m_shadowTraces.contains(window) && castsShadow(window)) m_shadowTraces.insert(window, {});
    }
    // KWin redraws where a window moved to and from, and a shadow reaches
    // past both: the frame that moves it redraws where its shadow was drawn
    // and where it now falls.
    KWin::Region changed;
    for (auto it = m_shadowTraces.begin(); it != m_shadowTraces.end();) {
        const QRectF now = castsShadow(it.key()) ? shadowExtent(QRectF(it.key()->frameGeometry())) : QRectF();
        if (now != it->expected) {
            changed |= KWin::RectF(it->drawn.united(now)).toAlignedRect();
            changed |= KWin::RectF(it->expected).toAlignedRect();
            it->expected = now;
        }
        if (now.isEmpty()) {
            changed |= KWin::RectF(it->drawn).toAlignedRect();
            it = m_shadowTraces.erase(it);
        } else {
            ++it;
        }
    }
    if (changed.isEmpty()) return;
    data.paint |= changed;
    // The same shadow can fall on another display, drawn in its own frame.
    KWin::effects->addRepaint(changed);
}

void Effect::paintCardShadow(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                             KWin::EffectWindow *window, const KWin::Region &deviceRegion,
                             const KWin::WindowPaintData &data)
{
    // A tilted card is one Spread or Table draws, and they cast none.
    if (!qFuzzyIsNull(data.rotationAngle()) || data.opacity() <= 0.0) return;
    const auto trace = m_shadowTraces.find(window);
    if (trace == m_shadowTraces.end()) return;
    const QRectF frame(window->frameGeometry());
    const QRectF card(frame.x() + data.xTranslation(), frame.y() + data.yTranslation(),
                      frame.width() * data.xScale(), frame.height() * data.yScale());
    const QRectF extent = shadowExtent(card);
    if (extent != trace->drawn) {
        m_shadowRepaint |= KWin::RectF(trace->drawn.united(extent)).toAlignedRect();
        trace->drawn = extent;
    }
    if (extent.isEmpty()) return;
    const CardShadowShape shape = cardShadowShape(cardTuning().shadow);
    // The ring around the card, not the card: it is drawn over by the window.
    QList<QVector2D> vertices;
    const auto quad = [&vertices](const QRectF &part) {
        if (part.width() <= 0 || part.height() <= 0) return;
        vertices << QVector2D(part.topLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomLeft())
                 << QVector2D(part.bottomLeft()) << QVector2D(part.topRight()) << QVector2D(part.bottomRight());
    };
    // Inside its corners the card is cut away, so the band along each edge
    // reaches in as far as the corners do.
    const double inset = std::min({cardCornerRadius(), card.width() / 2.0, card.height() / 2.0});
    const QRectF inner = card.adjusted(inset, inset, -inset, -inset);
    quad(QRectF(extent.left(), extent.top(), extent.width(), inner.top() - extent.top()));
    quad(QRectF(extent.left(), inner.bottom(), extent.width(), extent.bottom() - inner.bottom()));
    quad(QRectF(extent.left(), inner.top(), inner.left() - extent.left(), inner.height()));
    quad(QRectF(inner.right(), inner.top(), extent.right() - inner.right(), inner.height()));
    if (vertices.isEmpty()) return;
    KWin::ShaderBinder binder(m_shadowShader.get());
    auto matrix = viewport.projectionMatrix();
    matrix.scale(viewport.scale(), viewport.scale());
    m_shadowShader->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
    m_shadowShader->setUniform("cardBox", QVector4D(card.x(), card.y(), card.width(), card.height()));
    m_shadowShader->setUniform("cardRadius", float(cardCornerRadius()));
    m_shadowShader->setUniform("shadowBlur", float(shape.blur));
    m_shadowShader->setUniform("shadowOffset", float(shape.offset));
    m_shadowShader->setUniform("shadowOpacity", float(shape.opacity * data.opacity()));
    m_shadowShader->setColorspaceUniforms(KWin::ColorDescription::sRGB,
        renderTarget.colorDescription(), KWin::RenderingIntent::Perceptual);
    const bool blended = glIsEnabled(GL_BLEND);
    const bool scissored = glIsEnabled(GL_SCISSOR_TEST);
    GLint previousScissor[4];
    glGetIntegerv(GL_SCISSOR_BOX, previousScissor);
    GLint sr, dr, sa, da;
    glGetIntegerv(GL_BLEND_SRC_RGB, &sr); glGetIntegerv(GL_BLEND_DST_RGB, &dr);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &sa); glGetIntegerv(GL_BLEND_DST_ALPHA, &da);
    glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    auto *buffer = KWin::GLVertexBuffer::streamingBuffer();
    glEnable(GL_SCISSOR_TEST);
    buffer->reset(); buffer->setVertices(vertices); buffer->render(deviceRegion, GL_TRIANGLES, true);
    glScissor(previousScissor[0], previousScissor[1], previousScissor[2], previousScissor[3]);
    if (!scissored) glDisable(GL_SCISSOR_TEST);
    glBlendFuncSeparate(sr, dr, sa, da);
    if (!blended) glDisable(GL_BLEND);
}

bool Effect::redirectSource(KWin::EffectWindow *window, bool reportChange)
{
    const QRectF frame(window->frameGeometry());
    const std::array<QRectF, 3> sourceBounds = {
        QRectF(QPointF(), frame.size()),
        QRectF(window->bufferGeometry()).translated(-frame.topLeft()),
        QRectF(window->expandedGeometry()).translated(-frame.topLeft())};
    const auto previousBounds = m_previewSourceBounds.constFind(window);
    const bool fresh = previousBounds == m_previewSourceBounds.cend();
    const bool changed = !fresh && *previousBounds != sourceBounds;
    if (changed) {
        // Damage alone does not describe a changed frame/buffer mapping.
        if (reportChange)
            qInfo() << "Kadunce preview source bounds changed" << logNameOf(window)
                    << "frame" << (*previousBounds)[0] << "->" << sourceBounds[0]
                    << "buffer" << (*previousBounds)[1] << "->" << sourceBounds[1]
                    << "expanded" << (*previousBounds)[2] << "->" << sourceBounds[2];
        unredirect(window);
    }
    m_previewSourceBounds.insert(window, sourceBounds);
    redirect(window);
    return fresh || changed;
}

void Effect::redirectPreviewSource(KWin::EffectWindow *window)
{
    if (redirectSource(window, true)) ++m_framePreviewsMade;
}

PaintResult Effect::drawTrimmedWindow(const KWin::RenderTarget &renderTarget,
                                      const KWin::RenderViewport &viewport,
                                      KWin::EffectWindow *window,
                                      int mask,
                                      const KWin::Region &deviceRegion,
                                      KWin::WindowPaintData &data)
{
    // The window's own picture is cut to the paper tier before any transform,
    // so its corners travel, scale and fade with it.
    (void)redirectSource(window, false);
    setShader(window, m_fanApertureShader.get());
    KWin::ShaderManager::instance()->pushShader(m_fanApertureShader.get());
    const double scale = viewport.scale();
    const QSizeF frame = window->frameGeometry().size();
    m_fanApertureShader->setUniform(m_fanPaintSizeLocation, QVector2D(1.0F, 1.0F));
    m_fanApertureShader->setUniform(m_fanApertureOriginLocation, QVector2D(0.0F, 0.0F));
    m_fanApertureShader->setUniform(m_fanApertureSizeLocation,
        QVector2D(static_cast<float>(std::round(frame.width() * scale)),
                  static_cast<float>(std::round(frame.height() * scale))));
    m_fanApertureShader->setUniform(m_fanApertureRadiusLocation,
        static_cast<float>(cardCornerRadius() * scale));
    m_fanApertureShader->setUniform("tiltedSampling",
        qFuzzyIsNull(data.rotationAngle()) ? 0.0F : 1.0F);
    glActiveTexture(GL_TEXTURE0);
    const bool drawn = painted([&] { return KWin::OffscreenEffect::drawWindow(
        renderTarget, viewport, window, mask, deviceRegion, data); });
    KWin::ShaderManager::instance()->popShader();
    return paintResult(drawn);
}

PaintResult Effect::drawWindow(const KWin::RenderTarget &renderTarget,
                        const KWin::RenderViewport &viewport,
                        KWin::EffectWindow *window,
                        int mask,
                        const KWin::Region &deviceRegion,
                        KWin::WindowPaintData &data)
{
    // The picture kept of a card is the window as it stands, drawn plainly.
    if (window == m_capturingHeld)
        return paintResult(painted([&] { m_drawnAs.insert(window, QStringLiteral("kept picture taken")); return KWin::Effect::drawWindow(
            renderTarget, viewport, window, mask, deviceRegion, data); }));
    if (m_shadowTraces.contains(window) && window != m_fanApertureWindow && castsShadow(window))
        paintCardShadow(renderTarget, viewport, window, deviceRegion, data);
    if (const auto offered = m_offerPictures.find(window);
        offered != m_offerPictures.end() && m_roomOfferShown.contains(window)
        && offered->second.draw(viewport, renderTarget, window, deviceRegion, data)) {
        m_drawnAs.insert(window, QStringLiteral("offered picture"));
        return paintResult(true);
    }
    if (const auto held = m_heldPictures.find(window); held != m_heldPictures.end()) {
        // Let go once the app has drawn twice at its new size.
        if (!held->second.resize && held->second.drawnSince >= 2) releaseHeldPicture(window);
        else if (held->second.picture.draw(viewport, renderTarget, window, deviceRegion, data)) {
            m_drawnAs.insert(window, QStringLiteral("kept picture"));
            return paintResult(true);
        }
    }
    const bool useFanAperture = window == m_fanApertureWindow
        && m_fanApertureShader
        && !m_fanPaintSize.isEmpty()
        && !m_fanApertureSize.isEmpty()
        && m_fanPaintSizeLocation >= 0
        && m_fanApertureOriginLocation >= 0
        && m_fanApertureSizeLocation >= 0
        && m_fanApertureRadiusLocation >= 0;

    if (!useFanAperture) {
        if (trimsCorners(window)) {
            m_drawnAs.insert(window, QStringLiteral("trimmed"));
            return drawTrimmedWindow(renderTarget, viewport, window, mask, deviceRegion, data);
        }
        if (const float outline = m_keyboardEdgeShader ? keyboardOutlineOpacity(window, data) : 0.0F;
            outline > 0.0F) {
            m_drawnAs.insert(window, QStringLiteral("outlined"));
            (void)redirectSource(window, false);
            setShader(window, m_keyboardEdgeShader.get());
            KWin::ShaderManager::instance()->pushShader(m_keyboardEdgeShader.get());
            const QColor ink = keyboardOutlineInk();
            m_keyboardEdgeShader->setUniform("outlineInk", QVector3D(ink.redF(), ink.greenF(), ink.blueF()));
            m_keyboardEdgeShader->setUniform("outlineOpacity", outline);
            m_keyboardEdgeShader->setUniform("bandTexels", float(2.0 * viewport.scale()));
            // How far the picture reaches past the frame, for its shadow, and
            // how far an application's own corner reaches in from it.
            const QRectF frame(window->frameGeometry());
            const QRectF picture(window->expandedGeometry());
            const double margin = std::max({frame.left() - picture.left(), frame.top() - picture.top(),
                picture.right() - frame.right(), picture.bottom() - frame.bottom(), 0.0});
            m_keyboardEdgeShader->setUniform("frameMargin", float(margin * viewport.scale()));
            m_keyboardEdgeShader->setUniform("cornerReach", float(24.0 * viewport.scale()));
            glActiveTexture(GL_TEXTURE0);
            const bool drawn = painted([&] { return KWin::OffscreenEffect::drawWindow(
                renderTarget, viewport, window, mask, deviceRegion, data); });
            KWin::ShaderManager::instance()->popShader();
            return paintResult(drawn);
        }
        m_drawnAs.insert(window, QStringLiteral("plain"));
        // Redirection is otherwise ephemeral. A window Kadunce does not trim,
        // hidden deck members, and shader failure all return
        // to KWin's exact r20 direct path.
        unredirect(window);
        m_previewSourceBounds.remove(window);
        if (!painted([&] { return KWin::OffscreenEffect::drawWindow(
            renderTarget, viewport, window, mask, deviceRegion, data); })) return paintResult(false);
        return paintResult(true);
    }

    m_drawnAs.insert(window, QStringLiteral("aperture"));
    redirectPreviewSource(window);
    setShader(window, m_fanApertureShader.get());
    KWin::ShaderManager::instance()->pushShader(
        m_fanApertureShader.get());
    m_fanApertureShader->setUniform(
        m_fanPaintSizeLocation,
        QVector2D(static_cast<float>(m_fanPaintSize.width()),
                  static_cast<float>(m_fanPaintSize.height())));
    m_fanApertureShader->setUniform(
        m_fanApertureOriginLocation,
        QVector2D(static_cast<float>(m_fanApertureOrigin.x()),
                  static_cast<float>(m_fanApertureOrigin.y())));
    m_fanApertureShader->setUniform(
        m_fanApertureSizeLocation,
        QVector2D(static_cast<float>(m_fanApertureSize.width()),
                  static_cast<float>(m_fanApertureSize.height())));
    m_fanApertureShader->setUniform(
        m_fanApertureRadiusLocation, m_fanApertureRadius);
    m_fanApertureShader->setUniform("tiltedSampling",
        qFuzzyIsNull(data.rotationAngle()) ? 0.0F : 1.0F);
    glActiveTexture(GL_TEXTURE0);

    const bool drawn = painted([&] { return KWin::OffscreenEffect::drawWindow(
        renderTarget, viewport, window, mask, deviceRegion, data); });
    KWin::ShaderManager::instance()->popShader();
    if (!drawn) return paintResult(false);
    // Remain inside the draw-chain callback: OffscreenEffect's internal render
    // must continue AFTER this effect, not re-enter our drawWindow from paintScreen.
    // An empty final clip populates the live texture without painting on any output.
    if (!m_neighborPreparedThisFrame && m_paintingOutput
        && isTabletOutput(m_paintingOutput) && !m_preparationNeighbors.isEmpty()) {
        m_neighborPreparedThisFrame = true;
        if (m_neighborPreparationFrames > 0 && --m_neighborPreparationFrames > 0)
            m_continueRepaint = true; // only enough frames to visit both neighbors
        const auto neighbor = m_preparationNeighbors.at(
            m_neighborPreparationCursor++ % m_preparationNeighbors.size());
        if (neighbor && !neighbor->isDeleted() && neighbor != m_nativeCarry
            && m_cardStage->visibleSlot(neighbor) == 99 && neighbor->screen()) {
            const auto size = neighbor->expandedGeometry().size();
            const double scale = neighbor->screen()->scale();
            // At most two extra full-window surfaces, each bounded to32MiB.
            if (size.width() > 0 && size.height() > 0
                && size.width()*size.height()*scale*scale*4 <= 32*1024*1024) {
                redirectPreviewSource(neighbor);
                setShader(neighbor, nullptr);
                KWin::WindowPaintData preparation;
                if (!painted([&] { return KWin::OffscreenEffect::drawWindow(renderTarget, viewport, neighbor,
                    PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT,
                    KWin::Region(), preparation); })) return paintResult(false);
            }
        }
    }
    return paintResult(true);
}

PaintResult Effect::paintWindow(const KWin::RenderTarget &renderTarget,
                         const KWin::RenderViewport &viewport,
                         KWin::EffectWindow *window,
                         int mask,
                         const KWin::Region &deviceRegion,
                         KWin::WindowPaintData &data)
{
    drawSettingsFrostUnder(renderTarget, viewport, window);
    // A panel's fade laid under the windows is drawn over the frost instead.
    if (fadeOverSettingsFrost(window)) return paintResult(true);
    // The power question asks KDE's blur for its whole display, which would
    // haze the glass and the panels alike: it is drawn as moved, which the
    // blur leaves alone, so the glass under it shows as it is.
    if (isPowerQuestion(window) && m_paintingOutput && settingsSurfaceOn(m_paintingOutput) == window) {
        if (!painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window,
                mask | PAINT_WINDOW_TRANSFORMED, deviceRegion, data); })) return paintResult(false);
        return paintResult(true);
    }
    // Hidden with its application even before KWin is told, so a dialog that
    // opens behind the card in front is never seen for a frame.
    if (isDependentWindow(window) && !dependentShown(dependentLead(window))) return paintResult(true);
    // Drawn with its card in hand, after the card.
    if (drawnWithCarriedCard(window)) return paintResult(true);
    // Gooseberry's notes over a window, while Spread draws them on its card.
    if (notesSurfaceStepsAside(window)) return paintResult(true);
    // Keys are drawn only once they are known to be the person's.
    if (!m_keysForPerson && window == KWin::effects->inputPanel()) return paintResult(true);
    // A card flicked closed stays out of sight while its app closes.
    if (m_cardStage->thrownAway(window)) return paintResult(true);
    // A card a Stack steps to waits unseen until it stands in its place.
    if (steppingInUnseen(window)) return paintResult(true);
    // A previewed desktop is drawn as it stands, with nothing of the current
    // desktop's presentation over or under it.
    if (m_previewDesktop) {
        if (!hiddenByDesktopPreview(window))
            if (!painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window, mask, deviceRegion, data); })) return paintResult(false);
        return paintResult(true);
    }
    // §11, Connected: a Stack scrubbed draws its ladder over its pane, and
    // the cards on it nowhere else.
    if (m_stackScrub.active() && (window == m_stackScrubPane || m_stackScrubShown.contains(window))) {
        const auto rung = stackScrubRung(window);
        if (!rung || !m_paintingOutput || m_stackScrubPane->screen() != m_paintingOutput)
            return paintResult(true);
        const auto [box, shows] = *rung;
        const QSizeF size = QRectF(window->frameGeometry()).size();
        if (size.isEmpty()) return paintResult(true);
        // Its own shape, as wide as its rung, its title bar at the rung's top.
        const KWin::Rect target = KWin::RectF(QRectF(box.x(), box.y(), box.width(),
            box.width() * size.height() / size.width())).toRect();
        KWin::Rect logicalRegion = window->expandedGeometry().toRect();
        setPositionTransformations(data, logicalRegion, window, target, Qt::KeepAspectRatio);
        const auto deviceTarget = viewport.mapToDeviceCoordinatesAligned(target);
        const auto clip = deviceRegion
            & KWin::Region(viewport.mapToDeviceCoordinatesAligned(KWin::RectF(shows.intersected(box))))
            & roundedClip(deviceTarget, cardCornerRadius() * viewport.scale());
        if (!painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window,
            mask | PAINT_WINDOW_TRANSFORMED, clip, data); })) return paintResult(false);
        return paintResult(true);
    }
    // §11, Connected: a card the open room offers is drawn as its picture
    // there, and nowhere else.
    if (m_roomOffer && m_roomOfferShown.contains(window)) {
        const int index = m_roomOffer->cards.indexOf(window);
        if (index < 0 || index >= m_roomOffer->tiles.size() || !m_paintingOutput
            || m_paintingOutput != m_roomOffer->output) return paintResult(true);
        const QRectF tile = m_roomOffer->tiles.at(index);
        const KWin::Rect target = KWin::RectF(tile).toRect();
        KWin::Rect logicalRegion = window->expandedGeometry().toRect();
        // On the card's backing, as in Spread, so a see-through app still
        // reads as a card.
        // Rounded at its own corners, a narrow picture as a wide one.
        const QRectF picture = offerPicture(tile, QRectF(window->frameGeometry()).size());
        if (logicalRegion.width() > 0 && logicalRegion.height() > 0)
            paintCardSurface(m_destinationShader.get(), renderTarget, viewport,
                deviceRegion & KWin::Region(viewport.mapToDeviceCoordinatesAligned(KWin::RectF(picture))),
                picture, 0.0, float(data.opacity()), 0.0f, SurfaceTone::current().cardBacking(), 1.0F);
        setPositionTransformations(data, logicalRegion, window, target, Qt::KeepAspectRatio);
        const auto deviceTarget = viewport.mapToDeviceCoordinatesAligned(KWin::RectF(picture));
        const auto clip = deviceRegion
            & KWin::Region(deviceTarget)
            & roundedClip(deviceTarget, cardCornerRadius() * viewport.scale());
        if (!painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window,
            mask | PAINT_WINDOW_TRANSFORMED, clip, data); })) return paintResult(false);
        return paintResult(true);
    }
    if (hiddenOnOtherDesktop(window)) return paintResult(true);
    // A sleeping card stands in Spread dimmed, so it can be found and woken,
    // unless the context line says so instead and the card stands whole, or
    // a person has chosen either for every sleeping card.
    if (m_sleepingShown.contains(window)) {
        const std::optional<bool> chosen = cardTuning().sleepingDimmed;
        if (chosen ? *chosen : !contextLineClaimed(tabletOutput())) data.multiplyOpacity(SleepingCardOpacity);
    }
    if (guestNeighborOpacity() <= 0.0 && m_cardStage->launcherGuestActive()
        && m_paintingOutput == tabletOutput() && isCardWindow(window)
        && m_cardStage->paintSlot(window) != 99) return paintResult(true);
    const auto settle = window == m_settlingWindow ? dropSettleRect() : bentoMotionRect(window);
    if (settle && window != m_settlingWindow && m_paintingOutput
        && window->window()->moveResizeOutput() != m_paintingOutput) return paintResult(true);
    // A pane that gave way fades where it stood, wherever its window now is.
    std::optional<QRectF> yieldingRect;
    if (!settle && window != m_carriedWindow) {
        if (const auto opacity = yieldingPaneOpacity(window)) {
            for (const auto &yielding : std::as_const(m_yieldingPanes))
                if (yielding.window == window) yieldingRect = yielding.rect;
            if (yieldingRect) data.multiplyOpacity(*opacity);
        }
    }
    if (((window == m_carriedWindow && m_carryRuntime) || settle || yieldingRect) && m_paintingOutput) {
        const auto drawnAt = settle ? settle : yieldingRect;
        if (window == m_carriedWindow && !drawnAt) drawHeldBoxUnder(renderTarget, viewport);
        const auto plan = drawnAt
            ? carryPaintPlan(*drawnAt, drawnAt->topLeft(), QRectF(m_paintingOutput->geometry()))
            : carryPaintPlan(m_carryPickup, m_carryRuntime->handoff.carry().position(),
                             QRectF(m_paintingOutput->geometry()));
        if (!plan || plan->clip.isEmpty()) return paintResult(true);
        KWin::Rect logicalRegion = window->expandedGeometry().toRect();
        // Leaning into a Stack it would join, it shrinks to 80% about the
        // pointer and tilts 1.5 degrees about its centre.
        const double lean = window == m_carriedWindow && !drawnAt ? stackLean() : 0.0;
        const QRectF leaned = leanedIntoStack(plan->target, lean);
        const KWin::Rect target = KWin::RectF(leaned).toRect();
        setPositionTransformations(data, logicalRegion, window, target, Qt::KeepAspectRatioByExpanding);
        if (lean > 0.0 && data.xScale() > 0 && data.yScale() > 0) {
            data.setRotationAngle(1.5 * lean);
            data.setRotationOrigin(QVector3D((target.x() + target.width() / 2.0 - logicalRegion.x()) / data.xScale(),
                (target.y() + target.height() / 2.0 - logicalRegion.y()) / data.yScale(), 0.0));
        }
        const int tiltReach = lean > 0.0
            ? int(std::ceil(std::sin(1.5 * lean * M_PI / 180.0) * std::max(target.width(), target.height()))) + 2 : 0;
        const auto deviceTarget = viewport.mapToDeviceCoordinatesAligned(target);
        const bool rounded = m_fanApertureShader && data.xScale() > 0 && data.yScale() > 0;
        m_fanApertureWindow = rounded ? window : nullptr;
        m_fanPaintSize = QSizeF(data.xScale(), data.yScale());
        m_fanApertureOrigin = QPointF((target.x() - logicalRegion.x()) * viewport.scale(),
                                      (target.y() - logicalRegion.y()) * viewport.scale());
        m_fanApertureSize = QSizeF(deviceTarget.size());
        m_fanApertureRadius = cardCornerRadius() * viewport.scale();
        // Tilted, its corners reach past the box it was carried in: the
        // display, not that box, bounds them.
        const QRectF bounds = tiltReach > 0
            ? leaned.adjusted(-tiltReach, -tiltReach, tiltReach, tiltReach).intersected(QRectF(m_paintingOutput->geometry()))
            : plan->clip;
        const auto clip = deviceRegion
            & KWin::Region(viewport.mapToDeviceCoordinatesAligned(KWin::RectF(bounds)))
            & (tiltReach > 0 ? KWin::Region(viewport.mapToDeviceCoordinatesAligned(
                                   KWin::RectF(leaned.adjusted(-tiltReach, -tiltReach, tiltReach, tiltReach))))
               : rounded ? KWin::Region(deviceTarget) : roundedClip(deviceTarget, cardCornerRadius() * viewport.scale()));
        const bool drawn = painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window, mask | PAINT_WINDOW_TRANSFORMED, clip, data); });
        m_fanApertureWindow = nullptr; m_fanPaintSize = {}; m_fanApertureOrigin = {};
        m_fanApertureSize = {}; m_fanApertureRadius = 0;
        if (!drawn) return paintResult(false);
        if (window == m_dialogsLead)
            return paintResult(paintDialogsOn(renderTarget, viewport, window, carriedDialogs(), clip, data));
        return paintResult(true);
    }
    // A Bento pane is the desktop stage's to paint. Card Stage hides what it
    // owns, and a pane is not one of its cards, so it must not be routed here.
    // While the desktop shows, what Card Stage does not own is drawn as it
    // stands, and its cards are held aside where KWin draws nothing.
    if (!m_cardStage->isActive() || !m_paintingOutput
        || !(isCardWindow(window) || m_sleepingShown.contains(window))
        || m_desktopStage->managesWindow(window)
        || (m_cardStage->presentation() == CardPresentation::Desktop
            && m_cardStage->liveCardIndex(window) < 0)) {
        if (!painted([&] { return KWin::effects->paintWindow(
            renderTarget, viewport, window, mask, deviceRegion, data); })) return paintResult(false);
        paintKeyboardOutline(renderTarget, viewport, window, deviceRegion, data);
        return paintResult(true);
    }

    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet) {
        if (!painted([&] { return KWin::effects->paintWindow(
            renderTarget, viewport, window, mask, deviceRegion, data); })) return paintResult(false);
        return paintResult(true);
    }

    const int slot = m_cardStage->paintSlot(window);
    const bool grabbedWindow = m_cardStage->cardGrabActive()
        && window == m_cardStage->selectedWindow();
    const auto route = cardPaintRoute(m_paintingOutput == tablet,
        window->screen() == tablet, slot != 99, window == m_nativeCarry,
        grabbedWindow);
    if (route == CardPaintRoute::Hidden) return paintResult(true);
    if (route == CardPaintRoute::Native) {
        // Only the carried item may cross the fence. Passive tablet neighbors
        // remain hidden externally; the carrier is clipped per output.
        const auto outputClip = viewport.mapToDeviceCoordinatesAligned(m_paintingOutput->geometry());
        if (!painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window, mask,
            window == m_nativeCarry ? deviceRegion & KWin::Region(outputClip) : deviceRegion, data); })) return paintResult(false);
        return paintResult(true);
    }

    if (m_cardStage->presentation() == CardPresentation::Active) {
        // While the keys type into it, the card ends a gutter above them on
        // every frame, whatever size its client has drawn yet.
        if (const auto edge = m_cardStage->keyboardRoomEdge(window)) {
            const KWin::RectF output = m_paintingOutput->geometry();
            const KWin::RectF above(output.x(), output.y(), output.width(),
                                    std::max(0.0, *edge - output.y()));
            if (!painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window, mask,
                deviceRegion & KWin::Region(viewport.mapToDeviceCoordinatesAligned(above)), data); })) return paintResult(false);
            return paintResult(true);
        }
        if (const auto shift = m_cardStage->activeSlideShift(window)) {
            // The strip: each card moves with it, and stops at the display's
            // edge rather than reaching the next display.
            data.translate(*shift, 0.0);
            const auto outputClip = viewport.mapToDeviceCoordinatesAligned(m_paintingOutput->geometry());
            if (!painted([&] { return KWin::effects->paintWindow(renderTarget, viewport, window,
                mask | PAINT_WINDOW_TRANSFORMED, deviceRegion & KWin::Region(outputClip), data); })) return paintResult(false);
            return paintResult(true);
        }
        if (!painted([&] { return KWin::effects->paintWindow(
            renderTarget, viewport, window, mask, deviceRegion, data); })) return paintResult(false);
        return paintResult(true);
    }

    // A card held in its own Stack is lifted out of it; the place it would
    // take is a filled box at the Stack's seam, just below the held card. Let
    // go, it fades under the card then in front.
    if (window == m_cardStage->selectedWindow() && m_paintingOutput == tablet && m_destinationShader) {
        const auto outline = m_cardStage->stackOutline();
        if (!outline.rect.isEmpty() && outline.opacity > 0.0) {
            paintCardSurface(m_destinationShader.get(), renderTarget, viewport,
                deviceRegion & KWin::Region(viewport.mapToDeviceCoordinatesAligned(tablet->geometry())),
                QRectF(outline.rect), 0.6, float(0.2 * outline.opacity), 0.0f,
                SurfaceTone::current().lineInk());
        }
    }
    KWin::Rect logicalRegion = window->expandedGeometry().toRect();
    KWin::Rect target = m_cardStage->previewTargetForWindow(tablet, window);
    double launcherGuestRotation = 0.0;
    if (m_cardStage->launcherGuestActive()) {
        // previewTargetForWindow already blends the guest's incoming shoulders.
        const double offset = m_cardStage->launcherGuestOffset();
        const double progress =
            m_cardStage->launcherGuestTransitionProgress();
        const bool incoming = (offset < 0.0 && slot == 1)
            || (offset > 0.0 && slot == -1);
        if (incoming) {
            const KWin::Rect center = cardTargetForSlot(tablet, 0);
            const auto blend = [progress](int from, int to) {
                return qRound(from + (to - from) * progress);
            };
            target = KWin::Rect(blend(target.x(), center.x()),
                                blend(target.y(), center.y()),
                                blend(target.width(), center.width()),
                                blend(target.height(), center.height()));

            // Anticipate the handoff without changing either endpoint: the
            // incoming shoulder rises and leans during the user's drag, then
            // settles flat as it falls into the canonical center rectangle.
            constexpr double PreviewLimit = 0.18;
            const double settleSpan = 1.0 - PreviewLimit;
            const double liftBlend = progress <= PreviewLimit
                ? progress / PreviewLimit
                : (1.0 - progress) / settleSpan;
            target.translate(0, qRound(-16.0 * liftBlend));
            launcherGuestRotation = (slot < 0 ? -0.8 : 0.8)
                * liftBlend;
        }
    }
    CardStackPose paintPose = m_cardStage->stackPoseForWindow(window, target.width());
    if (grabbedWindow) {
        target = m_cardStage->cardGrabTarget();
        const auto offset = m_cardStage->cardGrabOffset();
        target.translate(qRound(offset.x()), qRound(offset.y()));
    } else {
        target.translate(qRound(paintPose.x), qRound(paintPose.y));
    }
    const double poseOpacity = m_cardStage->applyPoseTransition(window, target, paintPose);
    data.multiplyOpacity(poseOpacity * m_cardStage->liftOpacity(window));
    if (m_cardStage->launcherGuestActive() && m_paintingOutput == tablet) {
        const double opacity = guestNeighborOpacity();
        data.multiplyOpacity(opacity);
        const int direction = target.center().x() < tablet->geometry().center().x() ? -1 : 1;
        target.translate(qRound(direction * (1.0 - opacity) * target.width() * 0.25), 0);
    }
    paintPose.rotation += launcherGuestRotation;
    if (!paintPose.visible) {
        return paintResult(true);
    }
    KWin::Rect visualTarget = target;
    // Every card is drawn whole at its window's own shape, which is seldom
    // exactly its slot's: a window standing inside the Active card's gutter
    // is a little wider than the work area its slot is cut from, and a pane's
    // card can be far from it. The shape is the window's frame, not what it
    // paints around it, so a window drawing margins of its own outside its
    // frame is fitted, cut and backed at the frame.
    const QSizeF frameSize = QRectF(window->frameGeometry()).size();
    if (!grabbedWindow && !frameSize.isEmpty()
        && visualTarget.width() > 0 && visualTarget.height() > 0) {
        const QSizeF fitted = frameSize
            .scaled(QSizeF(visualTarget.width(), visualTarget.height()), Qt::KeepAspectRatio);
        visualTarget = KWin::Rect(
            qRound(visualTarget.x() + (visualTarget.width() - fitted.width()) / 2.0),
            qRound(visualTarget.y() + (visualTarget.height() - fitted.height()) / 2.0),
            qRound(fitted.width()), qRound(fitted.height()));
    }
    const bool rotatedFanCard = !qFuzzyIsNull(paintPose.rotation);
    m_cardLabelTargets.insert(window, QRectF(target));
    KWin::Effect::setPositionTransformations(
        data, logicalRegion, window, visualTarget,
        Qt::KeepAspectRatio);
    // The card is the window's frame exactly as KWin draws it this frame,
    // which it fits to whole points and may leave a fraction short of the
    // slot. The cut, its corners and the backing all follow that edge, so
    // neither the backing nor anything the window paints past its frame
    // shows beside it.
    const QRectF face = QRectF(logicalRegion.x(), logicalRegion.y(),
        window->width() * data.xScale(), window->height() * data.yScale())
        .intersected(QRectF(visualTarget));
    {
        // The window's edge fades over its last physical pixel; the backing
        // stops a pixel inside it, so it never rims the card.
        const double inset = 1.0 / viewport.scale();
        const QRectF backing = face.adjusted(inset, inset, -inset, -inset);
        const KWin::Region outputFence(
            viewport.mapToDeviceCoordinatesAligned(m_paintingOutput->geometry()));
        paintCardSurface(m_destinationShader.get(), renderTarget, viewport,
            deviceRegion & outputFence, backing, paintPose.rotation,
            float(data.opacity()), 0.0f, SurfaceTone::current().cardBacking(), 1.0F);
    }
    // KWin hands back where it will draw the window's frame, fitted inside
    // the card this frame; a note placed on the window stands on exactly
    // that, never on the slot the card is heading for.
    m_cardFaceRects.insert(window, QRectF(logicalRegion));
    if (rotatedFanCard) {
        // Rotate each visible aperture around its own lower-right corner. The
        // canonical slot still supplies the accepted fan offset and envelope.
        const double originX = (face.right() - logicalRegion.x()) / data.xScale();
        const double originY = (face.bottom() - logicalRegion.y()) / data.yScale();
        data.setRotationAngle(paintPose.rotation);
        data.setRotationOrigin(QVector3D(originX, originY, 0.0));
    }

    // deviceRegion is KWin's actual renderer clip.
    const KWin::Rect deviceAperture =
        viewport.mapToDeviceCoordinatesAligned(KWin::RectF(face));
    // QRegion remains only the hard output fence. Each visible preview uses
    // the shared offscreen aperture for one physical pixel of fractional edge
    // coverage; this is independent of client alpha and therefore treats a
    // solid Spotify surface exactly like a translucent decoration.
    // A tilted aperture must not be cut by an unrotated bottom boundary.
    // Straight and tilted previews share the same antialiased aperture.
    // Keep the hard-rounded path solely as the shader-unavailable fallback.
    // Active has already returned above and remains on the system paint path.
    const bool useFanAperture = m_fanApertureShader
        && data.xScale() > 0 && data.yScale() > 0
        && !deviceAperture.isEmpty();
    const KWin::Region outputFence(viewport.mapToDeviceCoordinatesAligned(
        m_paintingOutput->geometry()));
    const double apertureRadius = cardCornerRadius() * viewport.scale();
    const KWin::Region cardClip = (rotatedFanCard
        ? deviceRegion
        : deviceRegion & (useFanAperture ? KWin::Region(deviceAperture)
            : roundedClip(deviceAperture, apertureRadius)))
        & outputFence;
    m_fanApertureWindow = useFanAperture ? window : nullptr;
    m_fanPaintSize = useFanAperture
        ? QSizeF(data.xScale(), data.yScale()) : QSizeF();
    m_fanApertureOrigin = useFanAperture
        ? QPointF((face.x() - logicalRegion.x()) * viewport.scale(),
                  (face.y() - logicalRegion.y()) * viewport.scale())
        : QPointF();
    m_fanApertureSize = useFanAperture
        ? face.size() * viewport.scale() : QSizeF();
    m_fanApertureRadius = useFanAperture
        ? static_cast<float>(apertureRadius) : 0.0F;

    const bool drawn = painted([&] { return KWin::effects->paintWindow(
        renderTarget, viewport, window, mask | PAINT_WINDOW_TRANSFORMED,
        cardClip, data); });

    m_fanApertureWindow = nullptr;
    m_fanPaintSize = {};
    m_fanApertureOrigin = {};
    m_fanApertureSize = {};
    m_fanApertureRadius = 0.0F;
    if (!drawn) return paintResult(false);

    // A dialog waiting with this application shows on its card.
    return paintResult(paintDialogsOn(renderTarget, viewport, window, drawnDialogsOf(window), cardClip, data));
}

} // namespace Kadunce
