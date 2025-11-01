#ifndef STACKCONTROLLER_H
#define STACKCONTROLLER_H

#include <QObject>
#include <QDir>
#include <QImage>
#include <QMap>
#include <QStringList>
#include <QElapsedTimer>
#include <QDebug>
#include "ImageStack/stackaligner.h"

class StackController : public QObject
{
    Q_OBJECT
public:
    explicit StackController(QObject *parent = nullptr);

    void stop();
    void loadInputImages(const QStringList &paths, const QList<QImage*> &images);
    bool runAlignment(bool saveAligned = true, bool useGpu = false);
    void test();

signals:
    void progress(QString stage, int current, int total);
    void finished(QString resultPath);
    void updateStatus(bool keepBase, QString msg, QString src);

private:
    void prepareProjectFolders();

    QStringList inputPaths;
    QList<QImage*> inputImages;

    QString projectRoot;
    QString projectFolder;
    QDir alignedFolder;


    std::atomic<bool> isAborted{false};
    QThread *workerThread = nullptr;
};

#endif // STACKCONTROLLER_H
