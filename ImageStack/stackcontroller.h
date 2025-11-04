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
#include "ImageStack/stackaligner.h"
#include "ImageStack/focusmeasure.h"
#include "ImageStack/depthmap.h"
#include "ImageStack/stackfusion.h"

class StackController : public QObject
{
    Q_OBJECT
public:
    explicit StackController(QObject *parent = nullptr);

    void stop();
    void loadInputImages(const QStringList &paths, const QList<QImage*> &images);
    bool runAlignment(bool saveAligned = true, bool useGpu = false);
    bool runFocusMaps(const QString &alignedFolderPath);
    bool runDepthMap(const QString &alignedFolderPath);
    bool runFusion();
    void test();

signals:
    void progress(QString stage, int current, int total);
    void finished(QString resultPath);
    void finishedFocus(QString projectFolder);
    void finishedDepthMap(QString projectFolder);
    void finishedFusion(QString resultPath);
    void updateStatus(bool keepBase, QString msg, QString src);

public slots:
    // void computeFocusMaps(const QString &projectFolder);

private:
    void initialize();

    QStringList inputPaths;
    QList<QImage*>inputImages;

    QString projectRoot;
    QString projectFolder;

    QString alignedPath;
    QString masksPath;
    QString depthPath;
    QString depthFocusPath;
    QString outputPath;

    QDir projectDir;
    QDir alignedDir;
    QDir masksDir;
    QDir depthDir;
    QDir depthFocusDir;
    QDir outputDir;

    std::atomic<bool> isAborted{false};
    QThread *workerThread = nullptr;
};

#endif // STACKCONTROLLER_H
