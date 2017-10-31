#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QtWidgets>
#include "metadata.h"
#include "filters.h"
#include "global.h"

class DataModel : public QStandardItemModel
{
    Q_OBJECT
public:
    explicit DataModel(QWidget *parent, Metadata *metadata, Filters *filters);

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

signals:

public slots:
};

#endif // DATAMODEL_H
