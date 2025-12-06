#ifndef FSRUNNER_H
#define FSRUNNER_H

#include <QObject>
#include <QStringList>
#include <opencv2/core.hpp>

#include "FSLoader.h"
#include "FSAlign.h"

class PipelineBase;

class FSRunner : public QObject
{
    Q_OBJECT
public:
    explicit FSRunner(PipelineBase *pipeline, QObject *parent = nullptr);

    // Perform Petteri-style pairwise + stacked alignment
    bool runAlign(const QStringList &paths,
                  const std::vector<QString> &alignedColorPaths,
                  const std::vector<QString> &alignedGrayPaths,
                  bool emitSignals = true);

signals:
    void updateStatus(bool isError, const QString &msg, const QString &src);
    void progress(int current, int total);

private:
    PipelineBase *m_pipeline = nullptr;

    // Helpers
    void emitStatus(bool isError, const QString &msg, const QString &src, bool enabled);
    void emitProgress(int current, int total, bool enabled);
};

#endif // FSRUNNER_H
