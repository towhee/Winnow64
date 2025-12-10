#ifndef FS_H
#define FS_H

#include <QObject>
#include <QStringList>
#include <QString>
#include <atomic>
#include <functional>
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
        QString method         = "";
        bool overwriteExisting  = false;
        bool keepIntermediates  = true;

        bool enableAlign        = true;
        bool previewAlign       = true;
        bool overwriteAlign     = true;
        bool enableFocusMaps    = true;
        bool previewFocusMaps   = true;
        bool overwriteFocusMaps = true;
        bool enableDepthMap     = true;
        bool previewDepthMap    = true;
        bool overwriteDepthMap  = true;
        bool enableFusion       = true;
        bool previewFusion      = true;
        bool overWriteFusion    = true;

        bool enableOpenCL       = true;

        // You can extend these later (contrast, WB flags, etc.)
    };

    explicit FS(QObject *parent = nullptr);

    // Input configuration
    void setInput(const QStringList &paths);            // source paths
    void setProjectRoot(const QString &rootPath);       // folder containing align/focus/depth/fusion
    void setOptions(const Options &opt);
    void diagnostics();

    // Main pipeline API
    bool run();     // synchronous
    void abort();   // request abort (checked inside stages)

signals:
    void updateStatus(bool isError, const QString &message, const QString &src);
    void progress(int current, int total);

private:
    std::atomic_bool abortRequested = false;

    // Folder creation and skip detection
    bool prepareFolders();
    void setExistance();
    bool setParameters();
    bool validAlignMatsAvailable(int count) const;

    // Pipeline stages
    bool runAlign();
    bool runFocusMaps();
    bool runDepthMap();
    bool runFusion();

    // helpers for UI
    void status(const QString &msg);

    QStringList inputPaths;
    QString     projectRoot;

    // Stage folders (automatically set from projectRoot)
    QString alignFolder;
    QString focusFolder;
    QString depthFolder;
    QString fusionFolder;

    Options o;

    // Skip flags
    bool skipAlign     = false;
    bool skipFocusMaps = false;
    bool skipDepthMap  = false;
    bool skipFusion    = false;

    // Exist flags
    // bool alignGrayscaleExists = false;
    // bool alignColorExists     = false;
    bool alignExists          = false;
    bool focusMapsExist       = false;
    bool depthMapExists       = false;
    bool fusionExists         = false;

    // Alignment output paths
    std::vector<QString> alignedColorPaths;
    std::vector<QString> alignedGrayPaths;

    // In-memory aligned images (optional fast path for runFusion)
    std::vector<cv::Mat> alignedColorMats;
    std::vector<cv::Mat> alignedGrayMats;

    // Focus maps and depth map
    std::vector<cv::Mat> focusMaps;         // CV_32F per slice
    cv::Mat              depthIndex16Mat;   // CV_16U depth indices

    // Progress
    int progressCount = -1;
    int progressTotal = 0;
    void incrementProgress();
    void setTotalProgress();
};

#endif // FS_H
