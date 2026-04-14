#include "demo/passes/bloom_compute_pass.h"
#include "engine/pass_context.h"
#include "demo/demo_utils.h"
#include "engine/uniform_id.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/scene/demo_scene.h"
#include "renderer/features.h"
#include "platform/logger.h"

void BloomComputePass::init(const TierResourceView& res, const DemoTierConfig& cfg,
                            const DemoDebugOverrides& dbg) {
    res_ = &res;
    cfg_ = &cfg;
    dbg_ = &dbg;
    ub_down_.init(res.shader(ShaderBank::BloomDownT4));
    ub_up_.init(res.shader(ShaderBank::BloomUpT4));
}

bool BloomComputePass::isEnabled() const {
    return cfg_ && dbg_ && cfg_->enable_compute_bloom && !dbg_->skip_compute_bloom;
}

void BloomComputePass::execute(PassContext& ctx, FrameData& fd, const SceneData& scene) {
    Renderer* r = ctx.renderer();
    const TierResourceView& res = *res_;
    (void)scene;

    if (!res.shader(ShaderBank::BloomDownT4) || !res.shader(ShaderBank::BloomUpT4)) return;

    GL4Features* g4 = r->features<GL4Features>();
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!g4 || !cf) return;

    static const int MIP_COUNT = TierResourceView::T4::ComputeBloom::MIP_COUNT;

    // Verify all mips exist
    for (int i = 0; i < MIP_COUNT; i++) {
        if (res.t4.bloom.mips[i] == INVALID_TEXTURE) return;
    }

    // --- Downsample chain ---
    ub_down_.use();

    for (int i = 0; i < MIP_COUNT; i++) {
        // Source: either HDR scene texture (level 0) or previous mip
        if (i == 0) {
            r->bindRenderTargetTexture(res.t4.hdr.scene_rt, 0);
            ub_down_.set(U::SrcSize,
                static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));
            ub_down_.set(U::FirstPass, 1); // Karis average
            ub_down_.set(U::BloomThreshold, 1.5f);
        } else {
            r->bindTextureUnit(0, res.t4.bloom.mips[i - 1]);
            int prev_w = fd.viewport_w >> i;
            int prev_h = fd.viewport_h >> i;
            if (prev_w < 1) prev_w = 1;
            if (prev_h < 1) prev_h = 1;
            ub_down_.set(U::SrcSize,
                static_cast<float>(prev_w), static_cast<float>(prev_h));
            ub_down_.set(U::FirstPass, 0);
        }
        ub_down_.set(U::SrcTex, 0);

        g4->bindImageTexture(res.t4.bloom.mips[i], 0, false, true); // write-only

        int mw = fd.viewport_w >> (i + 1);
        int mh = fd.viewport_h >> (i + 1);
        if (mw < 1) mw = 1;
        if (mh < 1) mh = 1;

        int gx = (mw + 15) / 16;
        int gy = (mh + 15) / 16;
        cf->dispatchCompute(gx, gy, 1);
        g4->imageMemoryBarrier();
    }

    // --- Upsample chain (bottom-up, additive) ---
    ub_up_.use();
    ub_up_.set(U::BloomRadius, 1.0f);

    for (int i = MIP_COUNT - 2; i >= 0; i--) {
        // Source: lower (smaller) mip
        r->bindTextureUnit(0, res.t4.bloom.mips[i + 1]);
        ub_up_.set(U::SrcTex, 0);

        // Destination: current mip (read-write for additive blend)
        g4->bindImageTexture(res.t4.bloom.mips[i], 0, true, true);

        int mw = fd.viewport_w >> (i + 1);
        int mh = fd.viewport_h >> (i + 1);
        if (mw < 1) mw = 1;
        if (mh < 1) mh = 1;

        int gx = (mw + 15) / 16;
        int gy = (mh + 15) / 16;
        cf->dispatchCompute(gx, gy, 1);
        g4->imageMemoryBarrier();
    }
}
