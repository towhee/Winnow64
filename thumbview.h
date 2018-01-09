#ifndef THUMBVIEW_H
#define THUMBVIEW_H

#include <QtWidgets>
//#include "mainwindow.h"
#include "global.h"
#include "fstree.h"
#include "metadata.h"
#include "datamodel.h"
#include "thumbviewdelegate.h"
#include "filters.h"
#include <math.h>

//class Scrollbar : public QScrollBar
//{
//    Q_OBJECT

//public:
//    Scrollbar(QWidget *parent);
//    void test();

//protected:
//    bool eventFilter(QObject *obj, QEvent *event);

//signals:
//    void updateScrollTo();
//};

class ThumbView : public QListView
{
    Q_OBJECT

public:
    ThumbView(QWidget *parent, DataModel *dm);

    int thumbWidth;
    int thumbHeight;
    int thumbSpacing;
    int thumbPadding;
    int labelFontSize;
    bool showThumbLabels;
    bool wrapThumbs;

    bool isTopOrBottomDock;
    bool isFloat;       // set by MW ...

    void updateLayout();


    ThumbViewDelegate *thumbViewDelegate;
    void selectThumb(int row);
    void selectThumb(QString &filePath);
    bool isThumb(int row);
    void reportThumb();                             //debugging thumb roles

    int getFirstVisible();
    int getLastVisible();

    QFileInfoList getPicks();
    bool isPick();
    int getThumbSpaceMin();
    int getThumbSpaceMax();
    QSize getThumbCellSize();
    int getThumbSpaceWidth(int thumbSpaceHeight);
    int getScrollThreshold(int thumbSpaceHeight);
    QStringList getSelectedThumbsList();        //used by tags, might be useful
    int getCurrentRow();
    QString getCurrentFilename();       //not used, but might be handy
    void setThumbParameters(bool okayToUpdateThumbDockHeight);

//    void scrollTo(const QModelIndex &index, ScrollHint hint);
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

public slots:
    void scrollToCurrent(int row);
    void thumbsEnlarge();
    void thumbsShrink();
    void thumbsFit(Qt::DockWidgetArea area);
    void thumbsFitTopOrBottom();
    void toggleFilterPick(bool isFilter);
    void invertSelection();                         //in use
    void updateThumbRectRole(const QModelIndex index, QRect iconRect);

    void refreshThumbs();
    void setIcon(int row, QImage thumb);
    void setThumbParameters(int _thumbWidth, int _thumbHeight, int _thumbSpacing,
             int _thumbPadding, int _labelFontSize, bool _showThumbLabels, bool _wrapThumbs);
//    void setThumbGridParameters(int _thumbWidthGrid, int _thumbHeightGrid, int _thumbSpacingGrid,
//             int _thumbPaddingGrid, int _labelFontSizeGrid, bool _showThumbLabelsGrid);
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
    void delaySelectCurrentThumb();

protected:
    bool eventFilter(QObject *obj, QEvent *event);
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

signals:
    void thumbClick(float xPct, float yPct);        //used in ThumbView::mousePressEvent
    void displayLoupe();
    void updateStatus(bool, QString);
    void updateThumbDockHeight();
};


#endif // THUMBVIEW_H
