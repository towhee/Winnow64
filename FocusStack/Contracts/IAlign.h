#pragma once
#include <QStringList>
#include <QObject>

/*
   IAlign
   ------
   Input:  raw source image paths
   Output: alignedFolderOut (folder that now contains aligned images)
*/

class IAlign: public QObject
{
    Q_OBJECT

public:
    explicit IAlign(QObject *parent = nullptr)
        : QObject(parent) {}

    virtual ~IAlign() = default;

    virtual QString name() const = 0;

    virtual bool run(const QStringList &imagePaths,
                     QString &alignedFolderOut) = 0;

signals:
    void updateStatus(bool isError, const QString &msg, const QString &src);
    void updateProgress(int current, int total);
};
