#ifndef PIPELINEBASE_H
#define PIPELINEBASE_H

#include <QObject>
#include <QString>
#include <QStringList>

// Base class that manages folder layout and deterministic naming.
// Derived pipelines (PMax, DMapOnly, etc.) never modify the layout –
// they only *read* directory paths and *write* stage output paths.

class PipelineBase : public QObject
{
    Q_OBJECT
public:
    explicit PipelineBase(QObject *parent = nullptr);

    // Initialize from source images and pipeline name (“PMax”, etc.)
    void setInput(const QStringList &paths,
                  const QString &pipelineName,
                  bool isRedo);

    void clean();

    // Public Accessors (READ-ONLY)
    const QStringList &sourcePaths()      const { return m_sourcePaths; }
    const QStringList &alignedPaths()     const { return m_alignedPaths; }
    const QStringList &alignedGrayPaths() const { return m_alignedGrayPaths; }
    const QStringList &focusMapPaths()    const { return m_focusMapPaths; }

    const QString &srcFolder()            const { return m_srcFolder; }
    const QString &srcBase()              const { return m_srcBase; }
    const QString &srcExt()               const { return m_srcExt; }
    const QString &pipelineName()         const { return m_pipelineName; }
    const QString &projectFolder()        const { return m_projFolderPath; }

    const QString &alignDir()             const { return m_alignDir; }
    const QString &grayDir()              const { return m_grayDir; }
    const QString &focusDir()             const { return m_focusDir; }
    const QString &depthDir()             const { return m_depthDir; }
    const QString &fusionDir()            const { return m_fusionDir; }

    const QString &depthRawPath()         const { return m_depthRawPath; }
    const QString &depthFilteredPath()    const { return m_depthFilteredPath; }
    const QString &fusionOutputPath()     const { return m_fusionOutputPath; }

    int m_total = 0;            // total number of tasks for progress notification
    int m_progressCount = -1;

    QString msg;                // for general use

signals:
    void updateStatus(bool isError, const QString &msg, const QString &src);
    void progress(int current, int total);
    void finished(bool ok, const QString &outputFolder);

protected:
    //
    // Methods used by derived pipelines (PMax, etc.)
    //
    void prepareProjectStructure();
    void prepareAlignmentPaths();
    void prepareFocusMapPaths();
    void prepareDepthPaths();
    void prepareFusionPath();

    bool detectExistingAligned();

    void incrementProgress();

    static QString uniqueBaseName(const QString &folder,
                                  const QString &base,
                                  const QString &ext);

    //
    // PROTECTED: Derived pipelines may READ and WRITE these results.
    // But they must NOT modify directory layout or naming rules.
    //
    QStringList m_sourcePaths;
    QString     m_pipelineName;

    // Stage results - set by derived pipeline stages
    QStringList m_alignedPaths;
    QStringList m_alignedGrayPaths;
    QStringList m_focusMapPaths;
    QString     m_srcFolder;
    QString     m_srcBase;
    QString     m_projFolderPath;
    QString     m_depthRawPath;
    QString     m_depthFilteredPath;
    QString     m_fusionOutputPath;
    QString     m_alignDir;
    QString     m_grayDir;
    QString     m_focusDir;
    QString     m_depthDir;
    QString     m_fusionDir;
    QString     m_srcExt;

    bool m_skipAlign = false;

private:
    //
    // PRIVATE: internal project structure – never modified by derived classes.
    // Derived classes get read-only access via getters.
    //

};

#endif // PIPELINEBASE_H
