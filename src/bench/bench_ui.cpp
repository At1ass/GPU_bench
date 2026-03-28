#include "bench/bench_ui.h"
#include "renderer/render_context.h"
#include "renderer/renderer.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>

static constexpr int MIN_WARMUP_FRAMES  = 30;
static constexpr int MIN_MEASURE_FRAMES = 60;

UIAction BenchUI::render(RenderContext* ctx, const UIView& view, UIState& state) {
    UIAction action;
    ctx->imguiNewFrame();

    float panel_h = static_cast<float>(view.window_h) * 0.55f;
    ImGui::SetNextWindowPos(ImVec2(0, static_cast<float>(view.window_h) - panel_h));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(view.window_w), panel_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    ImGui::Begin("##panel", nullptr, flags);

    drawHardwareInfo(view);
    ImGui::Separator();

    if (!view.bench_active) {
        drawPresetSelector(view, state, action);
        drawResolutionSelector(view, state, action);
        drawCustomParams(state, action);

        // Clamp minimums after custom edits
        if (state.current_preset->warmup_frames < MIN_WARMUP_FRAMES)
            state.current_preset->warmup_frames = MIN_WARMUP_FRAMES;
        if (state.current_preset->measure_frames < MIN_MEASURE_FRAMES)
            state.current_preset->measure_frames = MIN_MEASURE_FRAMES;

        ImGui::Separator();
        drawTestSelector(view, state);
        drawControls(view, action);
    } else {
        ImGui::Text("Status: %s", view.bench_status->c_str());
        ImGui::ProgressBar(static_cast<float>(view.bench_progress) / 100.0f, ImVec2(-1.0f, 0.0f));
    }

    ImGui::Separator();
    drawCompositeScore(view);
    drawBottleneck(view);
    drawResultsTable(view);

    ImGui::End();
    ImGui::Render();
    ctx->imguiRender();

    return action;
}

// --- Hardware info & capabilities ---

void BenchUI::drawHardwareInfo(const UIView& view) {
    const RenderCaps& caps = *view.caps;
    float w = static_cast<float>(view.window_w);
    bool wide = (view.window_w >= 720);

    // Row 1: CPU + GPU (two-column if wide, stacked if narrow)
    ImGui::Text("CPU: %s", view.hw_info->cpu_name.c_str());
    if (wide) ImGui::SameLine(w * 0.5f);
    ImGui::Text("GPU: %s", view.gpu_renderer);

    // Row 2: OS + GL/VRAM
    ImGui::Text("OS: %s %s", view.hw_info->os_name.c_str(), view.hw_info->os_version.c_str());
    if (wide) ImGui::SameLine(w * 0.5f);
    if (caps.estimated_vram_mb > 0) {
        ImGui::Text("GL: %s  Renderer: %s  VRAM: %d MB",
                     view.gl_version, view.renderer_name, caps.estimated_vram_mb);
    } else {
        ImGui::Text("GL: %s  Renderer: %s", view.gl_version, view.renderer_name);
    }

    // Row 3: Caps + Tier
    ImGui::Text("MaxTex: %d  Attribs: %d", caps.max_texture_size, caps.max_vertex_attribs);
    ImGui::SameLine();
    auto capLabel = [](const char* name, bool supported) {
        if (supported)
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", name);
        else
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", name);
    };
    capLabel("VAO", (view.available_caps & Cap_VAO) != 0);
    ImGui::SameLine(); capLabel("Instancing", (view.available_caps & Cap_Instancing) != 0);
    ImGui::SameLine(); capLabel("FBO", (view.available_caps & Cap_FBO) != 0);
    ImGui::SameLine(); capLabel("TimerQ", (view.available_caps & Cap_TimerQuery) != 0);

    // GPU tier
    ImGui::SameLine();
    ImVec4 tier_colors[] = {
        ImVec4(0.7f, 0.5f, 0.3f, 1.0f),  // legacy
        ImVec4(0.8f, 0.8f, 0.3f, 1.0f),  // low
        ImVec4(0.3f, 0.8f, 0.3f, 1.0f),  // mid
        ImVec4(0.3f, 0.6f, 1.0f, 1.0f),  // high
        ImVec4(0.8f, 0.3f, 1.0f, 1.0f),  // ultra
    };
    int ti = static_cast<int>(view.gpu_tier);
    if (ti < 0 || ti > 4) ti = 2;
    ImGui::TextColored(tier_colors[ti], "Tier: %s", gpuTierName(view.gpu_tier));

    // Row 4: Resolution
    ImGui::Text("Window: %dx%d   Render: %dx%d%s",
                view.window_w, view.window_h,
                view.render_w, view.render_h,
                (view.render_w == view.window_w && view.render_h == view.window_h)
                    ? " (native)" : "");
}

// --- Preset buttons ---

void BenchUI::drawPresetSelector(const UIView& view, UIState& state, UIAction& out) {
    ImGui::Text("Preset:");
    ImGui::SameLine();

    const char* preset_labels[] = {"Light", "Medium", "Heavy", "Ultra", "Extreme"};
    const int preset_count = 5;

    for (int i = 0; i < preset_count; i++) {
        if (i > 0) ImGui::SameLine();
        char label[64];
        if (i == view.suggested_preset)
            snprintf(label, sizeof(label), "%s*", preset_labels[i]);
        else
            snprintf(label, sizeof(label), "%s", preset_labels[i]);

        bool is_selected = (*state.selected_preset_index == i);
        if (is_selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 0.3f, 1.0f));

        if (ImGui::Button(label, ImVec2(80, 0))) {
            out.type = UIAction::ApplyPreset;
            out.preset_index = i;
        }

        if (is_selected)
            ImGui::PopStyleColor();
    }

    if (view.suggested_preset >= 0) {
        ImGui::SameLine();
        ImGui::TextDisabled("(* = recommended)");
    }

    if (*state.selected_preset_index < 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[Custom]");
    }

    if (!state.validation_error->empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s",
                           state.validation_error->c_str());
    }
}

// --- Render resolution combo ---

void BenchUI::drawResolutionSelector(const UIView& view, UIState& state, UIAction& out) {
    static constexpr int ResNative = -1;

    ImGui::Text("Render:");
    ImGui::SameLine();

    // Native button
    {
        bool is_native = (*state.selected_resolution == ResNative);
        if (is_native)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 0.3f, 1.0f));

        char native_label[64];
        snprintf(native_label, sizeof(native_label), "Native (%dx%d)",
                 view.window_w, view.window_h);
        if (ImGui::Button(native_label)) {
            *state.selected_resolution = ResNative;
            out.type = UIAction::ResolutionChanged;
        }
        if (is_native)
            ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(140);

    // Build combo label
    char combo_label[64];
    if (*state.selected_resolution >= 0 && *state.selected_resolution < view.num_resolutions)
        snprintf(combo_label, sizeof(combo_label), "%s",
                 view.resolutions[*state.selected_resolution].label);
    else
        snprintf(combo_label, sizeof(combo_label), "Select...");

    const char* preview = (*state.selected_resolution == ResNative) ? "---" : combo_label;
    if (ImGui::BeginCombo("##res", preview)) {
        for (int i = 0; i < view.num_resolutions; i++) {
            bool is_sel = (*state.selected_resolution == i);
            bool needs_fbo = (view.resolutions[i].w > view.window_w
                           || view.resolutions[i].h > view.window_h);
            char item_label[64];
            if (needs_fbo && !view.supports_render_targets)
                snprintf(item_label, sizeof(item_label), "%s (no FBO)",
                         view.resolutions[i].label);
            else if (needs_fbo)
                snprintf(item_label, sizeof(item_label), "%s (FBO)",
                         view.resolutions[i].label);
            else
                snprintf(item_label, sizeof(item_label), "%s",
                         view.resolutions[i].label);

            bool disabled = needs_fbo && !view.supports_render_targets;
            if (disabled) ImGui::BeginDisabled();

            if (ImGui::Selectable(item_label, is_sel)) {
                *state.selected_resolution = i;
                out.type = UIAction::ResolutionChanged;
            }
            if (is_sel) ImGui::SetItemDefaultFocus();

            if (disabled) ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
}

// --- Custom parameter inputs ---

void BenchUI::drawCustomParams(UIState& state, UIAction& out) {
    if (!ImGui::TreeNode("Custom Parameters"))
        return;

    bool changed = false;
    BenchPreset& p = *state.current_preset;

    ImGui::Text("Timing:"); ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("Warmup##w", &p.warmup_frames, 10, 50);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("Measure##m", &p.measure_frames, 50, 200);

    ImGui::Separator();
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("Fillrate layers", &p.fillrate.layers, 50, 200);
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("Geometry grid", &p.geometry.grid_size, 5, 10);
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("Texturing tex size", &p.texturing.tex_size, 256, 512);
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("Texturing layers", &p.texturing.layers, 50, 100);
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("DrawCall draws", &p.drawcall.draws_per_frame, 100, 500);
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("Overdraw layers", &p.overdraw.layers, 50, 100);
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("TexUpload size", &p.texupload.tex_size, 128, 256);
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("StateChange switches", &p.statechange.switches, 50, 100);
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("Vertex count", &p.vertex.vertex_count, 10000, 100000);
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("ShaderALU iters", &p.shader_alu.iterations, 10, 50);
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("ShaderFMA iters", &p.shader_fma.iterations, 10, 50);
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("Instanced instances", &p.instanced_draw.instance_count, 500, 5000);
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("Compute iters", &p.compute_fma.iterations, 10, 50);
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputInt("Compute groups", &p.compute_fma.work_groups, 64, 256);

    if (changed)
        out.type = UIAction::CustomParamsChanged;

    ImGui::TreePop();
}

// --- Test selection checkboxes ---

void BenchUI::drawTestSelector(const UIView& view, UIState& state) {
    ImGui::Text("Tests:");
    uint32_t avail_caps = view.available_caps;
    float region_w = ImGui::GetContentRegionAvail().x;

    for (int i = 0; i < NUM_TESTS; i++) {
        // Flow layout: SameLine if the next checkbox fits, otherwise wrap
        if (i > 0) {
            // Check if placing next item on same line would overflow
            float last_x = ImGui::GetItemRectMax().x;
            float item_w = ImGui::CalcTextSize(g_tests[i].display_name).x + 30.0f;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            if (last_x + spacing + item_w < region_w + ImGui::GetWindowPos().x)
                ImGui::SameLine();
        }

        bool supported = (g_tests[i].required_caps & avail_caps) == g_tests[i].required_caps;
        if (!supported) {
            ImGui::BeginDisabled();
            bool disabled_val = false;
            ImGui::Checkbox(g_tests[i].display_name, &disabled_val);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s\n[Requires unsupported GL capabilities]",
                                  g_tests[i].description);
        } else {
            ImGui::Checkbox(g_tests[i].display_name, &state.test_enabled[i]);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", g_tests[i].description);
        }
    }
}

// --- Run / Clear / Export buttons ---

void BenchUI::drawControls(const UIView& view, UIAction& out) {
    bool can_run = !view.bench_active;
    if (view.validation_error && !view.validation_error->empty()) can_run = false;

    if (!can_run) ImGui::BeginDisabled();
    if (ImGui::Button("Run Selected", ImVec2(110, 0)))
        out.type = UIAction::RunSelected;
    ImGui::SameLine();
    if (ImGui::Button("Run All", ImVec2(80, 0)))
        out.type = UIAction::RunAll;
    if (!can_run) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Clear", ImVec2(60, 0)))
        out.type = UIAction::Clear;

    ImGui::SameLine();
    if (!view.results->empty() && ImGui::Button("Export", ImVec2(60, 0)))
        out.type = UIAction::Export;
}

// --- Composite score display ---

void BenchUI::drawCompositeScore(const UIView& view) {
    const auto& results = *view.results;
    const auto& composite = *view.composite;
    if (results.empty() || composite.overall <= 0) return;

    int cats_present = (composite.fill > 0 ? 1 : 0)
                     + (composite.geometry > 0 ? 1 : 0)
                     + (composite.compute > 0 ? 1 : 0)
                     + (composite.overhead > 0 ? 1 : 0);

    ImVec4 color = (cats_present < 4)
        ? ImVec4(0.8f, 0.8f, 0.4f, 1.0f)
        : ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
    ImGui::TextColored(color,
        "Composite: %.1f  |  Fill: %.1f  Geometry: %.1f  Compute: %.1f  Overhead: %.1f",
        composite.overall, composite.fill,
        composite.geometry, composite.compute, composite.overhead);
    if (cats_present < 4) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f),
                           " (partial -- run all for full score)");
    }
}

// --- Bottleneck analysis ---

void BenchUI::drawBottleneck(const UIView& view) {
    const auto& results = *view.results;
    const auto& bottleneck = *view.bottleneck;
    if (results.empty() || bottleneck.detail.empty()) return;

    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f),
                       "Bottleneck: %s", bottleneck.detail.c_str());
}

// --- Results table ---

void BenchUI::drawResultsTable(const UIView& view) {
    const auto& results = *view.results;
    if (results.empty()) return;

    if (!ImGui::BeginTable("results", 10,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
            | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY))
        return;

    ImGui::TableSetupColumn("Test",       ImGuiTableColumnFlags_None, 1.2f);
    ImGui::TableSetupColumn("Score",      ImGuiTableColumnFlags_None, 1.0f);
    ImGui::TableSetupColumn("Unit",       ImGuiTableColumnFlags_None, 0.8f);
    ImGui::TableSetupColumn("Avg (ms)",   ImGuiTableColumnFlags_None, 0.8f);
    ImGui::TableSetupColumn("Min (ms)",   ImGuiTableColumnFlags_None, 0.8f);
    ImGui::TableSetupColumn("Max (ms)",   ImGuiTableColumnFlags_None, 0.8f);
    ImGui::TableSetupColumn("P1 (ms)",    ImGuiTableColumnFlags_None, 0.8f);
    ImGui::TableSetupColumn("Median (ms)",ImGuiTableColumnFlags_None, 0.8f);
    ImGui::TableSetupColumn("P99 (ms)",   ImGuiTableColumnFlags_None, 0.8f);
    ImGui::TableSetupColumn("CV%",        ImGuiTableColumnFlags_None, 0.6f);
    ImGui::TableHeadersRow();

    for (size_t i = 0; i < results.size(); i++) {
        const BenchResult& r = results[i];
        ImGui::TableNextRow();

        if (!r.valid)
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                ImGui::GetColorU32(ImVec4(0.4f, 0.1f, 0.1f, 0.5f)));

        ImGui::TableSetColumnIndex(0);
        if (!r.sanity_ok) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s [!]", r.name.c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Sanity check failed: render output was black");
        } else {
            ImGui::Text("%s", r.name.c_str());
        }
        ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f", r.score);
        ImGui::TableSetColumnIndex(2); ImGui::Text("%s", r.unit.c_str());
        ImGui::TableSetColumnIndex(3); ImGui::Text("%.3f", r.avg_ms);
        ImGui::TableSetColumnIndex(4); ImGui::Text("%.3f", r.min_ms);
        ImGui::TableSetColumnIndex(5); ImGui::Text("%.3f", r.max_ms);
        ImGui::TableSetColumnIndex(6); ImGui::Text("%.3f", r.p1_ms);
        ImGui::TableSetColumnIndex(7); ImGui::Text("%.3f", r.median_ms);
        ImGui::TableSetColumnIndex(8); ImGui::Text("%.3f", r.p99_ms);

        ImGui::TableSetColumnIndex(9);
        double cv_pct = r.cv * 100.0;
        ImVec4 cv_color;
        if (cv_pct < 5.0)       cv_color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
        else if (cv_pct < 15.0) cv_color = ImVec4(1.0f, 1.0f, 0.3f, 1.0f);
        else                    cv_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        ImGui::TextColored(cv_color, "%.1f%%", cv_pct);
    }

    ImGui::EndTable();
}
