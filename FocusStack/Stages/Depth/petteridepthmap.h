#pragma once

#include "FocusStack/Contracts/IDepthMap.h"

namespace FStack {
class FocusStack;
}

class PetteriDepthMap : public IDepthMap
{
    Q_OBJECT

public:
    explicit PetteriDepthMap(QObject *parent = nullptr);
    QString name() const override;

    bool run(const QString &alignedFolder,
             const QString &focusFolder,
             QString &depthFolderOut) override;
};
