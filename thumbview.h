#ifndef THUMBVIEW_H
#define THUMBVIEW_H

#include <QtWidgets>
#include "global.h"
#include "fstree.h"
#include "metadata.h"
#include "thumbviewdelegate.h"

class ThumbView : public QListView
{
    Q_OBJECT

public:

    ThumbView(QWidget *parent, Metadata *metadata, bool iconView);

    enum UserRoles {
        FileNameRole = Qt::UserRole + 1,
        SortRole,
        LoadedRole,
        PickedRole,
        ThumbRectRole,
        PathRole,
        FileTypeRole,
        FileSizeRole,
        CreatedRole,
        ModifiedRole,
        LabelRole,
        RatingRole
    };

    enum dataModelColumns {
        PathColumn,
        NameColumn,
        TypeColumn,
        SizeColumn,
        CreatedColumn,
        ModifiedColumn,
        PickedColumn,
        LabelColumn,
        RatingColumn,
        MegaPixelsColumn,
        DimensionsColumn,
        ApertureColumn,
        ShutterspeedColumn,
        ISOColumn,
        CameraModelColumn,
        FocalLengthColumn,
        TitleColumn,
        TotalColumns    // insert additional columns before this
    };

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
    bool isAutoFit;

    bool load(QString &dir, bool inclSubfolders);
    void updateImageList();
    void addMetadataToModel();

    ThumbViewDelegate *thumbViewDelegate;
    void selectThumb(int row);
    void selectThumb(QString &filePath);
    void reportThumb();                             //debugging thumb roles


    QFileInfoList getPicks();
    bool isPick();
    QStringList getSelectedThumbsList();        //used by tags, might be useful
    int getCurrentRow();
    QString getCurrentFilename();       //not used, but might be handy
    void setThumbParameters();
    void forceScroll(int row);

    QStandardItemModel *thumbViewModel;
    QSortFilterProxyModel *thumbViewFilter;
    QItemSelectionModel *thumbViewSelection;
    QFileInfoList thumbFileInfoList;
    QStringList imageFilePathList;
    QFileInfoList dirFileInfoList;
    QDir::SortFlags thumbsSortFlags;

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

    QString currentViewDir;
    QDir *thumbsDir;
    QStringList *fileFilters;
    QList<QStandardItem*> *thumbList;
    QFileInfo thumbFileInfo;
    QImage emptyImg;
    QWidget *mw;
    Metadata *metadata;
    bool isIconDisplay;
    QSize treeViewSize;

signals:
    void thumbClick(float xPct, float yPct);        //used in ThumbView::mousePressEvent
    void displayLoupe();
    void updateStatus(bool, QString);
};

#endif // THUMBVIEW_H
