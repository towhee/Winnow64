#ifndef FS_H
#define FS_H

#include <QObject>
#include <QStringList>
#include <QString>
#include <atomic>
#include <functional>
#include <vector>

class FS : public QObject
{
    Q_OBJECT
public:
    struct Options
    {
        bool overwriteExisting = false;
        bool keepIntermediates = true;

        bool enableAlign      = true;
        bool enableFocusMaps  = true;
        bool enableDepthMap   = true;
        bool enableFusion     = true;

        // You can extend these later (contrast, WB flags, etc.)
    };

    explicit FS(QObject *parent = nullptr);

    // Input configuration
    void setInput(const QStringList &paths);            // source paths
    void setProjectRoot(const QString &rootPath);       // folder containing align/focus/depth/fusion
    void setOptions(const Options &opt);

    // Main pipeline API
    bool run();     // synchronous
    void abort();   // request abort (checked inside stages)

signals:
    void updateStatus(bool isError, const QString &message, const QString &src);
    void progress(int current, int total);

private:
    // Folder creation and skip detection
    bool prepareFolders();
    bool detectSkips();

    // Pipeline stages
    bool runAlign();
    bool runFocusMaps();
    bool runDepthMap();
    bool runFusion();

    // helpers for UI
    void emitStageStatus(const QString &stage, const QString &msg, bool isError = false);

    QStringList m_inputPaths;
    QString     m_projectRoot;

    // Stage folders (automatically set from projectRoot)
    QString m_alignFolder;
    QString m_focusFolder;
    QString m_depthFolder;
    QString m_fusionFolder;

    Options m_options;

    // Skip flags
    bool m_skipAlign     = false;
    bool m_skipFocusMaps = false;
    bool m_skipDepthMap  = false;
    bool m_skipFusion    = false;

    std::atomic_bool m_abortRequested = false;

    // --- Alignment output paths (needed for the new FSAlignStage) ---
    std::vector<QString> m_alignedColor;
    std::vector<QString> m_alignedGray;

    // progress
    int m_progress = -1;
    int m_total = 0;
    void incrementProgress();

    // Callback function passed to stages
    std::function<void(const QString &stage)> m_progressCallback;
};

#endif // FS_H
