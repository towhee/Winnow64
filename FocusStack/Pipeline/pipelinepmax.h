#ifndef PIPELINEPMAX_H
#define PIPELINEPMAX_H

#include "FocusStack/Pipeline/PipelineBase.h"

#include <atomic>

#include "PetteriModular/Wrappers/PetteriAlignWorker.h"
#include "PetteriModular/Wrappers/PetteriFocusMapsWorker.h"
#include "PetteriModular/Wrappers/PetteriDepthMapWorker.h"
#include "PetteriModular/Wrappers/PetteriPMaxFusionWorker.h"

class PipelinePMax : public PipelineBase
{
    Q_OBJECT
public:
    explicit PipelinePMax(QObject *parent = nullptr);

    // pipelineName is fixed to "PMax" in the implementation
    void setInput(const QStringList &paths, bool isRedo);

    // synchronous run
    void run();

    // request abort (checked between stages and optionally inside workers)
    void abort();

private:
    bool abortRequested() const { return m_abort.load(std::memory_order_relaxed); }

    bool runAlignment();
    bool runFocusMaps();
    bool runDepthMap();
    bool runFusion();

private:
    std::atomic<bool> m_abort { false };
};

#endif // PIPELINEPMAX_H
