// FocusStackWorker.cpp
#include "FocusStackWorker.h"

namespace FStack {

FocusStackWorker::FocusStackWorker(const QStringList &paths,
                                   PipelineBase *pipeline,
                                   QObject *parent)
    : QObject(parent),
    m_paths(paths),
    m_pipeline(pipeline)
{
    // Pipeline -> Worker signals
    connect(m_pipeline, &PipelineBase::updateStatus,
            this,        &FocusStackWorker::updateStatus,
            Qt::QueuedConnection);

    connect(m_pipeline, &PipelineBase::updateProgress,
            this,        &FocusStackWorker::updateProgress,
            Qt::QueuedConnection);

    connect(m_pipeline, &PipelineBase::finished,
            this,        &FocusStackWorker::finished,
            Qt::QueuedConnection);
}

void FocusStackWorker::process()
{
    QString output, depth;

    bool ok = m_pipeline->run(m_paths, output, depth);

    if (!ok)
    {
        emit finished(false, "", "");
        return;
    }
}

} // namespace FStack
