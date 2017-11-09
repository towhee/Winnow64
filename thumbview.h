#ifndef THUMBVIEW_H
#define THUMBVIEW_H

#include <QtWidgets>
#include "global.h"
#include "fstree.h"
#include "metadata.h"
#include "datamodel.h"
#include "thumbviewdelegate.h"
//#include "thumbviewfilter.h"
#include "filters.h"

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

    int thumbWidthGrid;
    int thumbHeightGrid;
    int thumbSpacingGrid;
    int thumbPaddingGrid;
    int labelFontSizeGrid;
    bool showThumbLabelsGrid;

    bool isThumbWrapWhenTopOrBottomDock;
    bool isTopOrBottomDock;
    bool isGrid;        // set by MW so know which thumb parameters to use
    bool isFloat;       // set by MW ...
    bool isAutoFit;

    void updateLayout();

    ThumbViewDelegate *thumbViewDelegate;
    void selectThumb(int row);
    void selectThumb(QString &filePath);
    void reportThumb();                             //debugging thumb roles


    QFileInfoList getPicks();
    bool isPick();
    int getThumbSpaceMin();
    int getThumbSpaceMax();
    QSize getThumbCellSize();
    QStringList getSelectedThumbsList();        //used by tags, might be useful
    int getCurrentRow();
    QString getCurrentFilename();       //not used, but might be handy
    void setThumbParameters();
    void forceScroll(int row);

    QStandardItemModel *thumbViewModel;     //dm
//    QSortFilterProxyModel *thumbViewFilter;
//     ThumbViewFilter *thumbViewFilter;     //dm
//    ThumbViewFilter *thumbViewFilterTest;
    QItemSelectionModel *thumbViewSelection;
//    QFileInfoList thumbFileInfoList;     //dm
//    QStringList imageFilePathList;     //dm
//    QFileInfoList dirFileInfoList;     //dm
//    QDir::SortFlags thumbsSortFlags;     //dm

    int thumbSize;
    QString filterStr;
    bool pickFilter;

public slots:
    void thumbsEnlarge();
    void thumbsShrink();
    void thumbsFit(Qt::DockWidgetArea area);
    void toggleFilterPick(bool isFilter);
    void invertSelection();                         //in use
    void updateThumbRectRole(const QModelIndex index, QRect iconRect);

    void refreshThumbs();
    void setIcon(QStandardItem *item, QImage thumb, QString folderPath);
    void setThumbParameters(int _thumbWidth, int _thumbHeight, int _thumbSpacing,
             int _thumbPadding, int _labelFontSize, bool _showThumbLabels);
    void setThumbGridParameters(int _thumbWidthGrid, int _thumbHeightGrid, int _thumbSpacingGrid,
             int _thumbPaddingGrid, int _labelFontSizeGrid, bool _showThumbLabelsGrid);
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
    void startDrag(Qt::DropActions);
    void wheelEvent(QWheelEvent *event);
    void mousePressEvent(QMouseEvent *event);
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

//    QString currentViewDir;     //dm
//    QDir *thumbsDir;     //dm
//    QStringList *fileFilters;     //dm
//    QList<QStandardItem*> *thumbList;     //dm
//    QFileInfo thumbFileInfo;     //dm
//    QImage emptyImg;     //dm
    QWidget *mw;
    DataModel *dm;
    QSize treeViewSize;

signals:
    void thumbClick(float xPct, float yPct);        //used in ThumbView::mousePressEvent
    void displayLoupe();
    void updateStatus(bool, QString);
    void updateThumbDock();
};

#endif // THUMBVIEW_H
