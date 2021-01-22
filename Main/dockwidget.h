#ifndef DOCKWIDGET_H
#define DOCKWIDGET_H

#include <QDockWidget>
#include "global.h"
#include "widgetcss.h"

/*-------------------------------------------------------------------------------------------*/
class BarBtn : public QToolButton
{
    Q_OBJECT
public:
    BarBtn(/*QWidget *parent = nullptr*/);
    QSize sizeHint() const override;
//    QModelIndex index;      // the index of the model row that contains the button
    int itemIndex;          // the unique index assigned to the child item that contains the button
    QString name;           // The value of col 0
    QString parName;        // The value of col 0 for the parent
    QString type;           // ie "effect"
    void setIcon(QString path, double opacity);
    void setIcon(const QIcon &icon);

protected:
//    void enterEvent(QEvent*);
//    void leaveEvent(QEvent*);

private:
    QColor btnHover;
};

/*-------------------------------------------------------------------------------------------*/
class DockTitleBar : public QFrame
{
    Q_OBJECT
public:
    DockTitleBar(const QString &title, QHBoxLayout *titleBarLayout/*, QWidget *parent = nullptr*/);
    void setStyle();
    void setTitle(QString title);
    //    QSize sizeHint() const;
protected:
//    void paintEvent(QPaintEvent *) override;
private:
    QLabel *titleLabel;

};

/*-------------------------------------------------------------------------------------------*/
class DockWidget : public QDockWidget
{
    Q_OBJECT
public:
    DockWidget(const QString &title, QWidget *parent = nullptr);
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
