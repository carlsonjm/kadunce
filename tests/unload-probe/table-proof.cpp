// What Table needs from KWin, as a test-only effect for a private compositor:
// another desktop drawn at full size on every display without switching to it,
// a switch the slide effect stays out of, and desktops created, renamed,
// filled and removed through KWin's own objects. It is not Kadunce, and it is
// never installed.
#include <core/output.h>
#include <effect/effect.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <virtualdesktops.h>
#include <window.h>

#include <QDBusConnection>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>

class TableProof final : public KWin::Effect
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "studio.warbler.TableProof")

public:
    TableProof()
    {
        using KWin::effects;
        connect(effects, &KWin::EffectsHandler::windowAdded, this, &TableProof::watch);
        connect(effects, &KWin::EffectsHandler::windowClosed, this, [this](KWin::EffectWindow *w) {
            m_refs.remove(w);
        });
        connect(effects, &KWin::EffectsHandler::windowDeleted, this, [this](KWin::EffectWindow *w) {
            m_refs.remove(w);
        });
        connect(effects, &KWin::EffectsHandler::desktopChanged, this, [this]() {
            ++m_desktopChanges;
        });
        for (KWin::EffectWindow *w : effects->stackingOrder()) {
            watch(w);
        }
        // Watches what KDE's own switching does, on a timer because this
        // effect's paint hooks run only while it previews.
        m_watch.setInterval(4);
        connect(&m_watch, &QTimer::timeout, this, [this]() {
            if (KWin::effects->isEffectActive(QStringLiteral("slide"))) {
                m_slideSeen = true;
            }
        });
        m_watch.start();
        QDBusConnection::sessionBus().registerObject(QStringLiteral("/TableProof"), this, QDBusConnection::ExportAllSlots);
    }

    ~TableProof() override
    {
        endPreview();
        QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/TableProof"));
    }

    bool isActive() const override
    {
        return m_preview != nullptr;
    }

    void prePaintWindow(KWin::RenderView *view, KWin::EffectWindow *w, KWin::WindowPrePaintData &data) override
    {
        // A window left out must not hide what is under it from the scene.
        if (!drawn(w)) {
            data.setTranslucent();
        }
        KWin::effects->prePaintWindow(view, w, data);
    }

    void paintWindow(const KWin::RenderTarget &target, const KWin::RenderViewport &viewport, KWin::EffectWindow *w,
                     int mask, const KWin::Region &deviceRegion, KWin::WindowPaintData &data) override
    {
        if (drawn(w)) {
            KWin::effects->paintWindow(target, viewport, w, mask, deviceRegion, data);
        }
    }

public Q_SLOTS:
    QString create(uint position, const QString &name)
    {
        KWin::VirtualDesktop *desktop = KWin::VirtualDesktopManager::self()->createVirtualDesktop(position, name);
        return desktop ? desktop->id() : QString();
    }

    bool rename(const QString &id, const QString &name)
    {
        KWin::VirtualDesktop *desktop = KWin::VirtualDesktopManager::self()->desktopForId(id);
        if (!desktop) {
            return false;
        }
        desktop->setName(name);
        return true;
    }

    bool remove(const QString &id)
    {
        if (!KWin::VirtualDesktopManager::self()->desktopForId(id)) {
            return false;
        }
        KWin::VirtualDesktopManager::self()->removeVirtualDesktop(id);
        return true;
    }

    bool moveWindow(const QString &caption, const QString &desktopId)
    {
        KWin::EffectWindow *w = find(caption);
        KWin::VirtualDesktop *desktop = KWin::VirtualDesktopManager::self()->desktopForId(desktopId);
        if (!w || !desktop) {
            return false;
        }
        KWin::effects->windowToDesktops(w, {desktop});
        return true;
    }

    bool place(const QString &caption, int x, int y, int width, int height)
    {
        KWin::EffectWindow *w = find(caption);
        if (!w) {
            return false;
        }
        w->window()->moveResize(KWin::RectF(x, y, width, height));
        return true;
    }

    // Draws the named desktop on every display in place of the current one,
    // without switching; an empty id ends the preview and changes nothing.
    bool preview(const QString &desktopId)
    {
        if (desktopId.isEmpty()) {
            endPreview();
            return true;
        }
        KWin::VirtualDesktop *desktop = KWin::VirtualDesktopManager::self()->desktopForId(desktopId);
        if (!desktop) {
            return false;
        }
        m_preview = desktop;
        KWin::effects->setActiveFullScreenEffect(this);
        sync();
        KWin::effects->addRepaintFull();
        return true;
    }

    // Makes the previewed desktop current while this effect is the one KWin's
    // switching effects defer to, then lets the preview go.
    bool commit()
    {
        if (!m_preview) {
            return false;
        }
        KWin::effects->setCurrentDesktop(m_preview);
        endPreview();
        return true;
    }

    bool holdsScreen() const
    {
        return KWin::effects->activeFullScreenEffect() == this;
    }

    bool slideActive() const
    {
        return KWin::effects->isEffectActive(QStringLiteral("slide"));
    }

    void resetWatch()
    {
        m_slideSeen = false;
        m_desktopChanges = 0;
    }

    QString watched() const
    {
        return QString::fromUtf8(QJsonDocument(QJsonObject{
                                                   {QStringLiteral("slideSeen"), m_slideSeen},
                                                   {QStringLiteral("desktopChanges"), m_desktopChanges},
                                               })
                                     .toJson(QJsonDocument::Compact));
    }

    QString outputs() const
    {
        QJsonArray list;
        for (KWin::LogicalOutput *output : KWin::effects->screens()) {
            const auto geometry = output->geometry();
            list.append(QJsonObject{{QStringLiteral("x"), geometry.x()},
                                    {QStringLiteral("y"), geometry.y()},
                                    {QStringLiteral("width"), geometry.width()},
                                    {QStringLiteral("height"), geometry.height()}});
        }
        return QString::fromUtf8(QJsonDocument(list).toJson(QJsonDocument::Compact));
    }

    QString uuidOf(const QString &caption) const
    {
        KWin::EffectWindow *w = find(caption);
        return w ? w->internalId().toString(QUuid::WithBraces) : QString();
    }

private:
    void watch(KWin::EffectWindow *w)
    {
        connect(w, &KWin::EffectWindow::windowDesktopsChanged, this, &TableProof::sync);
        sync();
    }

    // Holds each previewed window visible, and only those.
    void sync()
    {
        if (!m_preview) {
            return;
        }
        for (KWin::EffectWindow *w : KWin::effects->stackingOrder()) {
            const bool wanted = w->isOnDesktop(m_preview) && !w->isOnCurrentDesktop() && !w->isDeleted();
            if (wanted && !m_refs.contains(w)) {
                m_refs.insert(w, KWin::EffectWindowVisibleRef(w, KWin::EffectWindow::PAINT_DISABLED_BY_DESKTOP));
            } else if (!wanted) {
                m_refs.remove(w);
            }
        }
    }

    void endPreview()
    {
        if (!m_preview) {
            return;
        }
        m_preview = nullptr;
        m_refs.clear();
        if (KWin::effects->activeFullScreenEffect() == this) {
            KWin::effects->setActiveFullScreenEffect(nullptr);
        }
        KWin::effects->addRepaintFull();
    }

    bool drawn(KWin::EffectWindow *w) const
    {
        if (!m_preview) {
            return true;
        }
        return w->isOnDesktop(m_preview) || w->isDock() || w->isDesktop() || w->isOnAllDesktops();
    }

    KWin::EffectWindow *find(const QString &caption) const
    {
        for (KWin::EffectWindow *w : KWin::effects->stackingOrder()) {
            if (w->caption() == caption && !w->isDeleted()) {
                return w;
            }
        }
        return nullptr;
    }

    QPointer<KWin::VirtualDesktop> m_preview;
    QHash<KWin::EffectWindow *, KWin::EffectWindowVisibleRef> m_refs;
    bool m_slideSeen = false;
    int m_desktopChanges = 0;
    QTimer m_watch;
};

KWIN_EFFECT_FACTORY_SUPPORTED(TableProof, "table-proof.json", return true;)

#include "table-proof.moc"
