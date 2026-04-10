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
        DMap,            // DMap, streamed
        PMax             // PMax1, but streamed
    };
    static inline const QStringList MethodsString {
        "DMap",
        "PMax",
     };

    struct Options
    {
        QString method                  = "DMap";
        QString methodInfo              = "";
        bool isLocal                    = true;
        bool enableOpenCL               = true;
        bool writeFusedBackToSource     = true;    // false for debugging
        bool removeTemp                 = false;
        bool isDebugging                = true;
        bool saveDiagnostics            = true;
    } o;

    QString srcFolderPath; // original source for input images (parent if lightroom)

    QString statusGroupPrefix;
    QString statusRunPrefix = "Focus Stacking:  ";
    QList<QStringList> groups;

    // Input configuration
    bool setOptions(const Options &opt);

    void diagnostics();
    void requestAbort() { abort.store(true, std::memory_order_relaxed); }

    // Main pipeline API
    void run();             // run all testing deltas

signals:
    void updateStatus(bool isError, const QString &message, const QString &src);
    void progress(int current, int total);
    void requestImage(QString fPath, cv::Mat &mat);
    void finished(bool success);

protected:
    bool abortRequested() const { return abort.load(std::memory_order_relaxed); }

private:
    struct WarpResult {
        int slice;
        cv::Mat gray;
        cv::Mat color;
    };

    std::atomic_bool abort{false};

    bool initializeGroup(int group);            // source groups
    bool prepareFolders();
    bool validAlignMatsAvailable(int count) const;
    QImage thumbnail(const cv::Mat &mat);

    // Pipeline stages
    bool runDMap();
    bool runPMax();
    QString save(QString fuseFolderPath);
    bool cleanup();

    // helpers for UI
    void status(const QString &msg);

    QStringList inputPaths;
    QString     grpFolderPath;
    QStringList grpFolderPaths;
    int grpSlices = 0;
    int grpLastSlice = 0;
    int totSlices = 0;

    // Stage folders (automatically set from projectRoot)
    QString alignFolderPath;
    QString depthFolderPath;
    QString fusionFolderPath;

    // Options o;

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

    // Depth map
    cv::Mat depthIndex16Mat;        // CV_16U depth indices

    // Fusion Mat
    cv::Mat fusedColorMat;          // CU8 or CU16

    // Progress
    int sliceCount = 0;
    int msAveSlice = 0;
    int progressCount = -1;
    int progressTotal = 0;
    QElapsedTimer t;
    int msToGo;
    void incrementProgress();
    void initializeProgress();
};

#endif // FS_H
