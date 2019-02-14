#include "dockwidget.h"
#include <QDebug>

DockWidget::DockWidget(const QString &title, QWidget *parent)
    : QDockWidget(title, parent)
{
    ignore = false;
}

bool DockWidget::event(QEvent *event)
{
//    qDebug() << "Event type:" << event->type() << "Event:" << event;
    if (event->type() == QEvent::MouseButtonDblClick) {
        ignore = true;
        setFloating(!isFloating());
        if (isFloating()) {
            rpt("Double click and floating before move and adjust");
            QRect screenres = QApplication::desktop()->screenGeometry(dw.screen);
            move(QPoint(screenres.x() + dw.pos.x(), screenres.y() + dw.pos.y()));
            ignore = false;
            adjustSize();
            rpt("Double click and floating after move and adjust");
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
        QRect a = QApplication::desktop()->screen(dw.screen)->geometry();
        dw.pos = QPoint(r.x() - a.x(), r.y() - a.y());
        dw.size = event->size();
//        dw.pos = QPoint(x(), y());
        rpt("DockWidget::resizeEvent ");
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
    QRect a = QApplication::desktop()->screen(dw.screen)->geometry();
    dw.pos = QPoint(r.x() - a.x(), r.y() - a.y());
    dw.size = QSize(r.width(), r.height());
//    floatRect = QRect(dwPos, dwSize);
    rpt("DockWidget::moveEvent");
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
