#ifndef STACKFUSION_H
#define STACKFUSION_H

#include <QObject>
#include <QImage>
#include <QDir>
#include <QVector>
#include <QtMath>
#include <QDebug>
#include "main/global.h"
#include <opencv2/opencv.hpp>
#include "ImageStack/focusstackutilities.h"
#include "ImageStack/focuswavelet.h"

class StackFusion : public QObject
{
    Q_OBJECT
public:
    enum Method { Naive, PMax, PMax1, PMax2, Petteri };
    Q_ENUM(Method)

    explicit StackFusion(QObject *parent = nullptr);

    bool fuse(Method method,
              const QString &alignedFolderPath,
              const QString &depthMapPath,
              const QString &outputFolderPath);

signals:
    void progress(QString stage, int current, int total);
    void updateStatus(bool keepBase, QString msg, QString src);
    void finished(QString resultPath);

private:
    bool fuseNaive(const QString &depthMapPath,
                   const QString &alignedFolderPath,
                   const QString &outputPath);
    bool fusePMax(const QString &alignedFolderPath, const QString &outputPath);
    bool fusePMax1(const QString &alignedFolderPath, const QString &outputPath);
    bool fusePMax2(const QString &alignedFolderPath, const QString &outputPath);
    bool fusePetteri(const QString &alignedFolderPath,
                     const QString &depthMapPath,
                     const QString &outputPath);
};

#endif // STACKFUSION_H
