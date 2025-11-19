#ifndef STACKCONTROLLER_H
#define STACKCONTROLLER_H

#include <QObject>
#include <QDir>
#include <QDirIterator>
#include <QImage>
#include <QMap>
#include <QStringList>
#include <QElapsedTimer>
#include <QDebug>
#include <QThread>
#include "ImageStack/focusstackconstants.h"
#include "ImageStack/stackaligner.h"
#include "ImageStack/stack_focus_measure.h"
#include "ImageStack/stack_depth_map.h"
#include "ImageStack/stackfusion.h"
#include "ImageStack/focushalo.h"

class StackController : public QObject
{
    Q_OBJECT
public:
    explicit StackController(QObject *parent = nullptr);

    void stop();
    void loadInputImages(const QStringList &paths, const QList<QImage*> &images);
    bool runAlignment(bool saveAligned = true, bool useGpu = false);
    bool runFocusMaps(const QString &focusFolderPath);
    bool runDepthMap(const QString &alignedFolderPath);
    bool runFusion();
    bool runHaloReduction(const QString &imagePath);
    void test();

    void setPreset(const QString &presetName);
    QString currentPresetName() const { return m_currentPreset; }

signals:
    void progress(QString stage, int current, int total);
    void finishedAlign(QString resultPath);
    void finishedFocus(QString projectFolder);
    void finishedDepthMap(bool success);
    void finishedFusion(QString resultPath);
    void finishedHaloReduction(QString resultPath);
    void updateStatus(bool keepBase, QString msg, QString src);

public slots:
    // void computeFocusMaps(const QString &projectFolder);

private:
    void initialize();

    QString m_currentPreset;
    FSConst::FocusPreset  m_focusPreset;
    FSConst::DepthPreset  m_depthPreset;
    FSConst::FusionPreset m_fusionPreset;

    QStringList inputPaths;
    QList<QImage*>inputImages;

    QString projectRoot;
    QString projectFolder;

    QString alignedPath;
    QString focusPath;
    QString depthPath;
    QString depthMapPath;
    QString depthFocusPath;
    QString haloPath;
    QString outputPath;

    QDir projectDir;
    QDir alignedDir;
    QDir focusDir;
    QDir depthDir;
    QDir depthFocusDir;
    QDir haloDir;
    QDir outputDir;

    float haloStrength = 0;
    int   haloRadius = 1;

    bool daisyChain = true;

    std::atomic<bool> isAborted{false};
    QThread *workerThread = nullptr;
};

#endif // STACKCONTROLLER_H
