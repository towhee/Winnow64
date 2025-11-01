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

    // --- Configuration ---
    void setSourceFolder(const QString &path);
    void setInputFiles(const QStringList &filePaths);

    // --- Run pipeline stages ---
    bool runAlignment(bool saveAligned = true);

signals:
    void progress(QString stage, int current, int total);
    void finished(QString resultPath);
    void updateStatus(bool keepBase, QString msg, QString src);

private:
    QDir srcFolder;
    QDir projectFolder;
    QDir alignedFolder;
    QDir focusMapFolder;
    QDir depthMapFolder;
    QDir fusedFolder;

    QStringList inputFiles;
    QString projectBaseName;

    bool prepareProjectFolders();
    QMap<int, QImage> loadInputImages();
};

#endif // STACKCONTROLLER_H
