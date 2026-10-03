// Windows that, while asked to, redraw as fast as the compositor lets them,
// and count it: a Wayland client draws its next frame only when the compositor
// says the last one was shown, so the count is how live it keeps the window.
#include <QDBusConnection>
#include <QGuiApplication>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRasterWindow>

class Surface : public QRasterWindow
{
public:
    Surface(const QString &title, const QColor &color)
        : m_color(color)
    {
        setTitle(title);
    }

    qint64 frames = 0;
    // Redrawing without pause loads a machine running other private
    // compositors, so it runs only while a session measures it.
    bool animating = false;

    void setAnimating(bool on)
    {
        animating = on;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        ++frames;
        QPainter painter(this);
        painter.fillRect(QRect(QPoint(), size()), m_color);
        // A mark that moves every frame, kept in the bottom strip so the
        // window's colour still fills its middle.
        const int x = int((frames * 12) % qMax(1, width() - 40));
        painter.fillRect(QRect(x, height() - 40, 40, 40), m_color.darker(160));
        if (animating) {
            update();
        }
    }

private:
    QColor m_color;
};

class Client : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "studio.warbler.TableClient")

public Q_SLOTS:
    void open(const QString &title, const QString &hex, int width, int height)
    {
        auto *surface = new Surface(title, QColor(QLatin1Char('#') + hex));
        surface->resize(width, height);
        surface->show();
        m_surfaces.insert(title, surface);
    }

    void animate(bool on)
    {
        for (Surface *surface : std::as_const(m_surfaces)) {
            surface->setAnimating(on);
        }
    }

    QString counts() const
    {
        QJsonObject counts;
        for (auto it = m_surfaces.cbegin(); it != m_surfaces.cend(); ++it) {
            counts.insert(it.key(), it.value()->frames);
        }
        return QString::fromUtf8(QJsonDocument(counts).toJson(QJsonDocument::Compact));
    }

private:
    QHash<QString, Surface *> m_surfaces;
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    Client client;
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Client"), &client, QDBusConnection::ExportAllSlots);
    QDBusConnection::sessionBus().registerService(QStringLiteral("studio.warbler.TableClient"));
    return app.exec();
}

#include "table-client.moc"
