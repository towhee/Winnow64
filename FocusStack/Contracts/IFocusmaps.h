#pragma once
#include <QStringList>
#include <QObject>

/*
   IFocusMaps
   ----------
   Input:  aligned image folder
   Output: focusFolderOut (folder containing focus maps)
*/

class IFocusMaps: public QObject
{
    Q_OBJECT

public:
    explicit IFocusMaps(QObject *parent = nullptr)
        : QObject(parent) {}

    virtual ~IFocusMaps() = default;

    virtual QString name() const = 0;

    virtual bool run(const QString &alignedFolder,
                     QString &focusFolderOut) = 0;

signals:
    void updateStatus(bool isError, const QString &msg, const QString &src);
    void updateProgress(int current, int total);
};
