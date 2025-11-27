#pragma once

#include "FocusStack/Contracts/IAlign.h"

// Forward declaration of backend namespace/class
namespace FStack {
class FocusStack;
}

/*
   PetteriAlign
   ------------
   Thin stage wrapper around the PetteriModular FocusStack engine
   configured in "align-only" mode.

   Implements IAlign (Qt-based interface).
*/

class PetteriAlign : public IAlign
{
    Q_OBJECT

public:
    explicit PetteriAlign(QObject *parent = nullptr);

    QString name() const override;

    bool run(const QStringList &imagePaths,
             QString &alignedFolderOut) override;
};
