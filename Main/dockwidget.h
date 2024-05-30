#ifndef DOCKWIDGET_H
#define DOCKWIDGET_H

#include <QDockWidget>
#include "global.h"
#include "widgetcss.h"

/*-------------------------------------------------------------------------------------------
class RichTextTabBar : public QTabBar
{
    Q_OBJECT
public:
    RichTextTabBar(QWidget *parent = nullptr);
    void setTabText(int index, const QString& text);

private:
    int mTabWidth = 40;
    int mTabHeight = 20;
};

/* Not being used.
//-------------------------------------------------------------------------------------------
class RichTextTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    RichTextTabWidget(QWidget* parent = nullptr);
    void setRichTextTabBar(RichTextTabBar *rtb);
    void setTabText(int index, const QString &label);

private:
    RichTextTabBar* tabBar() const;
};
*/

//-------------------------------------------------------------------------------------------
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

//-------------------------------------------------------------------------------------------
class DockTitleBar : public QWidget
{
    Q_OBJECT
public:
    DockTitleBar(const QString &title, QHBoxLayout *titleBarLayout/*, QWidget *parent = nullptr*/);
    void setStyle();
    void setTitle(QString title);
    QSize sizeHint() const override;
protected:
//    void paintEvent(QPaintEvent *) override;
private:
    QLabel *titleLabel;

};

//-------------------------------------------------------------------------------------------
class DockWidget : public QDockWidget
{
    Q_OBJECT
public:
    DockWidget(const QString &title, QString objName, QWidget *parent = nullptr);
    QSize sizeHint() const override;

    void rpt(QString s);
    QRect deconstructSavedGeometry(QByteArray geometry);

    struct DWLoc {
        int screen;
        QPoint pos;
        QSize size;
        QRect geometry;
        qreal devicePixelRatio = 1;
    };
    DWLoc dw;

    QSize dprSize;
    double prevDpr = 1;

    bool ignoreResize;
    // bool isInitializing = true;

    void save();
    void restore();

private:

private slots:
    // void onTopLevelChanged(bool topLevel);

protected:
    bool event(QEvent *event) override;
    // void resizeEvent(QResizeEvent *event) override;
    // void moveEvent(QMoveEvent *event) override;
    // void showEvent(QShowEvent *event) override;
};

#endif // DOCKWIDGET_H
