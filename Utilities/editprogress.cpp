#include "Utilities/editprogress.h"
#include "Utilities/progress.h"
#include "Utilities/popup.h"
#include "Main/global.h"
#include <QCoreApplication>
#include <QLayout>
#include <QLocale>
#include <QThread>
#include <QWidget>

#ifdef Q_OS_MAC
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>
#endif

Progress *EditProgress::progress = nullptr;
int EditProgress::rowId = -1;
QWidget *EditProgress::statusBar = nullptr;
int EditProgress::depth = 0;

namespace {

/*  How long an edit may run before it is reported, and how early a projection may say
    it will run that long. The projection is what lets a 5-second write show its bar at
    a quarter of a second instead of at one. */
constexpr qint64 kShowAfterMs = 1000;
constexpr qint64 kProjectAfterMs = 250;
constexpr qint64 kPaintIntervalMs = 100;

void flushToScreen()
{
    /* See Popup::paintNow: on macOS repaint() only stages the layer contents. */
#ifdef Q_OS_MAC
    id caTransaction = (id)objc_getClass("CATransaction");
    if (caTransaction)
        ((void(*)(id, SEL))objc_msgSend)(caTransaction, sel_registerName("flush"));
#endif
}

} // namespace

void EditProgress::install(Progress *p, int id, QWidget *bar)
{
    progress = p;
    rowId = id;
    statusBar = bar;
}

EditProgress::EditProgress(const QString &what, int total)
    : what(what), total(total)
{
    ++depth;
    outermost = depth == 1 && QThread::currentThread() == QCoreApplication::instance()->thread();
    clock.start();
}

EditProgress::~EditProgress()
{
    --depth;
    if (target == Target::StatusBar && progress) {
        progress->clearProgress(rowId);
        progress->setRowToolTip(rowId, QString());
    }
    if (target == Target::Popup && G::popup) {
        G::popup->setProgressVisible(false);
        G::popup->reset();
    }
}

bool EditProgress::shouldShow(int n) const
{
    if (!outermost || target != Target::None || total <= 1) return false;
    const qint64 ms = clock.elapsed();
    if (ms >= kShowAfterMs) return true;
    if (ms < kProjectAfterMs || n <= 0) return false;
    return ms * total / n >= kShowAfterMs;
}

QString EditProgress::tip() const
{
    const QLocale loc;
    QString s = QString("%1: saving %2 of %3 images")
                    .arg(what, loc.toString(qMin(done, total)), loc.toString(total));
    if (!phaseText.isEmpty()) s = what + ": " + phaseText;
    return s;
}

void EditProgress::show()
{
    const bool barShown = statusBar && statusBar->isVisible() && progress;
    if (barShown) {
        target = Target::StatusBar;
        progress->setRowToolTip(rowId, tip());
        progress->updateProgress(rowId, done, total);
        /*  The row has just been revealed and the status bar has grown to hold it, but
            only as posted layout requests. Run them now, status bar first (it places the
            Progress widget), then the window (which gives the status bar its height). */
        if (QLayout *l = statusBar->layout()) l->activate();
        if (QWidget *w = statusBar->window(); w && w->layout()) w->layout()->activate();
        statusBar->repaint();
        flushToScreen();
        paintClock.restart();
        return;
    }
    /* No status bar to report in. A popup already up belongs to someone else. */
    if (!G::popup || G::popup->isVisible()) return;
    target = Target::Popup;
    G::popup->setProgressVisible(true);
    G::popup->setProgressMax(total);
    G::popup->setProgress(done);
    G::popup->showPopupNow(QString("Saving metadata (%1)").arg(what.toLower()), 0);
    paintClock.restart();
}

void EditProgress::paint(bool force)
{
    if (!force && paintClock.isValid() && paintClock.elapsed() < kPaintIntervalMs) return;
    paintClock.restart();
    if (target == Target::StatusBar && progress) {
        progress->setRowToolTip(rowId, tip());
        progress->updateProgress(rowId, done, total);
        progress->repaint();
        flushToScreen();
    }
    else if (target == Target::Popup && G::popup) {
        G::popup->setProgress(done);
        G::popup->pulse(0);
    }
}

void EditProgress::step(int n)
{
    done = n;
    if (target == Target::None) {
        if (shouldShow(n)) show();
        return;
    }
    paint(false);
}

void EditProgress::phase(const QString &text)
{
/*
    The work after the loop -- the filter category rebuild, a refilter -- can be a
    second of its own. A bar already up says what it is doing now; one not up yet still
    appears if the edit as a whole has gone past a second, full, naming the phase.
*/
    phaseText = text;
    done = total;
    if (target == Target::None) {
        if (outermost && total > 1 && clock.elapsed() >= kShowAfterMs) show();
        return;
    }
    if (target == Target::Popup && G::popup)
        G::popup->label.setText(QString("Saving metadata (%1)<br>%2")
                                    .arg(what.toLower(), text));
    paint(true);
}
