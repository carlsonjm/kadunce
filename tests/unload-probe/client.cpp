#include <QApplication>
#include <QWidget>
#include <QTouchEvent>
#include <QMouseEvent>
#include <QDBusConnection>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWindow>
#include <QPushButton>
#include <QDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QPalette>
#include <QtGui/qguiapplication_platform.h>
#include <xcb/xcb.h>
#include <QScreen>
#include <QThread>
#include <QTimer>
#include <LayerShellQt/Window>
// Accepts text but never says where its cursor is, the way a terminal or a
// canvas-drawn editor can: the rectangle it reports is empty.
class BlindText : public QWidget {
public:
 QVariant inputMethodQuery(Qt::InputMethodQuery query) const override {
  if (query == Qt::ImCursorRectangle || query == Qt::ImAnchorRectangle) return QRect();
  if (query == Qt::ImEnabled) return true;
  return QWidget::inputMethodQuery(query);
 }
};
// A stand-in for the Shuffle Bottom Surface on this private bus, speaking the
// interface the Keyboard asks it by: a dock band along the tablet's bottom edge
// that reserves its height, steps aside when the Keyboard asks for the region,
// says it has stopped reserving once that has gone out, and takes its room
// back when the Keyboard lets the region go. The Keyboard holds its keys below
// the screen's edge until the band has stopped reserving.
class BandStandIn : public QObject {
 Q_OBJECT
 Q_CLASSINFO("D-Bus Interface", "studio.warbler.BottomSurface")
public:
 explicit BandStandIn(int band) : m_band(band) {
  m_dock = new QWidget; m_dock->setObjectName("band");
  QPalette p = m_dock->palette(); p.setColor(QPalette::Window, QColor(0x10, 0x10, 0x10));
  m_dock->setPalette(p); m_dock->setAutoFillBackground(true);
  m_dock->winId();
  for (QScreen *screen : qGuiApp->screens()) if (screen->name() == "Virtual-0") m_dock->windowHandle()->setScreen(screen);
  if (auto *layer = LayerShellQt::Window::get(m_dock->windowHandle())) {
   layer->setScope(QStringLiteral("dock"));
   layer->setLayer(LayerShellQt::Window::LayerTop);
   layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom
    | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
   layer->setExclusiveZone(band);
   layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
   layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
  }
  m_dock->resize(1280, band); m_dock->show();
 }
 // Only scriptable slots are exported to the bus.
public Q_SLOTS:
 // What the Keyboard reads: the band's height, where the dock sits on it, and
 // whether the band still holds its reservation.
 Q_SCRIPTABLE QString dockExtent(const QString &) const {
  return QString::fromUtf8(QJsonDocument(QJsonObject{
   {"schema", "studio.warbler.shuffle.dock-extent"}, {"version", 1}, {"output", "Virtual-0"},
   {"presenting", true}, {"reserving", m_reserving},
   {"band", QJsonObject{{"height", m_band}}},
   {"dock", QJsonObject{{"left", 440}, {"right", 840}, {"center", 640}}},
   {"available", QJsonObject{{"left", 440}, {"right", 440}}}}).toJson(QJsonDocument::Compact));
 }
 // The dock leaves first and the reservation goes after it, as the surface's
 // own presentation does.
 Q_SCRIPTABLE bool yieldRegion() {
  ++yields; m_yielded = true;
  QTimer::singleShot(120, this, [this] {
   if (!m_yielded) return;
   setZone(0);
   QTimer::singleShot(40, this, [this] { if (m_yielded) { m_reserving = false; Q_EMIT dockExtentChanged("Virtual-0"); } });
  });
  return true;
 }
 Q_SCRIPTABLE bool releaseRegion() {
  ++releases; m_yielded = false;
  setZone(m_band); m_reserving = true; Q_EMIT dockExtentChanged("Virtual-0");
  return true;
 }
public:
 QJsonObject state() const { return {{"reserving", m_reserving}, {"yielded", m_yielded}, {"yields", yields}, {"releases", releases}}; }
 int yields = 0, releases = 0;
Q_SIGNALS:
 Q_SCRIPTABLE void dockExtentChanged(const QString &outputName);
private:
 void setZone(int zone) {
  if (auto *layer = LayerShellQt::Window::get(m_dock->windowHandle())) { layer->setExclusiveZone(zone); m_dock->update(); }
 }
 QWidget *m_dock = nullptr;
 int m_band;
 bool m_reserving = true, m_yielded = false;
};
// Takes as long to draw as a browser laying a page out again, so each size the
// compositor asks for reaches the screen that much later.
class SlowPaint : public QWidget {
public:
 int delay = 0;
 void paintEvent(QPaintEvent *e) override { QThread::msleep(delay); QWidget::paintEvent(e); }
};
class Client : public QWidget {
 Q_OBJECT
public:
 Client() { setAttribute(Qt::WA_AcceptTouchEvents); if (qEnvironmentVariableIsSet("KADUNCE_TEST_FRAMELESS")) setWindowFlag(Qt::FramelessWindowHint); resize(900,650); }
 bool event(QEvent *e) override {
  if (resizeArmed && (e->type()==QEvent::TouchBegin || e->type()==QEvent::MouseButtonPress)) {
   resizeArmed = false;
   if (windowHandle()) windowHandle()->startSystemResize(Qt::BottomEdge);
  }
  if (moveArmed && (e->type()==QEvent::TouchBegin || e->type()==QEvent::MouseButtonPress)) {
   moveArmed = false;
   if (e->type()==QEvent::TouchBegin && qGuiApp->platformName()=="xcb") x11Request(8,1);
   else if (windowHandle()) windowHandle()->startSystemMove();
  }
  if(e->type()==QEvent::TouchBegin) { ++downs; e->accept(); return true; }
  if(e->type()==QEvent::TouchEnd) { ++ups; e->accept(); return true; }
  if(e->type()==QEvent::TouchCancel) { ++cancels; e->accept(); return true; }
  if(e->type()==QEvent::TouchUpdate) { e->accept(); return true; }
  if(e->type()==QEvent::MouseButtonPress || e->type()==QEvent::MouseButtonDblClick) ++presses;
  if(e->type()==QEvent::MouseButtonRelease) ++releases;
  return QWidget::event(e);
 }
public Q_SLOTS:
 void closeWindow() { QApplication::closeAllWindows(); }
 void armMove() { moveArmed = true; }
 void armResize() { resizeArmed = true; }
 void minimumSizeHint(int width, int height) { setMinimumSize(width, height); }
 // Private test client only: exercise rejected EWMH requests through the real server.
 void x11Request(int direction, int button) {
  auto *native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
  if (!native) return;
  auto *c = native->connection();
  const char name[] = "_NET_WM_MOVERESIZE";
  auto *atom = xcb_intern_atom_reply(c, xcb_intern_atom(c, false, sizeof(name)-1, name), nullptr);
  if (!atom) return;
  xcb_client_message_event_t e{}; e.response_type=XCB_CLIENT_MESSAGE; e.format=32;
  e.window=winId(); e.type=atom->atom; free(atom);
  const auto pos = QCursor::pos();
  e.data.data32[0]=pos.x(); e.data.data32[1]=pos.y(); e.data.data32[2]=direction; e.data.data32[3]=button; e.data.data32[4]=1;
  xcb_send_event(c, false, xcb_setup_roots_iterator(xcb_get_setup(c)).data->root,
   XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, reinterpret_cast<const char *>(&e));
  xcb_flush(c);
 }
 void companion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->resize(500,400); w->show(); }
 // A named window with a minimum size, so a scene can decide how many fit a
 // layout without depending on the layout's own pane count.
 void titledCompanion(const QString &title, int width, int height) { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle(title); w->setMinimumSize(width, height); w->resize(width, height); w->show(); }
 void largeCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Large admission probe"); w->setMinimumSize(1000,700); w->resize(1000,700); w->show(); }
 // Fits the 0.62 pane of a two-pane landscape layout on a 1280x800 output and
 // nothing smaller, so it can only be admitted by growing the layout and can
 // only land in the larger pane.
 void paneCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Pane admission probe"); w->setMinimumSize(700,600); w->resize(700,600); w->show(); }
 void widerCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Large admission probe"); w->setMinimumSize(1250,700); w->resize(1250,700); w->show(); }
 void immediateCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Immediate ownership probe"); w->setMinimumSize(1500,900); w->resize(1500,900); w->show(); }
 void colouredCompanion(const QString &title, const QString &hex, int width, int height) { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle(title); tint(w, QColor(QLatin1Char('#') + hex)); w->resize(width, height); w->show(); }
 void crossCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Cross ownership probe"); tint(w, QColor(0x2e, 0x8b, 0x57)); w->resize(400,300); w->show(); }
 // A client whose own minimum grows while its layout is not live, the way one
 // does on a font or scale change. The rect its session stored is then a size
 // the client will not take, and the compositor clamps the placement to what
 // it will. Zero puts the minimum back.
 void companionMinimumSize(const QString &title, int width, int height) {
  for (auto *w : QApplication::topLevelWidgets())
   if (w->isWindow() && w->windowTitle().contains(title)) w->setMinimumSize(width, height);
 }
 // A window whose only text field sits at its bottom edge, focused, so the
 // text cursor it reports is where a keyboard raised from below arrives first.
 void textCompanion() {
  auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Keyboard reveal probe");
  // One colour nothing else in the session uses, so a photograph can tell
  // this window's pixels from everything around it.
  QPalette colour = w->palette(); colour.setColor(QPalette::Window, QColor(0x2a, 0x6f, 0x97));
  w->setPalette(colour); w->setAutoFillBackground(true);
  auto *layout = new QVBoxLayout(w); layout->addStretch(); auto *field = new QLineEdit; field->setObjectName("revealField");
  layout->addWidget(field); w->resize(560,420); w->show(); w->activateWindow(); field->setFocus();
 }
 // The same, drawn slowly.
 void slowTextCompanion(int delay) {
  auto *w = new SlowPaint; w->delay = delay; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Keyboard slow probe");
  QPalette colour = w->palette(); colour.setColor(QPalette::Window, QColor(0x2a, 0x6f, 0x97));
  w->setPalette(colour); w->setAutoFillBackground(true);
  auto *layout = new QVBoxLayout(w); layout->addStretch(); auto *field = new QLineEdit;
  layout->addWidget(field); w->resize(560,420); w->show(); w->activateWindow(); field->setFocus();
 }
 // The same field halfway down, where a card's middle lands: a search box
 // centred on a page, or the line an editor's cursor is on.
 void centreTextCompanion() {
  auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Keyboard centre probe");
  auto *layout = new QVBoxLayout(w); auto *field = new QLineEdit; field->setObjectName("centreField");
  layout->addStretch(); layout->addWidget(field); layout->addStretch();
  w->resize(560,420); w->show(); w->activateWindow(); field->setFocus();
 }
 // The same field at the top edge, where a raised keyboard never reaches.
 void topTextCompanion() {
  auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Keyboard top probe");
  auto *layout = new QVBoxLayout(w); auto *field = new QLineEdit; layout->addWidget(field); layout->addStretch();
  w->resize(560,420); w->show(); w->activateWindow(); field->setFocus();
 }
 // Two fields, one at the top edge and one at the bottom, and the application
 // focuses the top one itself, as a browser focuses its address bar.
 void pairTextCompanion() {
  auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Keyboard pair probe");
  auto *layout = new QVBoxLayout(w); auto *top = new QLineEdit; top->setObjectName("topField");
  auto *bottom = new QLineEdit; bottom->setObjectName("bottomField");
  layout->addWidget(top); layout->addStretch(); layout->addWidget(bottom);
  w->resize(560,420); w->show(); w->activateWindow(); top->setFocus();
 }
 // Where a named field's middle is in its window, as "x y" in the window's
 // own coordinates; a scene adds where the window stands.
 QString fieldCentre(const QString &title, const QString &name) {
  for (auto *w : QApplication::topLevelWidgets())
   if (w->isWindow() && w->windowTitle().contains(title))
    if (auto *field = w->findChild<QLineEdit *>(name)) {
     const QPoint c = field->mapTo(w, field->rect().center());
     return QStringLiteral("%1 %2").arg(c.x()).arg(c.y());
    }
  return {};
 }
 // Which field of this client has the focus, by name: what a touch landed in.
 QString focusedField() {
  QWidget *w = QApplication::focusWidget();
  return w ? w->window()->windowTitle() + QLatin1Char('/') + w->objectName() : QString();
 }
 // The Bottom Surface stand-in, registered as the surface registers itself:
 // the object first, then the name the Keyboard watches for.
 void bottomSurface(int band) {
  if (band_ || qGuiApp->platformName() != "wayland") return;
  band_ = new BandStandIn(band);
  QDBusConnection::sessionBus().registerObject("/BottomSurface", band_,
   QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals);
  QDBusConnection::sessionBus().registerService("studio.warbler.BottomSurface");
  // Said again once the name has had time to reach everyone watching for it.
  Q_EMIT band_->dockExtentChanged("Virtual-0");
  QTimer::singleShot(500, band_, [this] { Q_EMIT band_->dockExtentChanged("Virtual-0"); });
 }
 QString bandState() { return band_ ? QString::fromUtf8(QJsonDocument(band_->state()).toJson(QJsonDocument::Compact)) : QString("{}"); }
 // A window that asks for text input and reports no cursor at all.
 void blindTextCompanion() {
  auto *w = new BlindText; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Keyboard blind probe");
  w->setAttribute(Qt::WA_InputMethodEnabled); w->setFocusPolicy(Qt::StrongFocus); w->resize(560,420); w->show(); w->activateWindow(); w->setFocus();
 }
 // The window takes a size of its own accord, as a client answering a
 // configure the compositor has since superseded does.
 void resizeCompanion(const QString &title, int width, int height) {
  for (auto *w : QApplication::topLevelWidgets())
   if (w->isWindow() && w->windowTitle().contains(title)) w->resize(width, height);
 }
 void focusText(const QString &title) {
  for (auto *w : QApplication::topLevelWidgets())
   if (w->isWindow() && w->windowTitle().contains(title)) {
    // Focus leaves and returns so the client reports its cursor afresh; a
    // client reports it on a focus or cursor change, not on a resize.
    w->activateWindow(); QWidget *target = w->findChild<QLineEdit *>(); if (!target) target = w;
    target->clearFocus(); target->setFocus();
   }
 }
 // Typing ends: the field lets go of the text cursor and the window keeps
 // the focus, as a tap on the page around a field leaves it.
 void leaveText(const QString &title) {
  for (auto *w : QApplication::topLevelWidgets())
   if (w->isWindow() && w->windowTitle().contains(title)) {
    w->setFocusPolicy(Qt::StrongFocus); w->setFocus();
   }
 }
 // The dialogs an application opens over itself: a save dialog and a
 // confirmation, both modal to this window, and a plain dialog that is not.
 void saveDialog() { auto *d = new QFileDialog(this, "Save probe"); d->setOption(QFileDialog::DontUseNativeDialog); d->setAcceptMode(QFileDialog::AcceptSave); d->setAttribute(Qt::WA_DeleteOnClose); d->open(); }
 void confirmDialog() { auto *d = new QMessageBox(QMessageBox::Question, "Confirm probe", "Discard changes?", QMessageBox::Yes | QMessageBox::No, this); d->setAttribute(Qt::WA_DeleteOnClose); d->open(); }
 void plainDialog() { auto *d = new QDialog(this); d->setWindowTitle("Dialog probe"); d->resize(420,300); d->setAttribute(Qt::WA_DeleteOnClose); d->show(); }
 // A plain dialog in its own colour, so a photograph finds where it is drawn,
 // and wider than the smaller pane of a Bento pair.
 void tintedDialog() { auto *d = new QDialog(this); d->setWindowTitle("Tinted dialog probe"); tint(d, QColor(0xc0, 0x3a, 0x8a)); d->resize(700,300); d->setAttribute(Qt::WA_DeleteOnClose); d->show(); }
 void closeDialogs() { for (auto *d : findChildren<QDialog *>()) d->close(); }
 // A tooltip over this window: a popup that no layout ever holds.
 void tooltip() { auto *w = new QWidget(this, Qt::ToolTip); w->setAttribute(Qt::WA_DeleteOnClose); w->setObjectName("tooltip"); w->setGeometry(40,40,160,32); w->show(); }
 void closeTooltip() { for (auto *w : findChildren<QWidget *>("tooltip", Qt::FindDirectChildrenOnly)) w->close(); }
 void closeCompanion(const QString &title) {
  for (auto *w : QApplication::topLevelWidgets())
   if (w->isWindow() && w->windowTitle().contains(title)) w->close();
 }
 // Each in its own colour, so a photograph says which window is showing.
 static void tint(QWidget *w, QColor c) { QPalette p = w->palette(); p.setColor(QPalette::Window, c); w->setPalette(p); w->setAutoFillBackground(true); }
 // Plasma's desktop view on Wayland: a layer surface with the desktop scope,
 // under everything on the first output, where the tablet fixture holds cards.
 void desktopSurface() {
  if (qGuiApp->platformName() != "wayland") return;
  auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); tint(w, QColor(0x33, 0x44, 0x55));
  w->winId();
  for (QScreen *screen : qGuiApp->screens()) if (screen->name() == "Virtual-0") w->windowHandle()->setScreen(screen);
  if (auto *layer = LayerShellQt::Window::get(w->windowHandle())) {
   layer->setScope(QStringLiteral("desktop"));
   layer->setLayer(LayerShellQt::Window::LayerBackground);
   layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
    | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
   layer->setExclusiveZone(-1);
   layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
   layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
  }
  w->show();
 }
 // A dock along the tablet's bottom edge that reserves its height, and gives
 // the reservation up and takes it back as a bottom surface does for the keys.
 void dockSurface(int height) {
  if (qGuiApp->platformName() != "wayland") return;
  auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setObjectName("dock"); tint(w, QColor(0x10, 0x10, 0x10));
  w->winId();
  for (QScreen *screen : qGuiApp->screens()) if (screen->name() == "Virtual-0") w->windowHandle()->setScreen(screen);
  if (auto *layer = LayerShellQt::Window::get(w->windowHandle())) {
   layer->setScope(QStringLiteral("dock"));
   layer->setLayer(LayerShellQt::Window::LayerTop);
   layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom
    | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
   layer->setExclusiveZone(height);
   layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
   layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
  }
  w->resize(1280, height); w->show();
 }
 void dockReserve(int zone) {
  for (auto *w : QApplication::topLevelWidgets())
   if (w->objectName() == "dock")
    if (auto *layer = LayerShellQt::Window::get(w->windowHandle())) { layer->setExclusiveZone(zone); w->update(); }
 }
 void ordinaryCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Ordinary neighbor probe"); tint(w, QColor(0xc8, 0x8a, 0x1e)); w->resize(560,420); w->show(); }
 void oversizedCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Oversized ownership probe"); w->setMinimumSize(1500,900); w->resize(1500,900); w->show(); }
 // A client that changes its own frame, the way a terminal does on a font or
 // scale change. The window it names is a pane whose layout is not live, so
 // nothing is holding the frame and the request lands.
 void resizeCompanion(const QString &title, int delta) {
  for (auto *w : QApplication::topLevelWidgets())
   if (w->isWindow() && w->windowTitle().contains(title)) w->resize(w->width(), w->height() + delta);
 }
 void clickTarget() {
  auto *button = new QPushButton("Unload safety target");
  button->setAttribute(Qt::WA_DeleteOnClose);
  button->setGeometry(1400,200,500,400);
  connect(button, &QPushButton::pressed, this, [this] { ++targetPresses; });
  connect(button, &QPushButton::released, this, [this] { ++targetReleases; });
  connect(button, &QPushButton::clicked, this, [this] { ++targetClicks; });
  button->show();
 }
 QString targetState() { return QString::fromUtf8(QJsonDocument(QJsonObject{{"press",targetPresses},{"release",targetReleases},{"click",targetClicks}}).toJson(QJsonDocument::Compact)); }
 QString state() { return QString::fromUtf8(QJsonDocument(QJsonObject{{"touchDown",downs},{"touchUp",ups},{"cancel",cancels},{"press",presses},{"release",releases}}).toJson(QJsonDocument::Compact)); }
 void reset() { downs=ups=cancels=presses=releases=0; }
private: BandStandIn *band_ = nullptr;
 bool moveArmed = false, resizeArmed = false; int downs=0,ups=0,cancels=0,presses=0,releases=0;
 int targetPresses=0,targetReleases=0,targetClicks=0;
};
int main(int argc,char**argv) { QApplication a(argc,argv); Client w; w.showMaximized(); QDBusConnection::sessionBus().registerService("studio.warbler.UnloadClient"); QDBusConnection::sessionBus().registerObject("/Client",&w,QDBusConnection::ExportAllSlots); return a.exec(); }
#include "client.moc"
