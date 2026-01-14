#ifndef FS_H
#define FS_H

#include "Metadata/ExifTool.h"
#include "Metadata/xmp.h"

#include "QtCore/qdebug.h"
#include <QObject>
#include <QStringList>
#include <QString>
#include <atomic>
#include <vector>

#include <opencv2/core.hpp>

class FS : public QObject
{
    Q_OBJECT
public:
    /*
    */

    explicit FS(QObject *parent = nullptr);

    enum Methods {
        StreamPMax,        // PMax1, but streamed
        PMax,              // align, fuse using multiscale wavelets
        TennengradVersions // multiple depth maps for diff radius/thresholds
    };
    static inline const QStringList MethodsString {
        "StreamPMax",        // PMax1, but streamed
        "PMax",              // align, fuse using multiscale wavelets
        "TennengradVersions" // multiple depth maps for diff radius/thresholds
    };

    struct Options
    {
        QString method          = "";
        QString methodAlign     = "";
        QString methodFocus     = "";
        QString methodDepth     = "";
        QString methodFuse      = "";

        bool isStream           = false;
        bool isLocal            = true;

        bool useIntermediates   = false;
        bool useCache           = true;     // use disk if false
        bool enableOpenCL       = true;

        bool enableAlign        = true;
        bool keepAlign          = true;     // intermediates

        bool enableFocusMaps    = true;
        bool previewFocusMaps   = true;
        bool keepFocusMaps      = true;

        bool enableDepthMap     = true;
        bool previewDepthMap    = true;
        bool keepDepthMap       = true;

        bool enableFusion       = true;
        bool previewFusion      = true;

        bool enableBackgroundMask       = true;
        bool enableBackgroundReplace    = true;
        bool previewBackgroundMask      = true;
        QString backgroundMethod        = "Depth+Focus";
        int backgroundSourceIndex       = -1;      // -1 = lastSlice (macro default)

        bool enableArtifactDetect       = true;
        bool enableArtifactRepair       = true;
        QString artifactMethod          = "MultiScale";
    };

    QString statusGroupPrefix;
    // Groups
    QList<QStringList> groups;

    // Input configuration
    void initialize(QString dstFolderPath, QString dstFusedPath);
    void setOptions(const Options &opt);

    void diagnostics();

    void requestAbort()
    {
        abort.store(true, std::memory_order_relaxed);
    }

    // Main pipeline API
    bool run();     // synchronous

signals:
    void updateStatus(bool isError, const QString &message, const QString &src);
    void progress(int current, int total);
    void finished(bool success);

protected:
    bool abortRequested() const
    {
        // qDebug() << "FS::abortRequested";
        return abort.load(std::memory_order_relaxed);
    }

private:
    // std::atomic_bool abortRequested;
    std::atomic_bool abort{false};

    bool initializeGroup(int group);            // source groups
    bool prepareFolders();
    void updateIntermediateStatus();
    bool validAlignMatsAvailable(int count) const;
    void previewOverview(cv::Mat &fusedColor8Mat);
    QImage thumbnail(const cv::Mat &mat);

    // Pipeline stages
    bool runAlign();
    bool runFocusMaps();
    bool runDepthMap();
    bool runFusion();
    bool runBackground();
    bool runArtifact();
    bool runStreamWaveletPMax();
    bool runStreamTennengradVersions();
    bool save(QString fuseFolderPath);

    // helpers for UI
    void status(const QString &msg);

    QStringList inputPaths;
    QString     grpFolderPath;
    QString     dstFolderPath; // original source for input images (parent if lightroom)
    QString     fusedBase; // assigned in MW:generateFocusStack
    int slices = 0;
    int lastSlice = 0;

    // Stage folders (automatically set from projectRoot)
    QString alignFolderPath;
    QString focusFolderPath;
    QString depthFolderPath;
    QString backgroundFolderPath;
    QString fusionFolderPath;
    QString artifactsFolderPath;

    Options o;

    // Skip flags
    bool skipAlign     = false;
    bool skipFocusMaps = false;
    bool skipDepthMap  = false;
    bool skipFusion    = false;
    bool skipArtifacts = false;

    // Exist flags
    bool alignExists  = true;
    bool focusExists  = true;
    bool depthExists  = true;
    bool fusionExists = true;

    // Alignment output paths
    std::vector<QString> alignedColorPaths;     // intermediate
    std::vector<QString> alignedGrayPaths;      // intermediate
    void setAlignedColorPaths();
    // Depth output path
    QString depthIdxPath;                       // intermediate
    QString lastFusedPath;                      // intermediate

    // In-memory aligned images (optional fast path for runFusion)
    std::vector<cv::Mat> alignedColorSlices;
    std::vector<cv::Mat> alignedGraySlices;
    void setAlignedColorSlices();
    void setAlignedGraySlices();

    // Focus maps and depth map
    std::vector<cv::Mat> focusSlices;           // CV_32F per slice
    cv::Mat              depthIndex16Mat;       // CV_16U depth indices

    // Background mask (computed in runBackground, applied in runFusion)
    cv::Mat bgConfidence01Mat;   // CV_32F 0..1
    cv::Mat subjectMask8Mat;     // CV_8U  0/255

    // Fusion Mat
    cv::Mat fusedColorMat;       // CU8 or CU16

    // Progress
    int progressCount = -1;
    int progressTotal = 0;
    void incrementProgress();
    void setTotalProgress();
};

#endif // FS_H
