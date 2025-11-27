#pragma once
#include <QString>
#include <QStringList>
#include <QObject>

/*
   IFusion
   -------
   Input:  alignedFolder + depthFolder
   Output: outputImagePath and optional generatedMaskPath
*/

class IFusion : public QObject
{
    Q_OBJECT

public:
    explicit IFusion(QObject *parent = nullptr)
        : QObject(parent) {}


    virtual ~IFusion() = default;

    virtual QString name() const = 0;

    virtual bool run(const QString &alignedFolder,
                     const QString &depthFolder,
                     const QString &outputFolder,
                     QString &outputImagePath,
                     QString &generatedMaskPath) = 0;

signals:
    void updateStatus(bool isError, const QString &msg, const QString &src);
    void updateProgress(int current, int total);
};
