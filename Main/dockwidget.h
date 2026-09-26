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
    /* Show/clear a blue "active" border, e.g. when the panel this button toggles is
       visible (mirrors a conventional checked toolbutton). */
    void setActive(bool on);

protected:
//    void enterEvent(QEvent*);
//    void leaveEvent(QEvent*);

private:
    QColor btnHover;
};

//-------------------------------------------------------------------------------------------
class FrameLineBox : public QWidget
{
/*
    A container that draws the frameLine (G::frameLineColor, G::frameLineWidth) on the
    chosen sides and insets its content by the same width, so the line is never painted
    over and nothing overlaps the content (an overlay would break Qt's scroll blits).
    Holds every panel's content (DockWidget::setWidget) and is the central widget.
*/
    Q_OBJECT
public:
    explicit FrameLineBox(QWidget *parent = nullptr);
    void setSides(Qt::Edges sides);
    Qt::Edges sides() const { return m_sides; }
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    Qt::Edges m_sides = Qt::TopEdge | Qt::LeftEdge | Qt::RightEdge | Qt::BottomEdge;
};

//-------------------------------------------------------------------------------------------
class DockTitleBar : public QWidget
{
    Q_OBJECT
public:
    DockTitleBar(const QString &title, QHBoxLayout *titleBarLayout/*, QWidget *parent = nullptr*/);
    void setStyle();
    /* Drop the 1px rule under the title. The Develop dock does this: its action row
       carries the panel separator (G::panelBorderHeight) a few pixels lower, and two
       rules that close to each other read as a mistake. */
    void setBottomBorderVisible(bool visible);
    void setTitle(QString title);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
private:
    QLabel *titleLabel;
    bool bottomBorder = true;           // see setBottomBorderVisible

};

/* Shared tooltip display for dock title bars, tabs and the thumbDock title.
   Positions the tip ourselves (rather than relying on Qt's per-platform
   placement) so the offset below the cursor is consistent across macOS and
   Windows. */
void showDockToolTip(const QPoint &globalPos, const QString &tip, QWidget *w);

//-------------------------------------------------------------------------------------------
class DockWidget : public QDockWidget
{
    Q_OBJECT
public:
    DockWidget(const QString &title, QString objName, QWidget *parent = nullptr);
    bool isCollapsed() const { return m_isCollapsed; }
    /*  These HIDE QDockWidget's (non-virtual) versions, so they are only reached through
        a DockWidget pointer -- which every panel is. setWidget puts the content in a
        FrameLineBox, so widget() is that box and content() is what was passed in.
        setTitleBarWidget re-decides the box's top side (see syncFrameSides). */
    void setWidget(QWidget *content);
    void setTitleBarWidget(QWidget *titleBar);
    QWidget *content() const { return m_content; }
    /*  Off for a panel that is not bordered (the Module dock): no sides, no inset. */
    void setFrameLineVisible(bool visible);

public slots:
    void setCollapsed(bool collapse);
    void toggleCollapsed();

private:
    void toggleTopLevel();
    QRect setDefaultFloatingGeometry();
    QRect defaultFloatingGeometry;
    QRect floatingGeometry;
    bool hasCustomTitleBar();
    bool isTitleBarPos(const QPoint &globalPos) const;
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
    int m_uncollapsedContentMinH = 0;
    FrameLineBox *m_frame = nullptr;
    bool m_frameLineVisible = true;
    QWidget *m_content = nullptr;
    void syncFrameSides();

signals:
    void focus(DockWidget *dw);
    void closeFloatingDock();
    void collapsedChanged(bool collapsed);

protected:
    bool event(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
};

#endif // DOCKWIDGET_H
