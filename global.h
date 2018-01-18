#ifndef GLOBAL_H
#define GLOBAL_H

//#define ISDEBUG        // Uncomment this line to show debugging output

#include <QtWidgets>
#include <QColor>
#include <QModelIndexList>
#include <QStringList>
//#include <QtGlobal>
#include <QElapsedTimer>

namespace G
{
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
        PickedColumn,
        LabelColumn,
        RatingColumn,
        TypeColumn,
        SizeColumn,
        CreatedColumn,
        ModifiedColumn,
        CreatorColumn,
        MegaPixelsColumn,
        DimensionsColumn,
        ApertureColumn,
        ShutterspeedColumn,
        ISOColumn,
        CameraModelColumn,
        LensColumn,
        FocalLengthColumn,
        TitleColumn,
        CopyrightColumn,
        EmailColumn,
        UrlColumn,
        TotalColumns    // insert additional columns before this
    };

    extern QColor labelNoneColor;
    extern QColor labelRedColor;
    extern QColor labelYellowColor;
    extern QColor labelGreenColor;
    extern QColor labelBlueColor;
    extern QColor labelPurpleColor;

    extern QString mode;
    extern QString lastThumbChangeEvent;

    extern qreal actualDevicePixelRatio;

    extern bool isThreadTrackingOn;
    extern bool isNewFolderLoaded;
    extern int scrollBarThickness;
    extern qreal devicePixelRatio;
    extern QModelIndexList copyCutIdxList;  // req'd?
    extern QStringList copyCutFileList;     // req'd?
    extern QElapsedTimer t;
    extern bool isTimer;
}

#endif // GLOBAL_H

