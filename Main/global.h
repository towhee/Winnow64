#ifndef GLOBAL_H

#define GLOBAL_H

#include <QtWidgets>
#include <QtConcurrent/QtConcurrent>
#include <QColor>
#include <QModelIndexList>
#include <QStringList>
#include <QElapsedTimer>
#include <iostream>
#include <iomanip>
#include <QMutex>
#include "popup.h"
#include "Utilities/utilities.h"

// METAREAD or METAREAD2: one or the other:
#ifdef Q_OS_WIN
#define METAREAD2
#endif
#ifdef Q_OS_MAC
#define METAREAD2
#endif

#define ICON_MIN	40
#define ICON_MAX	480  // 256 is default
#define EXISTS if (p.file.exists())

//#define "CLASSFUNCTION" QString::fromUtf8(metaObject()->className()) + "::" + __func__
//#define OK !currRootFolder.isEmpty()

namespace G
{
    enum UserRoles {
        PathRole = Qt::UserRole + 1,    // path to image file
        IconRectRole,                   // used in IconView
        CachedRole,                     // used in ImageView, IconViewDelegate
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
        PickColumn,
        IngestedColumn,
        LabelColumn,
        RatingColumn,
        SearchColumn,
        TypeColumn,
        VideoColumn,
        ApertureColumn,
        ShutterspeedColumn,
        ISOColumn,
        ExposureCompensationColumn,
        DurationColumn,
        CameraMakeColumn,
        CameraModelColumn,
        LensColumn,
        FocalLengthColumn,
        FocusXColumn,
        FocusYColumn,
        GPSCoordColumn,
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

        CopyrightColumn,
        TitleColumn,
        EmailColumn,
        UrlColumn,
        KeywordsColumn,
        MetadataLoadedColumn,
        MissingThumbColumn,
        CompareColumn,
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
        PermissionsColumn,
        ReadWriteColumn,

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
        //LoadErrColumn,
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

    extern enum ShowProgress {
        None,
        MetaCache,
        ImageCache
    } showProgress;

    // used to pass externalApps from MW to AppDlg
    struct App {
        QString name;
        QString path;
        QString args;
    };

    // flow
    extern bool stop;
    extern bool dmEmpty;
    extern bool isLinearLoadDone;
    extern bool isInitializing;
    extern bool metaReadDone;
    extern bool allMetadataLoaded;
    extern bool allIconsLoaded;

    extern int dmInstance;

    // limit functionality for testing
    extern bool useApplicationStateChanged;
    extern bool useZoomWindow;
    extern bool useImageCache;
    extern bool useImageView;
    extern bool useInfoView;
    extern bool useMultimedia;
    extern bool useUpdateStatus ;
    extern bool useFilterView;      // not finished
    extern bool useReadIcons;
    extern bool useReadMetadata;
    extern bool useProcessEvents;
    extern QString metaReadInUse;;

    extern QSettings *settings;

    extern bool isTestLogger;
    extern bool isLogger;
    extern bool isFlowLogger;
    extern bool isFlowLogger2;
    extern bool isWarningLogger;
    extern bool isFileLogger;
    extern bool isErrorLogger;
    extern bool sendLogToConsole;
    extern QFile logFile;
    extern QFile errlogFile;
    extern bool isDev;
    extern bool isRemote;

    // Rory version
    extern bool isRory;


    extern bool loadOnlyVisibleIcons;
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

    extern QColor textColor;
    extern QColor backgroundColor;
    extern QColor borderColor;
    extern QColor disabledColor;
    extern QColor tabWidgetBorderColor;
    extern QColor pushButtonBackgroundColor;
    extern QColor scrollBarHandleBackgroundColor;
    extern QColor selectionColor;
    extern QColor mouseOverColor;
    extern QColor appleBlue;

    extern QColor labelNoneColor;
    extern QColor labelRedColor;
    extern QColor labelYellowColor;
    extern QColor labelGreenColor;
    extern QColor labelBlueColor;
    extern QColor labelPurpleColor;

    extern QStringList ratings;
    extern QStringList labelColors;

    extern double iconOpacity;

    extern int wheelSensitivity;

    extern QString mode;
    extern QString fileSelectionChangeSource;

    extern int fontSize;                        // in pixels
    extern QString strFontSize;                 // in pixels
    extern qreal dpi;                           // current screen dots per inch
    extern qreal ptToPx;
    extern int textShade;
    extern int backgroundShade;
    extern QString css;

    extern QString currRootFolder;
    extern bool ignoreScrollSignal;
    extern bool isSlideShow;
    extern bool isRunningColorAnalysis;
    extern bool isRunningStackOperation;
    extern bool isProcessingExportedImages;
    extern bool isEmbellish;
    extern bool colorManage;
    extern bool modifySourceFiles;
    extern bool backupBeforeModifying;
    extern bool autoAddMissingThumbnails;
    extern bool useSidecar;
//    extern bool embedTifJpgThumb;
    extern bool isLoadLinear;
    extern bool isLoadConcurrent;
    extern bool renderVideoThumb;
    extern bool includeSubfolders;
    extern bool isFilter;

    extern bool isRunningBackgroundIngest;
    extern int ingestCount;
    extern QDate ingestLastSeqDate;

    // copying files
    extern bool isCopyingFiles;
    extern bool stopCopyingFiles;

    extern bool isThreadTrackingOn;
    extern bool showAllTableColumns;

    extern int scrollBarThickness;
    extern int propertyWidgetMarginLeft;
    extern int propertyWidgetMarginRight;
    extern QModelIndexList copyCutIdxList;  // req'd?
    extern QStringList copyCutFileList;     // req'd?

    extern QString tiffData;

    extern QElapsedTimer t;
    extern QElapsedTimer t1;
    extern bool isTimer;
    extern bool isTest;

//    extern QList<int>rowsWithIcon;

//    extern bool empty();
    extern void track(QString functionName = "", QString comment = "", bool hideTime = false);
    extern QStringList doNotLog;
    extern void log(QString functionName = "",
                    QString comment = "",
                    bool zeroElapsedTime = false);
    extern void errlog(QString err, QString functionName, QString fPath = "");
    extern void error(QString err, QString functionName, QString fPath = "");
    extern int wait(int ms);
    extern QString s(QVariant x);
    extern QString sj(QString s, int x);
    extern bool instanceClash(int instance, QString src);

    extern int popUpProgressCount;
    extern int popUpLoadFolderStep;
    extern PopUp *popUp;
    extern void newPopUp(QWidget *widget, QWidget *centralWidget);
}
#endif // GLOBAL_H

