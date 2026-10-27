#include "screenfit.h"

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QGuiApplication>
#include <QRegion>
#include <QScreen>
#include <QTimer>

void ScreenFit::install()
{
    static ScreenFit *filter = nullptr;
    if (filter) return;
    // headless runs (soak, self test) have a nominal screen: leave their windows alone
    if (QGuiApplication::platformName() == "offscreen") return;
    filter = new ScreenFit(qApp);
    qApp->installEventFilter(filter);
}

void ScreenFit::fit(QWidget *w)
{
    if (!w || !w->isWindow() || !w->isVisible()) return;
    const Qt::WindowType type = w->windowType();
    if (type == Qt::Popup || type == Qt::ToolTip || type == Qt::SplashScreen) return;
    if (w->windowFlags() & Qt::FramelessWindowHint) return;
    if (w->isMaximized() || w->isFullScreen() || w->isMinimized()) return;

    const QRect inner = w->geometry();
    /* Before the native window is decorated the frame equals the client area; the
       deferred second pass in eventFilter sees the real frame. */
    const QRect frame = w->frameGeometry();

    // already wholly on usable screen area (on one screen or spanning several)
    QRegion usable;
    for (const QScreen *s : QGuiApplication::screens()) usable += s->availableGeometry();
    if ((QRegion(frame) - usable).isEmpty()) return;

    QScreen *scr = QGuiApplication::screenAt(frame.center());
    if (!scr) scr = w->screen();
    if (!scr) scr = QGuiApplication::primaryScreen();
    if (!scr) return;
    const QRect avail = scr->availableGeometry();

    // shrink the client area so the frame fits, but never below the minimum size
    const int frameW = frame.width() - inner.width();
    const int frameH = frame.height() - inner.height();
    QSize size(qMin(inner.width(), avail.width() - frameW),
               qMin(inner.height(), avail.height() - frameH));
    size = size.expandedTo(w->minimumSize());
    if (size != inner.size()) w->resize(size);

    /* Move the frame inside, right/bottom first and then left/top, so a window still
       larger than the screen (minimum size) keeps its title bar reachable. A top level
       move() positions the frame. */
    QRect f(frame.topLeft(), QSize(size.width() + frameW, size.height() + frameH));
    QPoint p = f.topLeft();
    if (f.right() > avail.right()) p.rx() -= f.right() - avail.right();
    if (f.bottom() > avail.bottom()) p.ry() -= f.bottom() - avail.bottom();
    if (p.x() < avail.left()) p.setX(avail.left());
    if (p.y() < avail.top()) p.setY(avail.top());
    if (p != w->pos()) w->move(p);
}

bool ScreenFit::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Show && obj->isWidgetType()) {
        QWidget *w = static_cast<QWidget *>(obj);
        if (w->isWindow()) {
            /* A dialog not yet placed centres itself on its parent in
               QDialog::showEvent, which runs AFTER this filter and only while WA_Moved is
               clear. Fitting it now (still at its default spot, under the menu bar) would
               move() it, set WA_Moved and cancel the centring: every dialog opened in the
               top-left corner. Leave it to the deferred pass, which sees it centred. */
            const bool unplacedDialog =
                qobject_cast<QDialog *>(w) && !w->testAttribute(Qt::WA_Moved);
            if (!unplacedDialog) fit(w);
            /* Again once the show has settled: the main window restores its geometry
               inside its own showEvent, after this filter has run. */
            QTimer::singleShot(0, w, [w] { fit(w); });     // w as context: never dangling
        }
    }
    return QObject::eventFilter(obj, event);
}
