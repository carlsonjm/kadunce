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
 void largeCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Large admission probe"); w->setMinimumSize(1000,700); w->resize(1000,700); w->show(); }
 // Fits the 0.62 pane of a two-pane landscape layout on a 1280x800 output and
 // nothing smaller, so it can only be admitted by growing the layout and can
 // only land in the larger pane.
 void paneCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Pane admission probe"); w->setMinimumSize(700,600); w->resize(700,600); w->show(); }
 void widerCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Large admission probe"); w->setMinimumSize(1250,700); w->resize(1250,700); w->show(); }
 void immediateCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Immediate ownership probe"); w->setMinimumSize(1500,900); w->resize(1500,900); w->show(); }
 void crossCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Cross ownership probe"); w->resize(400,300); w->show(); }
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
 // The same field at the top edge, where a raised keyboard never reaches.
 void topTextCompanion() {
  auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Keyboard top probe");
  auto *layout = new QVBoxLayout(w); auto *field = new QLineEdit; layout->addWidget(field); layout->addStretch();
  w->resize(560,420); w->show(); w->activateWindow(); field->setFocus();
 }
 // A window that asks for text input and reports no cursor at all.
 void blindTextCompanion() {
  auto *w = new BlindText; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Keyboard blind probe");
  w->setAttribute(Qt::WA_InputMethodEnabled); w->setFocusPolicy(Qt::StrongFocus); w->resize(560,420); w->show(); w->activateWindow(); w->setFocus();
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
 // The dialogs an application opens over itself: a save dialog and a
 // confirmation, both modal to this window, and a plain dialog that is not.
 void saveDialog() { auto *d = new QFileDialog(this, "Save probe"); d->setOption(QFileDialog::DontUseNativeDialog); d->setAcceptMode(QFileDialog::AcceptSave); d->setAttribute(Qt::WA_DeleteOnClose); d->open(); }
 void confirmDialog() { auto *d = new QMessageBox(QMessageBox::Question, "Confirm probe", "Discard changes?", QMessageBox::Yes | QMessageBox::No, this); d->setAttribute(Qt::WA_DeleteOnClose); d->open(); }
 void plainDialog() { auto *d = new QDialog(this); d->setWindowTitle("Dialog probe"); d->resize(420,300); d->setAttribute(Qt::WA_DeleteOnClose); d->show(); }
 void closeDialogs() { for (auto *d : findChildren<QDialog *>()) d->close(); }
 // A tooltip over this window: a popup that no layout ever holds.
 void tooltip() { auto *w = new QWidget(this, Qt::ToolTip); w->setAttribute(Qt::WA_DeleteOnClose); w->setObjectName("tooltip"); w->setGeometry(40,40,160,32); w->show(); }
 void closeTooltip() { for (auto *w : findChildren<QWidget *>("tooltip", Qt::FindDirectChildrenOnly)) w->close(); }
 void closeCompanion(const QString &title) {
  for (auto *w : QApplication::topLevelWidgets())
   if (w->isWindow() && w->windowTitle().contains(title)) w->close();
 }
 void ordinaryCompanion() { auto *w = new QWidget; w->setAttribute(Qt::WA_DeleteOnClose); w->setWindowTitle("Ordinary neighbor probe"); w->resize(560,420); w->show(); }
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
private: bool moveArmed = false, resizeArmed = false; int downs=0,ups=0,cancels=0,presses=0,releases=0;
 int targetPresses=0,targetReleases=0,targetClicks=0;
};
int main(int argc,char**argv) { QApplication a(argc,argv); Client w; w.showMaximized(); QDBusConnection::sessionBus().registerService("studio.warbler.UnloadClient"); QDBusConnection::sessionBus().registerObject("/Client",&w,QDBusConnection::ExportAllSlots); return a.exec(); }
#include "client.moc"
