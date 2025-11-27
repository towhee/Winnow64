#pragma once

#include "FocusStack/Contracts/IFocusMaps.h"
#include <QStringList>

class PetteriFocusMaps : public IFocusMaps
{
    Q_OBJECT
public:
    explicit PetteriFocusMaps(QObject *parent = nullptr);

    QString name() const override { return "PetteriFocusMaps"; }

    // alignedFolder: folder containing aligned images (e.g. ".../align")
    // focusFolderOut: will be set to output folder (e.g. ".../focus")
    bool run(const QString &alignedFolder,
             QString &focusFolderOut) override;

signals:
    // isError, message, source
    void updateStatus(bool isBase, const QString &msg, const QString &src);
    void updateProgress(int current, int total);
};
