#include "demo/passes/auto_exposure_pass.h"
#include "engine/pass_context.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "renderer/features.h"
#include "platform/logger.h"

void AutoExposurePass::init(const TierResourceView& res) {
    ub_histogram_.init(res.t4.exposure.histogram_shader);
    ub_exposure_.init(res.t4.exposure.exposure_shader);
}

void AutoExposurePass::execute(PassContext& ctx, FrameData& fd,
                               const TierResourceView& res,
                               const DemoTierConfig& cfg,
                               const SceneData& scene) {
    Renderer* r = ctx.renderer();
    (void)cfg;
    (void)scene;

    if (!res.t4.exposure.histogram_shader || !res.t4.exposure.exposure_shader) return;
    if (res.t4.exposure.histogram_ssbo == INVALID_BUFFER || res.t4.exposure.exposure_ssbo == INVALID_BUFFER) return;

    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!cf) return;

    float min_log_lum = -8.0f;
    float log_lum_range = 12.0f;

    // Step 1: Build histogram
    ub_histogram_.use();
    r->bindRenderTargetTexture(res.t4.hdr.scene_rt, 0);
    ub_histogram_.set(U::SceneTex, 0);
    ub_histogram_.set(U::ScreenSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));
    ub_histogram_.set(U::MinLogLum, min_log_lum);
    ub_histogram_.set(U::LogLumRange, log_lum_range);

    cf->bindSSBO(res.t4.exposure.histogram_ssbo, 1);

    int gx = (fd.viewport_w + 15) / 16;
    int gy = (fd.viewport_h + 15) / 16;
    cf->dispatchCompute(gx, gy, 1);
    cf->computeMemoryBarrier();

    // Step 2: Reduce histogram to exposure value
    ub_exposure_.use();
    cf->bindSSBO(res.t4.exposure.histogram_ssbo, 1);
    cf->bindSSBO(res.t4.exposure.exposure_ssbo, 2);
    ub_exposure_.set(U::MinLogLum, min_log_lum);
    ub_exposure_.set(U::LogLumRange, log_lum_range);
    ub_exposure_.set(U::TotalPixels, static_cast<float>(fd.viewport_w * fd.viewport_h));
    ub_exposure_.set(U::AdaptSpeed, 2.0f);
    ub_exposure_.set(U::Dt, 1.0f / 60.0f);

    cf->dispatchCompute(1, 1, 1);
    cf->computeMemoryBarrier();
}
