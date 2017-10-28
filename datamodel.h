#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QtWidgets>
#include "global.h"

class DataModel : public QWidget
{
    Q_OBJECT
public:
    explicit DataModel(QWidget *parent);

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
