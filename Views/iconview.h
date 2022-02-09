#ifndef THUMBVIEW_H
#define THUMBVIEW_H

//#include <QAbstractSlider>

//class AbstractSlider : public QAbstractSlider
//{
//    Q_OBJECT
//public:
//    explicit AbstractSlider(QWidget *parent = nullptr);
//};

//#include <QScrollBar>

//// this ScrollBar experiment did not help
//class ScrollBar : public QScrollBar
//{
//    Q_OBJECT
//public:
//    explicit ScrollBar(QWidget *parent = nullptr);
//    void setValue(int value);

//protected:
//    bool event (QEvent *event);
//    void sliderChange(QAbstractSlider::SliderChange change);

//signals:

//public slots:

//};


#include <QtWidgets>
#include "Main/global.h"
#include "File/fstree.h"
#include "Metadata/metadata.h"
#include "Datamodel/datamodel.h"
#include "iconviewdelegate.h"
#include "Datamodel/filters.h"
#include "Cache/threadsafehash.h"
#include <math.h>

class IconView : public QListView
{
    Q_OBJECT

public:
    IconView(QWidget *parent, DataModel *dm, QString objName);

    int iconWidth;
    int iconHeight;
    int labelFontSize = 13;
    bool showIconLabels;
    bool showZoomFrame;
    int badgeSize;
    QSize cellSize;

    int firstVisibleCell;
    int midVisibleCell;
    int lastVisibleCell;
    int visibleCells;

    QModelIndex mouseOverIndex; // for toggle pick
    QModelIndex prevIdx;        // for zoomCursor

    bool calcViewportRange(int row);
    void zoomCursor(const QModelIndex &idx,
                    bool forceUpdate = false,
                    QPoint mousePos = QPoint(-1, -1));

    void updateLayout();
    bool waitUntilOkToScroll();
    bool okToScroll();

    IconViewDelegate *iconViewDelegate;
    void selectThumb(int row);
    void selectThumb(QString &fPath);
    bool isThumb(int row);
    void reportThumb();                             //debugging thumb roles
    QString diagnostics();
    void move();

    QFileInfoList getPicks();
    bool isPick();
    QSize getMinCellSize();
    QSize getMaxCellSize();
    QSize getCellSize();
    int getThumbSpaceWidth(int thumbSpaceHeight);
    int getScrollThreshold(int thumbSpaceHeight);
    QStringList getSelectedThumbsList();        //used by tags, might be useful
    int getCurrentRow();
    QString getCurrentFilePath();       //not used, but might be handy
    int getSelectedCount();

    void setThumbParameters();

    int getHorizontalScrollBarOffset(int row);
    int getVerticalScrollBarOffset(int row);
    int getHorizontalScrollBarMax();
    int getVerticalScrollBarMax();

     QItemSelectionModel *thumbViewSelection;

    bool scrollWhenReady;

    enum JustifyAction {
        Shrink = 1,
        Enlarge = -1
    };
    double bestAspectRatio;

public slots:
    void scrollDown(int);
    void scrollUp(int);
    void scrollPageDown(int);
    void scrollPageUp(int);
    void scrollToRow(int row, QString source);
    void thumbsEnlarge();
    void thumbsShrink();
    int justifyMargin();
    void justify(JustifyAction action);
    void rejustify();
//    void resizeRejustify();
//    void thumbsFit(Qt::DockWidgetArea area);
    void bestAspect();
    void thumbsFitTopOrBottom();
    void invertSelection();                         //in use
    void updateThumbRectRole(const QModelIndex index, QRect iconRect);

    int getFirstVisible();
    int getLastVisible();
    void scannedViewportRange();
    bool allPageIconsLoaded();
    bool isRowVisible(int row);
    int getThumbsPerPage();

    void refreshThumb(QModelIndex idx, int role);
    void refreshThumbs();
    void setThumbParameters(int _thumbWidth, int _thumbHeight,
             int _labelFontSize, bool _showThumbLabels, int _badgeSize);
    void reportThumbs();
    void selectThumb(QModelIndex idx);
    void selectNext();
    void selectPrev();
    void selectUp();
    void selectDown();
    void selectPageUp();
    void selectPageDown();
    void selectFirst();
    void selectLast();
    void selectRandom();
    void selectNextPick();
    void selectPrevPick();
    void copyThumbs();
    void sortThumbs(int sortColumn, bool isReverse);

private slots:
    void selectionChanged (const QItemSelection &selected, const QItemSelection &deselected) override;

protected:
    void startDrag(Qt::DropActions) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent* event) override;
//    void enterEvent(QEvent *event);   // qt6.2
    void leaveEvent(QEvent *event) override;
//    QModelIndex moveCursor(QAbstractItemView::CursorAction cursorAction,
//                           Qt::KeyboardModifiers modifiers);
//    bool event(QEvent* event);      // key capture

private:
    void initLoad();
    bool addFolderImageDataToModel();
    bool isSelectedItem();          // call before getting row or index

    int getNextRow();
    int getPrevRow();
    int getLastRow();                           //not used? Seems handy
    uint getRandomRow();                         //not used? Seems handy

    int getNearestPick();
    int getNextPick();
    int getPrevPick();

    DataModel *dm;

    bool isLeftMouseBtnPressed;
    bool isMouseDrag;
    bool isFitTopOrBottom = false;

    QRect cursorRect;
    QRect iconRect;
    QLabel *zoomFrame;

    // used during gridView resize to keep close to beginning thumb size
    int assignedIconWidth;
    bool skipResize;

signals:
    void togglePick();
    void thumbClick(float xPct, float yPct);        // used in ThumbView::mousePressEvent
    void displayLoupe();
    void updateStatus(bool, QString, QString);
    void updateThumbDockHeight();
};


#endif // THUMBVIEW_H
