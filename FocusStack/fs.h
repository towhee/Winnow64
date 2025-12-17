#ifndef FS_H
#define FS_H

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
        enable      Run the stage
        preview     Make preview files
        overwrite   Overwrite previews
    */
    struct Options
    {
        QString method          = "";
        bool keepIntermediates  = true;
        bool useIntermediates   = true;
        bool useCache           = true;     // use disk if false

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

        bool enableArtifactDetect = true;
        bool enableArtifactRepair = true;
        QString artifactMethod    = "MultiScale";

        bool enableOpenCL       = true;
    };

    explicit FS(QObject *parent = nullptr);

    // Input configuration
    void setInput(const QStringList &paths);            // source paths
    void setProjectRoot(const QString &rootPath);       // folder containing align/focus/depth/fusion
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

protected:
    bool abortRequested() const
    {
        // qDebug() << "FS::abortRequested";
        return abort.load(std::memory_order_relaxed);
    }

private:
    // std::atomic_bool abortRequested;
    std::atomic_bool abort{false};

    bool prepareFolders();
    void updateIntermediateStatus();
    bool setParameters();
    bool validAlignMatsAvailable(int count) const;
    void previewOverview(cv::Mat &fusedColor8Mat);

    // Pipeline stages
    bool runAlign();
    bool runFocusMaps();
    bool runDepthMap();
    bool runFusion();
    bool runArtifact();

    // helpers for UI
    void status(const QString &msg);

    QStringList inputPaths;
    QString     projectRoot;
    int slices = 0;
    int lastSlice = 0;

    // Stage folders (automatically set from projectRoot)
    QString alignFolder;
    QString focusFolder;
    QString depthFolder;
    QString fusionFolder;
    QString artifactsFolder;

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

    // Fusion Mat
    cv::Mat fusedColor8Mat;

    // Progress
    int progressCount = -1;
    int progressTotal = 0;
    void incrementProgress();
    void setTotalProgress();
};

#endif // FS_H
