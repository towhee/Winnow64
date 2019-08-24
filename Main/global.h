#ifndef GLOBAL_H
#define GLOBAL_H

//#define ISDEBUG        // Uncomment this line to show debugging output
//#define ISPROFILE      // Uncomment this line to show profiling output
//#define ISTEST         // Uncomment this line to show testing output

#define CHAR static_cast<char>
#define INT static_cast<int>
#define UINT static_cast<uint>
#define LONG static_cast<long>
#define FLOAT static_cast<float>
#define DOUBLE static_cast<double>

#define TRACK

#include <QtWidgets>
#include <QColor>
#include <QModelIndexList>
#include <QStringList>
#include <QElapsedTimer>
#include <iostream>
#include <iomanip>
#include <QMutex>
#include "popup.h"

#define ICON_MIN	40
#define ICON_MAX	480  // 256

namespace G
{
    enum UserRoles {
        PathRole = Qt::UserRole + 1,
        ThumbRectRole,
        CachedRole,
        DupIsJpgRole,
        DupRawIdxRole,
        DupHideRawRole,
        DupRawTypeRole,
        ColumnRole,
        GeekRole
    };

    /*
    dataModel setHorizontalHeaderItem in DataModel::DataModel must include all prior enum but
    not in the same order!
    */
    enum dataModelColumns {
        // items available for TableView in order
        // items read when new folder (core fields)
        PathColumn,
        NameColumn,
        RefineColumn,
        PickColumn,
        IngestedColumn,
        LabelColumn,
        RatingColumn,
        SearchColumn,
        TypeColumn,
        SizeColumn,
        WidthColumn,
        HeightColumn,
        ModifiedColumn,
        // items read on demand (secondary metadata fields)
        CreatedColumn,
        YearColumn,
        DayColumn,
        CreatorColumn,
        MegaPixelsColumn,
        DimensionsColumn,
        RotationColumn,
        ApertureColumn,
        ShutterspeedColumn,
        ISOColumn,
        CameraMakeColumn,
        CameraModelColumn,
        LensColumn,
        FocalLengthColumn,
        CopyrightColumn,
        TitleColumn,
        EmailColumn,
        UrlColumn,
        // unavailable for TableView from here on (move items above to show and below to hide)
        MetadataLoadedColumn,
        // original values
        _RatingColumn,
        _LabelColumn,
        _CreatorColumn,
        _TitleColumn,
        _CopyrightColumn,
        _EmailColumn,
        _UrlColumn,

        // binary helpers
        OffsetFullJPGColumn,
        LengthFullJPGColumn,
        OffsetThumbJPGColumn,
        LengthThumbJPGColumn,
        OffsetSmallJPGColumn,
        LengthSmallJPGColumn,
        XmpSegmentOffsetColumn,
        XmpNextSegmentOffsetColumn,
        IsXMPColumn,
        OrientationOffsetColumn,
        OrientationColumn,
        RotationDegreesColumn,
        ShootingInfoColumn,
        ErrColumn,
        SearchTextColumn,
        TotalColumns    // insert additional columns before this
    };

    struct Pair {
        QString name;
        QString path;
    };

    struct ImPair {
        int row;
        QImage image;
    };

    extern int maxIconSize;
    extern int minIconSize;
    extern int iconWMax;
    extern int iconHMax;

    extern QColor labelNoneColor;
    extern QColor labelRedColor;
    extern QColor labelYellowColor;
    extern QColor labelGreenColor;
    extern QColor labelBlueColor;
    extern QColor labelPurpleColor;

    extern QStringList ratings;
    extern QStringList labelColors;

    extern QColor progressCurrentColor;
    extern QColor progressBgColor;
    extern QColor progressAddFileInfoColor;
    extern QColor progressAddMetadataColor;
    extern QColor progressBuildFiltersColor;
    extern QColor progressTargetColor;
    extern QColor progressImageCacheColor;

    extern QString mode;
    extern QString fileSelectionChangeSource;

    extern QString fontSize;

    extern int actualDevicePixelRatio;
    extern bool allMetadataLoaded;
    extern bool buildingFilters;
    extern bool ignoreScrollSignal;
    extern bool isSlideShow;

    extern bool isThreadTrackingOn;
    extern bool showAllTableColumns;
    extern bool isNewFolderLoaded;
    extern bool isInitializing;
    extern int scrollBarThickness;
    extern int propertyWidgetMarginLeft;
    extern int propertyWidgetMarginRight;
//    extern qreal devicePixelRatio;
    extern QModelIndexList copyCutIdxList;  // req'd?
    extern QStringList copyCutFileList;     // req'd?
    extern QElapsedTimer t;
    extern bool isTimer;

    extern void track(QString functionName = "", QString comment = "");
    extern void wait(int ms);
    extern QString s(QVariant x);

    extern PopUp *popUp;
    extern void newPopUp(QWidget *widget);
}
#endif // GLOBAL_H

