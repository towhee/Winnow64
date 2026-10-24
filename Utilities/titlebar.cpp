#include "titlebar.h"

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QWindow>

#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif
#ifdef Q_OS_MAC
#include "Utilities/mac.h"
#endif
#ifdef Q_OS_LINUX
#include "Utilities/linux.h"
#endif

QColor TitleBar::color;

/*
    SHELVED ON macOS 2026-10-24, kept for a revisit; both off. macOS shows the system's
    own title bar again, with the window title and file path. Windows (title bars were
    Winnow-coloured long before TitleBar) and Linux keep the Winnow background.
    isColorMacTitleBars: the Winnow background on macOS title bars (Mac::setTitleBarColor).
    isModesInTitleBar: the Module dock's buttons in the macOS title bar (TitleBarStrip).
*/
static constexpr bool isColorMacTitleBars = false;
static constexpr bool isModesInTitleBar = false;

void TitleBar::setColor(const QColor &bgColor)
{
    color = bgColor;
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
#ifdef Q_OS_MAC
    if (!isColorMacTitleBars) return;
#endif
    static TitleBar *filter = nullptr;
    if (!filter) {
        filter = new TitleBar(qApp);
        qApp->installEventFilter(filter);
    }
    for (QWidget *w : QApplication::topLevelWidgets())
        if (w->isVisible()) apply(w);
#elif defined(Q_OS_LINUX)
    Linux::setTitleBarColor(bgColor);
#endif
}

void TitleBar::apply(QWidget *w)
{
/*
    Only windows that have a title bar: menus, tooltips, combo drop-downs and frameless
    windows (the floating docks draw their own) are skipped. Called on Show, when the
    native window exists, so winId() never creates one.
*/
    if (!color.isValid() || !w->isWindow()) return;
    const Qt::WindowType type = w->windowType();
    if (type == Qt::Popup || type == Qt::ToolTip || type == Qt::SplashScreen) return;
    if (w->windowFlags() & Qt::FramelessWindowHint) return;
    if (!w->windowHandle()) return;
#if defined(Q_OS_WIN)
    Win::setTitleBarColor(w->winId(), color);
#elif defined(Q_OS_MAC)
    /* Qt (6.9+) recomputes titlebarAppearsTransparent from this flag every time it
       applies window flags (show, window state changes), so setting the NSWindow
       property alone is undone at once. The flag goes on the QWindow, not the widget:
       QWidget::setWindowFlags would hide the window, and inside a Show event re-enter. */
    w->windowHandle()->setFlag(Qt::NoTitleBarBackgroundHint, true);
    Mac::setTitleBarColor(w->winId(), color);
#endif
}

bool TitleBar::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Show && obj->isWidgetType()) {
        QWidget *w = static_cast<QWidget *>(obj);
        if (w->isWindow()) apply(w);
    }
    return QObject::eventFilter(obj, event);
}

bool TitleBarStrip::isSupported()
{
#ifdef Q_OS_MAC
    return isModesInTitleBar;
#else
    return false;
#endif
}

TitleBarStrip::TitleBarStrip(QWidget *window) : QWidget(window)
{
    setObjectName("TitleBarStrip");
    setAttribute(Qt::WA_TranslucentBackground);     // the window paints under it
    setVisible(false);                              // until place() knows the height
    if (!isSupported()) return;
    // before the window is shown, so it is created with the full-size content view
    window->setWindowFlag(Qt::ExpandedClientAreaHint, true);
    window->installEventFilter(this);
}

void TitleBarStrip::setContent(QWidget *left, QWidget *centre, QWidget *right)
{
    this->left = left;
    this->centre = centre;
    this->right = right;
    for (QWidget *w : {left, centre, right}) if (w) w->setParent(this);
    place();
}

void TitleBarStrip::place()
{
    QWidget *w = parentWidget();
    const QWindow *wh = w->windowHandle();
    const int safeTop = wh ? wh->safeAreaMargins().top() : 0;
    if (safeTop <= 0) {
        // no title bar to sit in (full screen): give the window its top back
        if (w->contentsMargins().top() != safeTop) w->setContentsMargins(0, 0, 0, 0);
        setVisible(false);
        return;
    }

    int h = safeTop;
    for (QWidget *c : {left, centre, right}) if (c) h = qMax(h, c->sizeHint().height());
    /* Taller than the system bar: the user top margin pushes the window's layout down
       (contentsMargins() is the larger of it and the safe area, not the sum). */
    const int userTop = h > safeTop ? h : 0;
    const QMargins m = w->contentsMargins();
    if (m.top() != qMax(userTop, safeTop)) w->setContentsMargins(0, userTop, 0, 0);

    setGeometry(0, 0, w->width(), h);
    const int gap = 12;
    int x0 = gap;
#ifdef Q_OS_MAC
    if (w->windowHandle()) x0 = Mac::titleBarButtonsRight(w->winId()) + gap;
#endif
    int x1 = width() - gap;
    auto put = [h](QWidget *c, int x) {
        const QSize sz = c->sizeHint();
        c->setGeometry(x, (h - sz.height()) / 2, sz.width(), sz.height());
    };
    if (left) {
        put(left, x0);
        x0 += left->sizeHint().width() + gap;
    }
    if (right) {
        x1 -= right->sizeHint().width();
        put(right, x1);
        x1 -= gap;
    }
    if (centre) {
        const int cw = centre->sizeHint().width();
        const int x = qBound(x0, (width() - cw) / 2, qMax(x0, x1 - cw));
        put(centre, x);
    }
    setVisible(true);
    raise();
}

bool TitleBarStrip::eventFilter(QObject *obj, QEvent *event)
{
    /* ContentsRectChange is how a safe-area change reaches the widget (entering or
       leaving full screen); Resize keeps the strip as wide as the window. */
    if (obj == parentWidget()) {
        switch (event->type()) {
        case QEvent::Show:
#ifdef Q_OS_MAC
            Mac::setTitleTextVisible(parentWidget()->winId(), false);
#endif
            place();
            break;
        case QEvent::Resize:
        case QEvent::ContentsRectChange:
            place();
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void TitleBarStrip::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (QWindow *wh = window()->windowHandle()) wh->startSystemMove();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TitleBarStrip::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
#ifdef Q_OS_MAC
    Mac::titleBarDoubleClick(window()->winId());
#endif
    event->accept();
}
