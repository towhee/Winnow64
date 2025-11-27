#pragma once
#include <QString>
#include <QObject>

/*
   IDepthMap
   ---------
   Input:  alignedFolder + focusFolder
   Output: depthFolderOut containing depth_index.png
*/

class IDepthMap: public QObject
{
    Q_OBJECT

public:
    explicit IDepthMap(QObject *parent = nullptr)
        : QObject(parent) {}

    virtual ~IDepthMap() = default;

    virtual QString name() const = 0;

    virtual bool run(const QString &alignedFolder,
                     const QString &focusFolder,
                     QString &depthFolderOut) = 0;

signals:
    void updateStatus(bool isError, const QString &msg, const QString &src);
    void updateProgress(int current, int total);
};
