#ifndef DOCKWIDGET_H
#define DOCKWIDGET_H

#include <QObject>
#include <QDockWidget>
#include <QtWidgets>
#include "global.h"

class DockWidget : public QDockWidget
{
    Q_OBJECT
public:
    DockWidget(const QString &title, QWidget *parent = nullptr);
//    explicit DockWidget(const QString &title, QWidget *parent = nullptr);

    QSize sizeHint() const;
    void rpt(QString s);

    struct DWLoc {
        int screen;
        QPoint pos;
        QSize size;
    };
    DWLoc dw;
    bool ignore;

protected:
    bool event(QEvent *event);
    void resizeEvent(QResizeEvent *event);
    void moveEvent(QMoveEvent *event);
};

#endif // DOCKWIDGET_H
