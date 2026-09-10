#include "Main/global.h"
#include "Utilities/popup.h"
#include <QPainter>
#include <QApplication>
#include <QDebug>
#include <QThread>

// --- macOS special case: show window even if app inactive (e.g., Finder drag) ---
#ifdef Q_OS_MAC
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>      // defines objc_msgSend
typedef unsigned long NSUInteger;
typedef long NSInteger;
#endif

/*
This class shows a pop up message for a given time with an assigned transparency.
The default time is 1500 ms and the default transparency is 75%.

The widget will autosize to fit the text message if isAutoSize is true.

If there are a series of progress messages (example loading filters) then use the
first message to set the size and then set isAutoSize = false and setPopupText to
change the text while keeping the pop up box size constant.

In Winnow an instance of this class is created in global so that it is available to
all parts of the program. It is created once in MW::initialize.
*/

Popup::Popup(QWidget *source, QWidget *centralWidget, QWidget *parent) : QWidget(parent)
{
    this->source = source;

    setAttribute(Qt::WA_NativeWindow, true);        // ensure NSWindow exists

    this->centralWidget = centralWidget;
    setWindowFlags(Qt::FramelessWindowHint |        // Disable window decoration
                   Qt::ToolTip |                       // Discard display in a separate window
                   Qt::WindowStaysOnTopHint);       // Set on top of all windows
    // setWindowFlags(Qt::FramelessWindowHint |        // Disable window decoration
    //                Qt::Tool |                       // Discard display in a separate window
    //                Qt::WindowStaysOnTopHint);       // Set on top of all windows
    setAttribute(Qt::WA_TranslucentBackground);     // Indicates that the background will be transparent
    setAttribute(Qt::WA_ShowWithoutActivating);     // At the show, the widget does not get the focus automatically
    setAttribute(Qt::WA_TransparentForMouseEvents, true);

    label.setTextFormat(Qt::RichText);
    label.setStyleSheet(
        "QLabel "
        "{"
            "color: white;"
            "background-color: transparent;"
            "font-size: 16px;"
            "margin-top: 10px;"
            "margin-bottom: 10px;"
            "margin-left: 10px;"
            "margin-right: 10px; "
        "}"
    );
    setProgressVisible(false);
    progressBar.setTextVisible(false);
    layout.addWidget(&label, 0, 0);
    layout.addWidget(&progressBar, 1, 0);
    setLayout(&layout);

    hideTimer = new QTimer(this);
    connect(hideTimer, &QTimer::timeout, this, &Popup::reset);
}

void Popup::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect roundedRect;
    roundedRect.setX(rect().x() + 5);
    roundedRect.setY(rect().y() + 5);
    roundedRect.setWidth(rect().width() - 10);
    roundedRect.setHeight(rect().height() - 10);

    painter.setBrush(QBrush(QColor(0,0,0,180)));
    painter.setPen(Qt::NoPen);

    painter.drawRoundedRect(roundedRect, 10, 10);
}

void Popup::showPopup(const QString &text,
                      int msDuration,
                      bool isAutoSize,
                      float opacity,
                      Qt::Alignment alignment)
{
    QPointer<Popup> self(this);
    QTimer::singleShot(0, this, [self, text, msDuration, isAutoSize, opacity, alignment] {
        if (!self) return;
        self->showPopup1(text, msDuration, isAutoSize, opacity, alignment);
        self->raise();
        self->update();
    });
}

void Popup::showPopupNow(const QString &text,
                         int msDuration,
                         bool isAutoSize,
                         float opacity,
                         Qt::Alignment alignment)
{
/*
    THE SAME MESSAGE, ON THE SCREEN NOW, WITHOUT PUMPING THE APPLICATION.

    showPopup defers through a zero timer, and that deferral is what makes it safe to
    call from any thread. The price is paid by a SYNCHRONOUS GUI-THREAD loop, which has
    to turn the event loop before its message is seen -- and qApp->processEvents() does
    not just deliver that one timer, it repaints the entire main window. Measured on a
    keyword move: 923 ms across three such pumps, of which 678 ms was tree-view rows
    drawn through the stylesheet style and an AppKit NSAppearance block apiece. The
    message itself is a small top-level window and costs none of that.

    FALLS BACK TO THE QUEUED ROUTE OFF THE GUI THREAD rather than asserting. Callers do
    not all know which thread they are on, and a diagnostic message is never worth a
    crash; the deferred path is correct everywhere, just later.
*/
    if (QThread::currentThread() != thread()) {
        showPopup(text, msDuration, isAutoSize, opacity, alignment);
        return;
    }

    /*  ANY SHOW ALREADY IN THE QUEUE IS DELIVERED FIRST, or an earlier showPopup's
        single-shot would land later and overwrite this message with a stale one.
        sendPostedEvents is aimed at THIS OBJECT, so it delivers the popup's own events
        and nothing else -- which is the whole distinction being drawn here. A bare
        processEvents would deliver the main window's pending update requests too, and
        those are the 112-577 ms this exists to stop paying. */
    QCoreApplication::sendPostedEvents(this, 0);

    showPopup1(text, msDuration, isAutoSize, opacity, alignment);
    raise();
    repaint();
    pulseClock.restart();
}

void Popup::pulse(int minIntervalMs)
{
/*
    Repaint the popup from inside a long synchronous loop.

    THE PROGRESS BAR HAS NEVER ACTUALLY BEEN DRAWN by the loops that set it. setProgress
    writes the value and returns; with nothing turning the event loop the widget is never
    asked to paint, so a rewrite of several thousand sidecars showed a bar frozen at zero
    -- indistinguishable from a hang, which is how it was reported.

    THROTTLED, or it becomes the cost it was written to avoid. A repaint per row is a
    pump per row in smaller pieces; ten a second is past the rate anyone reads a number
    at, and is what keeps this a rounding error against the work it is reporting.

    SILENT WHEN THERE IS NOTHING TO SEE -- not visible, or called from a worker -- so a
    caller can sprinkle it without first proving the popup is up.
*/
    if (QThread::currentThread() != thread()) return;
    if (!isVisible()) return;
    if (pulseClock.isValid() && pulseClock.elapsed() < minIntervalMs) return;
    pulseClock.restart();
    QCoreApplication::sendPostedEvents(this, 0);
    repaint();
}

void Popup::showPopup1(const QString &text,
                 int msDuration,
                 bool isAutoSize,
                 float opacity,
                 Qt::Alignment alignment)
{
    // qDebug() << "PopUp::showPopup" << text << msDuration;
    hideTimer->stop();
    // okayToHide = true;
    // hide();
    if (msDuration > 0) okayToHide = false;
    if (msDuration == 0) {
        openAndNoTimeout = true;
        okayToHide = false;
    }

    popupDuration = msDuration;
    popupOpacity = opacity;
    popupAlignment = alignment;
    popupText = text;
    label.setAlignment(alignment/* | Qt::AlignVCenter*/);
    label.setText(popupText);
    if (isAutoSize) adjustSize();               // With the recalculation notice sizes
    setWindowOpacity(static_cast<double>(popupOpacity));

    // popup geometry
    QRect cwRect = centralWidget->geometry();
    int w = width();
    int h = height();
    // int x = source->geometry().x() + cwRect.x() + cwRect.width() / 2 - w / 2;
    // int y = source->geometry().y() + cwRect.y() + cwRect.height() / 2 - h / 2;
    QPoint center = centralWidget->mapToGlobal(centralWidget->rect().center());
    int x = center.x() - w / 2;
    int y = center.y() - h / 2;
    setGeometry(x, y, w, h);

#ifdef Q_OS_MAC
    // --- macOS special case: show window even if app inactive (e.g., Finder drag) ---
    QWidget::setVisible(true); // mark QWidget as shown
    WId wid = winId();
    if (wid) {
        id view = (id)reinterpret_cast<void *>(wid);
        id nsWindow = ((id(*)(id, SEL))objc_msgSend)(view, sel_registerName("window"));
        if (nsWindow) {
            // ignores mouse events
            ((void(*)(id, SEL, bool))objc_msgSend)(nsWindow,
                 sel_registerName("setIgnoresMouseEvents:"),
                 true);
            // NSStatusWindowLevel is 25 in current SDKs
            NSInteger level = 25;
            ((void(*)(id, SEL, NSInteger))objc_msgSend)(nsWindow,
                 sel_registerName("setLevel:"),
                 level);
            // show even when app inactive
            ((void(*)(id, SEL))objc_msgSend)(nsWindow,
                 sel_registerName("orderFrontRegardless"));
        }
    }
#else
    show();
#endif

    // set popupDuration = 0 to keep open and manually close like a msgbox
    if (popupDuration > 0) hideTimer->start(popupDuration);
}

void Popup::reset()
{
    // qDebug() << "PopUp::reset";
    okayToHide = true;
    openAndNoTimeout = false;
    hideTimer->stop();
    hide();
}

void Popup::hide()
{
    if (okayToHide) {
        QWidget::hide();
    }
}

void Popup::setProgressVisible(bool isVisible)
{
    if (isVisible) {
        progressBar.setStyleSheet(
            "QProgressBar"
            "{"
                "border: 1px solid black;"
                "background-color: #202020;"
                "margin-bottom: 6px;"
            "}"
            "QProgressBar::chunk"
            "{"
                "background-color: cadetblue;"
            "}"
        );
        progressBar.setMaximumHeight(20);
    }
    else {
        progressBar.setStyleSheet(
            "QProgressBar"
            "{"
                "border: 1px solid grey;"
                "margin-bottom: 0px;"
            "}"
        );
        progressBar.setMaximumHeight(0);
        progressBar.setValue(0);
    }
}

void Popup::setProgressMax(int maxProgress)
{
    progressBar.setMaximum(maxProgress);
    progressBar.setMinimum(0);
}

void Popup::setProgress(int progress)
{
    progressBar.setValue(progress);
}

void Popup::setPopupAlignment(Qt::Alignment alignment)
{
    label.setAlignment(alignment/* | Qt::AlignVCenter*/);
}

void Popup::setPopupText(const QString &text)
{
    label.setText(text);
    // repaint(0, 0, width(), height());
    update();  // instead of repaint
}

void Popup::setPopupOpacity(float opacity)
{
    popupOpacity = opacity;
    setWindowOpacity(static_cast<double>(opacity));
}

float Popup::getPopupOpacity() const
{
    return popupOpacity;
}

void Popup::setPopupDuration(int msDuration)
{
    popupDuration = msDuration;
}

void Popup::setPopUpSize(int w, int h)
{
    resize(w, h);
}
