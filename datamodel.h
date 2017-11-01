#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QtWidgets>
#include "metadata.h"
#include "filters.h"
#include "global.h"

//namespace DM
//{
//    enum UserRoles {
//        FileNameRole = Qt::UserRole + 1,
//        SortRole,
//        LoadedRole,
//        PickedRole,
//        ThumbRectRole,
//        PathRole,
//        FileTypeRole,
//        FileSizeRole,
//        CreatedRole,
//        ModifiedRole,
//        LabelRole,
//        RatingRole,
//        ColumnRole
//    };

//    enum dataModelColumns {
//        PathColumn,
//        NameColumn,
//        TypeColumn,
//        SizeColumn,
//        CreatedColumn,
//        ModifiedColumn,
//        PickedColumn,
//        LabelColumn,
//        RatingColumn,
//        MegaPixelsColumn,
//        DimensionsColumn,
//        ApertureColumn,
//        ShutterspeedColumn,
//        ISOColumn,
//        CameraModelColumn,
//        FocalLengthColumn,
//        TitleColumn,
//        TotalColumns    // insert additional columns before this
//    };
//}   // end namespace

class SortFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    SortFilter(QObject *parent, Filters *filters);

public slots:
    void filterChanged(QTreeWidgetItem* x, int col);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    Filters *filters;
};

class DataModel : public QStandardItemModel
{
    Q_OBJECT
public:
    DataModel(QWidget *parent, Metadata *metadata, Filters *filters);

    bool load(QString &dir, bool includeSubfolders);
    void addMetadataToModel();
    void updateImageList();
    void sortThumbs(int sortColumn, bool isReverse);

    SortFilter *sf; //ThumbViewFilter *thumbViewFilter;
//    ThumbViewFilter *thumbViewFilterTest;
    QItemSelectionModel *thumbViewSelection;
    QFileInfoList thumbFileInfoList;
    QStringList imageFilePathList;
    QFileInfoList dirFileInfoList;
    QDir::SortFlags thumbsSortFlags;
    QString currentFolderPath;

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
        RatingRole,
        ColumnRole
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

signals:

public slots:

private:
    QWidget *mw;
    Metadata *metadata;
    Filters *filters;

    QDir *dir;
    QStringList *fileFilters;
    QList<QStandardItem*> *thumbList;
    QFileInfo thumbFileInfo;
    QImage emptyImg;

    bool addFiles();

};

#endif // DATAMODEL_H
