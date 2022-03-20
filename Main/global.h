#ifndef GLOBAL_H

#define GLOBAL_H

//#define ISDEBUG        // Uncomment this line to show debugging output
//#define ISFLOW         // Uncomment this line to show debugging output
//#define ISLOGGER       // Uncomment this line to show debugging output

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
#define ICON_MAX	480  // 256 is default
#define EXISTS if(p.file.exists())

namespace G
{
    enum UserRoles {
        PathRole = Qt::UserRole + 1,    // path to image file
        IconRectRole,                   // used in IconView
        CachedRole,                     // used in ImageView
        BusyRole,                       // not used
        DupIsJpgRole,                   // manage raw/jpg pairs
        DupOtherIdxRole,                // manage raw/jpg pairs
        DupHideRawRole,                 // manage raw/jpg pairs
        DupRawTypeRole,                 // manage raw/jpg pairs
        ColumnRole,                     // used by Filters
        GeekRole                        // used in TableView display of columns
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
        CreatedColumn,
        // items read on demand (secondary metadata fields)
        YearColumn,
        DayColumn,
        CreatorColumn,
        MegaPixelsColumn,
        LoadMsecPerMpColumn,
        DimensionsColumn,
        AspectRatioColumn,
        OrientationColumn,
        RotationColumn,
        ApertureColumn,
        ShutterspeedColumn,
        ISOColumn,
        ExposureCompensationColumn,
        CameraMakeColumn,
        CameraModelColumn,
        LensColumn,
        FocalLengthColumn,
        CopyrightColumn,
        TitleColumn,
        EmailColumn,
        UrlColumn,
        MetadataLoadedColumn,
        // original values
        _RatingColumn,
        _LabelColumn,
        _CreatorColumn,
        _TitleColumn,
        _CopyrightColumn,
        _EmailColumn,
        _UrlColumn,
//        _OrientationColumn,
//        _RotationColumn,

        // binary helpers
        OffsetFullColumn,
        LengthFullColumn,
        WidthPreviewColumn,
        HeightPreviewColumn,
        OffsetThumbColumn,
        LengthThumbColumn,
        samplesPerPixelColumn,
        isBigEndianColumn,
        ifd0OffsetColumn,
        XmpSegmentOffsetColumn,
        XmpSegmentLengthColumn,
        IsXMPColumn,
        ICCSegmentOffsetColumn,
        ICCSegmentLengthColumn,
        ICCBufColumn,
        ICCSpaceColumn,

        OrientationOffsetColumn,
        RotationDegreesColumn,
        ShootingInfoColumn,
        SearchTextColumn,
        TotalColumns    // insert additional columns before this
    };

    // Errors
    extern QMap<QString,QStringList> err;

    enum ImageFormat {
        UseQt,
        Jpg,
        Tif,
        Heic
    };

    // used to pass externalApps from MW to AppDlg
    struct Pair {
        QString name;
        QString path;
    };

    struct ImPair {
        int row;
        QImage image;
    };

    extern bool stop;

    extern QSettings *settings;

    extern bool isLogger;
    extern bool isErrorLogger;
    extern bool isFlowLogger;
    extern bool isTestLogger;
    extern bool sendLogToConsole;
    extern QFile logFile;
    extern QFile errlogFile;
    extern bool isDev;

    extern QPoint mousePos;

    extern int availableMemoryMB;
    extern int winnowMemoryBeforeCacheMB;
    extern int metaCacheMB;

    struct WinScreen {
        QString adaptor;
        QString device;
        QString profile;
    };
    extern QHash<QString, WinScreen> winScreenHash;
    extern QString winOutProfilePath;
    extern int displayPhysicalHorizontalPixels; // current monitor
    extern int displayPhysicalVerticalPixels;   // current monitor
    extern int displayVirtualHorizontalPixels;  // current monitor
    extern int displayVirtualVerticalPixels;    // current monitor
    extern qreal actDevicePixelRatio;           // current monitor
    extern qreal sysDevicePixelRatio;           // current monitor

    extern int maxIconSize;
    extern int minIconSize;
    extern int iconWMax;
    extern int iconHMax;

    extern QColor textColor;
    extern QColor backgroundColor;
    extern QColor borderColor;
    extern QColor disabledColor;
    extern QColor tabWidgetBorderColor;
    extern QColor pushButtonBackgroundColor;
    extern QColor scrollBarHandleBackgroundColor;
    extern QColor selectionColor;
    extern QColor mouseOverColor;

    extern QColor labelNoneColor;
    extern QColor labelRedColor;
    extern QColor labelYellowColor;
    extern QColor labelGreenColor;
    extern QColor labelBlueColor;
    extern QColor labelPurpleColor;

    extern QStringList ratings;
    extern QStringList labelColors;

    extern double iconOpacity;

    extern QString mode;
    extern QString fileSelectionChangeSource;

    extern QString fontSize;                    // in points
    extern qreal dpi;                           // current screen dots per inch
    extern qreal ptToPx;
    extern int textShade;
    extern int backgroundShade;
    extern QString css;

    extern bool allMetadataLoaded;
//    extern bool buildingFilters;
    extern bool ignoreScrollSignal;
    extern bool isSlideShow;
    extern bool isProcessingExportedImages;
    extern bool isRunningColorAnalysis;
    extern bool isRunningStackOperation;
    extern bool isEmbellish;
    extern bool colorManage;
    extern bool modifySourceFiles;
    extern bool useSidecar;
    extern bool embedTifThumb;

    extern bool isRunningBackgroundIngest;
    extern int ingestCount;
    extern QDate ingestLastSeqDate;

    extern bool isThreadTrackingOn;
    extern bool showAllTableColumns;
    extern bool isNewFolderLoaded;
    extern bool isNewFolderLoadedAndInfoViewUpToDate;
    extern bool isInitializing;
    extern bool isNewSelection;
    extern int scrollBarThickness;
    extern int propertyWidgetMarginLeft;
    extern int propertyWidgetMarginRight;
//    extern qreal actDevicePixelRatio;
    extern QModelIndexList copyCutIdxList;  // req'd?
    extern QStringList copyCutFileList;     // req'd?

    extern QString tiffData;

    extern QElapsedTimer t;
    extern QElapsedTimer t1;
    extern bool isTimer;
    extern bool isTest;

    extern void track(QString functionName = "", QString comment = "", bool hideTime = false);
    extern QStringList doNotLog;
    extern void log(QString functionName = "", QString comment = "",
                    bool zeroElapsedTime = false);
    extern void errlog(QString functionName, QString fPath, QString err);
    extern void error(QString functionName, QString fPath, QString err);
    extern void wait(int ms);
    extern QString s(QVariant x);
    extern QString sj(QString s, int x);

    extern int popUpProgressCount;
    extern int popUpLoadFolderStep;
    extern PopUp *popUp;
    extern void newPopUp(QWidget *widget, QWidget *centralWidget);
}
#endif // GLOBAL_H

