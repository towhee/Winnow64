// FocusStackWorker.h
#pragma once
#include <QObject>
#include <QStringList>
#include "Pipeline/PipelineBase.h"

namespace FStack {

class FocusStackWorker : public QObject
{
    Q_OBJECT
public:
    FocusStackWorker(const QStringList &paths,
                     PipelineBase *pipeline,
                     QObject *parent = nullptr);

public slots:
    void process();

signals:
    void updateStatus(bool isError, const QString &msg, const QString &src);
    void updateProgress(int current, int total);
    void finished(bool ok, const QString &output, const QString &depthmap);

private:
    QStringList m_paths;
    PipelineBase *m_pipeline;
};

} // namespace FStack
