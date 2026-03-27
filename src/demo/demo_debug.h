#pragma once

// Runtime debug overrides for demo render pipeline.
// Allows disabling individual T4 features without recompilation.
// Default values match the previous hardcoded #define behavior.

struct DemoDebugOverrides {
    bool skip_hdr;           // skip entire HDR pipeline (use T3 path)
    bool skip_auto_exposure; // skip auto-exposure compute
    bool skip_gtao;          // skip compute GTAO (use fragment SSAO)
    bool skip_vol_fog;       // skip volumetric fog
    bool skip_ssr;           // skip SSR compute
    bool skip_compute_bloom; // skip compute bloom (use fragment bloom)
    bool skip_dof;           // skip depth of field

    DemoDebugOverrides()
        : skip_hdr(false)
        , skip_auto_exposure(false)
        , skip_gtao(false)
        , skip_vol_fog(false)
        , skip_ssr(false)      // not checked by any pass, kept for future use
        , skip_compute_bloom(false)
        , skip_dof(false) {}
};
