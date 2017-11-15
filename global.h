
#ifndef GLOBAL_H
#define GLOBAL_H

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

    extern QColor labelNoneColor;
    extern QColor labelRedColor;
    extern QColor labelYellowColor;
    extern QColor labelGreenColor;
    extern QColor labelBlueColor;
    extern QColor labelPurpleColor;

    extern bool isThreadTrackingOn;
    extern bool isNewFolderLoaded;
    extern qreal devicePixelRatio;
    extern QModelIndexList copyCutIdxList;  // req'd?
    extern QStringList copyCutFileList;     // req'd?
    extern QElapsedTimer t;
    extern QString appName;                 // may be req'd for mult workspaces + be persistent
    extern bool isTimer;
}

#define ISDEBUG        // Uncomment this line to show debugging output

#endif // GLOBAL_H

