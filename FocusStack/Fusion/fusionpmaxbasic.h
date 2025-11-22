#ifndef FUSIONPMAXBASIC_H
#define FUSIONPMAXBASIC_H

#include "fusionbase.h"

class FusionPMaxBasic : public FusionBase
{
    Q_OBJECT
public:
    explicit FusionPMaxBasic(QObject *parent = nullptr);

    QString name() const override { return "FusionPMaxBasic"; }

    bool fuse(const QStringList &imagePaths,
              const QString &projDir,
              const Options &options) override;

private:
    // These helpers will call into the Petteri-based pipeline, using paths.
    // bool runAlignment(const QStringList &imagePaths, const QString &alignedFolder);
    // bool runFocusMaps(const QString &alignedFolder, QString &focusFolder);
    // bool runDepthMap(const QString &alignedFolder,
    //                  const QString &focusFolder,
    //                  QString &depthFolder);
    // bool runFusion(const QString &alignedFolder,
    //                const QString &depthFolder,
    //                const QString &outputFolder,
    //                QString &outputImagePath);
};

#endif // FUSIONPMAXBASIC_H
