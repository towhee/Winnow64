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
        // QString methodMerge             = "";
        QString methodInfo              = "";
        bool isLocal                    = true;
        bool saveDiagnostics            = true;
        bool enableOpenCL               = true;
        bool writeFusedBackToSource     = false;    // false for debugging
        bool removeTemp                 = false;
    } o;

    QString dstFolderPath; // original source for input images (parent if lightroom)

    QString statusGroupPrefix;
    QString statusRunPrefix;
    QList<QStringList> groups;

    // Input configuration
    void initialize();
    bool setOptions(const Options &opt);

    void diagnostics();
    void requestAbort()
    {
        abort.store(true, std::memory_order_relaxed);
    }

    // Main pipeline API
    bool run();             // run all testing deltas

signals:
    void updateStatus(bool isError, const QString &message, const QString &src);
    void progress(int current, int total);
    void finished(bool success);

protected:
    bool abortRequested() const
    {
        return abort.load(std::memory_order_relaxed);
    }

private:
    std::atomic_bool abort{false};

    bool initializeGroup(int group);            // source groups
    bool prepareFolders();
    bool validAlignMatsAvailable(int count) const;
    QImage thumbnail(const cv::Mat &mat);

    // Pipeline stages
    bool runDMap();
    bool runPMax();
    bool save(QString fuseFolderPath);
    bool cleanup();

    // helpers for UI
    void status(const QString &msg);

    QStringList inputPaths;
    QString     grpFolderPath;
    QStringList grpFolderPaths;
    int slices = 0;
    int lastSlice = 0;

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
    int progressCount = -1;
    int progressTotal = 0;
    void incrementProgress();
    void initializeProgress();
};

#endif // FS_H
