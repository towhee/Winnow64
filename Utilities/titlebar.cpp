#include "titlebar.h"

#include <QApplication>
#include <QEvent>
#include <QWidget>

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

void TitleBar::setColor(const QColor &bgColor)
{
    color = bgColor;
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
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
