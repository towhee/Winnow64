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
    // QModelIndex index;      // the index of the model row that contains the button
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
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
private:
    QLabel *titleLabel;

};

//-------------------------------------------------------------------------------------------
class DockWidget : public QDockWidget
{
    Q_OBJECT
public:
    DockWidget(const QString &title, QString objName, QWidget *parent = nullptr);
    bool isCollapsed() const { return m_isCollapsed; }

public slots:
    void setCollapsed(bool collapse);
    void toggleCollapsed();

private:
    void toggleTopLevel();
    QRect setDefaultFloatingGeometry();
    QRect defaultFloatingGeometry;
    QRect floatingGeometry;
    bool hasCustomTitleBar();
    void rpt(QString s);
    QRect deconstructSavedGeometry(QByteArray geometry);
    void save();
    void restore();
    bool doubleClickDocked;
    bool isRestoring;
    bool m_isCollapsed = false;
    QSize m_uncollapsedSize;
    int m_uncollapsedMinH = 0;
    int m_uncollapsedMaxH = QWIDGETSIZE_MAX;
    int m_uncollapsedBodyMinH = 0;

signals:
    void focus(DockWidget *dw);
    void closeFloatingDock();
    void collapsedChanged(bool collapsed);

protected:
    bool event(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
};

#endif // DOCKWIDGET_H
