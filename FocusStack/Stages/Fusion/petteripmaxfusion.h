#pragma once

#include "FocusStack/Contracts/IFusion.h"

namespace FStack {
class FocusStack;
}

class PetteriPMaxFusion : public IFusion
{
    Q_OBJECT

public:
    explicit PetteriPMaxFusion(QObject *parent = nullptr);

    QString name() const override;

    bool run(const QString &alignedFolder,
             const QString &depthFolder,
             const QString &outputFolder,
             QString &outputImagePath,
             QString &generatedMaskPath) override;
};
