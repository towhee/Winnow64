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
// #include "Main/logger.h"
#include "Log/issue.h"
#include "Log/log.h"

#define ICON_MIN	40
#define ICON_MAX	480  // 256 is default
#define EXISTS if (p.file.exists())

//#define "CLASSFUNCTION" QString::fromUtf8(metaObject()->className()) + "::" + __func__

namespace G
{
    enum UserRoles {
        PathRole = Qt::UserRole + 1,    // path to image file
        IconRectRole,                   // used in IconView
        // CachedRole,                     // used in ImageView, IconViewDelegate
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
        RowNumberColumn,
        NameColumn,
        FolderNameColumn,
        MSToReadColumn,
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
        MetadataReadingColumn,
        MetadataAttemptedColumn,
        MetadataLoadedColumn,
        IconLoadedColumn,
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
        ifdOffsetsColumn,
        XmpSegmentOffsetColumn,
        XmpSegmentLengthColumn,
        IsXMPColumn,
        ICCSegmentOffsetColumn,
        ICCSegmentLengthColumn,
        ICCBufColumn,
        ICCSpaceColumn,

        // image cache helpers
        CacheSizeColumn,
        IsCachingColumn,
        IsCachedColumn,
        AttemptsColumn,
        DecoderIdColumn,
        DecoderReturnStatusColumn,
        DecoderErrMsgColumn,

        OrientationOffsetColumn,
        RotationDegreesColumn,
        ShootingInfoColumn,
        SearchTextColumn,
        ErrColumn,
        TotalColumns    // insert additional columns before this
    };

    // Errors
    extern QStringList issueList;

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

    // mutex
    extern QWaitCondition waitCondition;
    extern QMutex gMutex;

    extern QThread* guiThread;

    // flow
    extern bool stop;
    extern bool removingFolderFromDM;
    extern bool removingRowsFromDM;
    extern bool isInitializing;

    // datamodel
    extern bool allMetadataLoaded;
    extern bool iconChunkLoaded;

    extern int dmInstance;

    extern bool useMyTiff;
    extern bool useMissingThumbs;

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
    extern bool useReadMeta;
    extern bool useFSTreeCount;
    extern bool useProcessEvents;

    extern QSettings *settings;

    extern bool isTestLogger;
    extern bool isLogger;
    extern bool isFlowLogger;
    extern bool isFlowLogger2;
    extern bool showIssueInConsole;
    extern bool isFileLogger;
    extern bool isErrorLogger;
    extern bool isIssueLogger;
    extern bool sendLogToConsole;
    extern bool showAllEvents;
    extern QFile logFile;
    extern QFile issueLogFile;
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

    extern QString trash;

    extern int maxIconSize;
    extern int minIconSize;
    extern int maxIconChunk;

    extern QColor textColor;
    extern QColor backgroundColor;
    extern QColor borderColor;
    extern QColor disabledColor;
    extern QColor tabWidgetBorderColor;
    extern QColor pushButtonBackgroundColor;
    extern QColor scrollBarHandleBackgroundColor;
    extern QColor helpColor;
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
    extern bool wheelSpinning;

    extern QString mode;
    extern QString fileSelectionChangeSource;
    extern bool autoAdvance;

    extern int fontSize;                        // in pixels
    extern QString strFontSize;                 // in pixels
    extern qreal dpi;                           // current screen dots per inch
    extern qreal ptToPx;
    extern int textShade;
    extern int backgroundShade;
    extern QString css;

    extern bool isModifyingDatamodel;
    extern bool ignoreScrollSignal;
    extern bool resizingIcons;
    extern bool isSlideShow;
    extern bool isRunningColorAnalysis;
    extern bool isRunningStackOperation;
    extern bool isProcessingExportedImages;
    extern bool isEmbellish;
    extern bool includeSidecars;
    extern bool colorManage;
    extern bool modifySourceFiles;
    extern bool backupBeforeModifying;
    extern bool autoAddMissingThumbnails;
    extern bool useSidecar;
    extern bool renderVideoThumb;
    extern bool isFilter;

    // training
    extern bool isTraining;

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
    extern bool isStressTest;

    // Signal relay class for global communication
    class SignalRelay : public QObject {
        Q_OBJECT
    public:
        explicit SignalRelay(QObject *parent = nullptr) : QObject(parent) {}

    signals:
        void updateStatus(bool keepBase, QString s, QString source);
        void showPopUp(QString msg, int duration, bool isAutosize,
                       float opacity, Qt::Alignment alignment);
    };

    extern SignalRelay *relay;

    void setDM(QObject *dm);

    extern Logger logger;
    extern void track(QString functionName = "", QString comment = "", bool hideTime = false);
    extern void log(QString functionName = "",
                    QString comment = "",
                    bool zeroElapsedTime = false);
    extern IssueLog *issueLog;
    extern void newIssueLog();
    extern QMutex issueListMutex;
    extern void issue(QString type, QString msg = "", QString src = "",
                      int sfRow = -1, QString fPath = "");

    extern int wait(int ms);
    extern QString s(QVariant x);
    extern QString sj(QString s, int x);
    extern bool instanceClash(int instance, QString src);

    extern bool isGuiThread();

    extern int popUpProgressCount;
    extern int popUpLoadFolderStep;
    extern PopUp *popUp;
    extern void newPopUp(QWidget *widget, QWidget *centralWidget);
}
#endif // GLOBAL_H

