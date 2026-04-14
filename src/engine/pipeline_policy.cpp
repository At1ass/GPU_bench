#include "engine/pipeline_policy.h"
#include "engine/render_pass.h"

PassSyncHint PipelinePolicy::syncHint(const RenderPassBase& pass) const {
    PassSyncHint h;
    h.barrier_after = (pass.queueType() == QueueType::Compute);
    return h;
}
