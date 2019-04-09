#ifndef THUMBVIEW_H
#define THUMBVIEW_H

#include <QAbstractSlider>

//class AbstractSlider : public QAbstractSlider
//{
//    Q_OBJECT
//public:
//    explicit AbstractSlider(QWidget *parent = nullptr);
//};

#include <QScrollBar>

// this ScrollBar experiment did not help
class ScrollBar : public QScrollBar
{
    Q_OBJECT
public:
    explicit ScrollBar(QWidget *parent = nullptr);
    void setValue(int value);

protected:
    bool event (QEvent *event);
    void sliderChange(QAbstractSlider::SliderChange change);

signals:

public slots:

};


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

    int thumbWidth;
    int thumbHeight;
    int thumbSpacing;
    int thumbPadding;
    int labelFontSize;
    bool showThumbLabels;
//    bool wrapThumbs;
    int badgeSize;
    QSize cellSize;

//    bool isTopOrBottomDock;
    bool isFloat;       // set by MW ...
    bool isBestAspect;

    int firstVisibleRow;
    int midVisibleRow;
    int lastVisibleRow;
    int thumbsPerPage;

    void updateLayout();

    IconViewDelegate *iconViewDelegate;
    void selectThumb(int row);
    void selectThumb(QString &filePath);
    bool isThumb(int row);
    void reportThumb();                             //debugging thumb roles

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

    int thumbSize;
    QString pickMemorySize;
    QString filterStr;
    bool pickFilter;
    bool readyToScroll;
    bool scrollPaintFound;

    enum JustifyAction {
        Shrink = 1,
        Enlarge = -1
    };
    double bestAspectRatio;
    int iconWMax;               // widest icon found in datamodel
    int iconHMax;               // highest icon found in datamodel

public slots:
    void scrollDown(int);
    void scrollUp(int);
    void scrollPageDown(int);
    void scrollPageUp(int);
    void scrollToRow(int row);
    void thumbsEnlarge();
    void thumbsShrink();
    void justify(JustifyAction action);
    void rejustify();
//    void resizeRejustify();
    void thumbsFit(Qt::DockWidgetArea area);
    void bestAspect();
    void thumbsFitTopOrBottom();
    void invertSelection();                         //in use
    void updateThumbRectRole(const QModelIndex index, QRect iconRect);

    int getFirstVisible();
    int getLastVisible();
    void setViewportParameters();
    bool isRowVisible(int row);
    int getThumbsPerPage();

    void refreshThumb(QModelIndex idx, int role);
    void refreshThumbs();
    void setIcon(int row, QImage thumb);
    void setThumbParameters(int _thumbWidth, int _thumbHeight, int _thumbSpacing,
             int _thumbPadding, int _labelFontSize, bool _showThumbLabels,
             /*bool _wrapThumbs, */int _badgeSize);
    void reportThumbs();
    void selectThumb(QModelIndex idx);
    void selectNext();
    void selectPrev();
    void selectUp();
    void selectDown();
    void selectFirst();
    void selectLast();
    void selectRandom();
    void selectNextPick();
    void selectPrevPick();
    void copyThumbs();
    void sortThumbs(int sortColumn, bool isReverse);

private slots:
//    void delaySelectCurrentThumb();
    void selectionChanged (const QItemSelection &selected, const QItemSelection &deselected);

protected:
    void startDrag(Qt::DropActions);
    void wheelEvent(QWheelEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void resizeEvent(QResizeEvent* event);
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
    int getRandomRow();                         //not used? Seems handy

    int getNearestPick();
    int getNextPick();
    int getPrevPick();

    DataModel *dm;
    QSize treeViewSize;


    bool isLeftMouseBtnPressed;
    bool isMouseDrag;

    // used during gridView resize to keep close to beginning thumb size
    int assignedThumbWidth;
    bool skipResize;

signals:
    void togglePick();
    void thumbClick(float xPct, float yPct);        // used in ThumbView::mousePressEvent
    void displayLoupe();
    void updateStatus(bool, QString);
    void updateThumbDockHeight();
};


#endif // THUMBVIEW_H
