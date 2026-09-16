#include <QApplication>
#include <QWidget>
#include <QTouchEvent>
#include <QMouseEvent>
#include <QDBusConnection>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWindow>
#include <QPushButton>
#include <QtGui/qguiapplication_platform.h>
#include <xcb/xcb.h>
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
 void largeCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Large admission probe"); w->setMinimumSize(1000,700); w->resize(1000,700); w->show(); }
 void overflowCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Large admission probe"); w->setMinimumSize(1250,700); w->resize(1250,700); w->show(); }
 void immediateCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Immediate ownership probe"); w->setMinimumSize(1500,900); w->resize(1500,900); w->show(); }
 void crossCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Cross ownership probe"); w->resize(400,300); w->show(); }
 void oversizedCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Oversized ownership probe"); w->setMinimumSize(1500,900); w->resize(1500,900); w->show(); }
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
private: bool moveArmed = false, resizeArmed = false; int downs=0,ups=0,cancels=0,presses=0,releases=0;
 int targetPresses=0,targetReleases=0,targetClicks=0;
};
int main(int argc,char**argv) { QApplication a(argc,argv); Client w; w.showMaximized(); QDBusConnection::sessionBus().registerService("studio.warbler.UnloadClient"); QDBusConnection::sessionBus().registerObject("/Client",&w,QDBusConnection::ExportAllSlots); return a.exec(); }
#include "client.moc"
