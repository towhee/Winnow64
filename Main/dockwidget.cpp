#include "dockwidget.h"

/*
QDockWidget has a feature where you can double click on the title bar and the dock will toggle
to a floating window and back to its docked state. The problem is if you move and resize the
floating window and then toggle back to the dock and then back to floating again, your
position and size are lost.

This subclass of QDockWidget overrides the MouseButtonDblClick, resizeEvent and moveEvent,
ignoring Qt's attempts to impose its size and location "suggestions. The screen, position and
window size and stored in a struct, which in turn is saved in QSettings for persistence
between sessions.

When a mouse double click occurs in the docked state, the stored screen, postion and size are
used to re-establish the prior state.
*/

DockWidget::DockWidget(const QString &title, QWidget *parent)
    : QDockWidget(title, parent)
{
    ignore = false;
}

bool DockWidget::event(QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        ignore = true;
        setFloating(!isFloating());
        if (isFloating()) {
            // move and size to previous state
            QRect screenRect = QGuiApplication::screens().at(dw.screen)->geometry();
            move(QPoint(screenRect.x() + dw.pos.x(), screenRect.y() + dw.pos.y()));
            ignore = false;
            adjustSize();
        }
        ignore = false;
        return true;
    }
    QDockWidget::event(event);
    return true;
}

void DockWidget::resizeEvent(QResizeEvent *event)
{
    if (ignore) {
        return;
    }
    if (isFloating()) {
        dw.screen = QApplication::desktop()->screenNumber(this);
        QRect r = geometry();
        QRect a = QGuiApplication::screens().at(dw.screen)->geometry();
        dw.pos = QPoint(r.x() - a.x(), r.y() - a.y());
        dw.size = event->size();
    }
}

QSize DockWidget::sizeHint() const
{
    return dw.size;
}

void DockWidget::moveEvent(QMoveEvent *event)
{
    if (ignore || !isFloating()) return;
    dw.screen = QApplication::desktop()->screenNumber(this);
    QRect r = geometry();
    QRect a = QGuiApplication::screens().at(dw.screen)->geometry();
    dw.pos = QPoint(r.x() - a.x(), r.y() - a.y());
    dw.size = QSize(r.width(), r.height());
    QDockWidget::moveEvent(event);  // suppress compiler warning
}

void DockWidget::rpt(QString s)
{
    qDebug() << s
             << "isFloating" << isFloating()
             << "screen" << dw.screen
             << "pos" << dw.pos
             << "size" << dw.size;
    s = "";
}
