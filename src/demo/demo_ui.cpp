#include "demo/demo_ui.h"
#include "renderer/render_context.h"
#include <imgui.h>
#include <cstdio>

static const char* tierName(DemoTier tier) {
    switch (tier) {
        case DemoTier::Basic:    return "Basic";
        case DemoTier::Enhanced: return "Enhanced";
        case DemoTier::Quality:  return "Quality";
        case DemoTier::Ultra:    return "Ultra";
    }
    return "?";
}

void DemoUI::drawOverlay(RenderContext* ctx,
                          const char* gpu_name,
                          DemoTier current_tier,
                          int tier_index, int total_tiers,
                          float tier_progress,
                          double fps, double frame_ms,
                          const std::vector<double>& frame_history) {
    ctx->imguiNewFrame();

    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(280, 0));
    ImGui::SetNextWindowBgAlpha(0.5f);
    ImGui::Begin("Demo", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoInputs);

    ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s", gpu_name);
    ImGui::Separator();

    char tier_label[64];
    snprintf(tier_label, sizeof(tier_label), "Tier %d: %s (%d/%d)",
             static_cast<int>(current_tier), tierName(current_tier),
             tier_index + 1, total_tiers);
    ImGui::Text("%s", tier_label);
    ImGui::ProgressBar(tier_progress, ImVec2(-1, 0), "");

    ImGui::Spacing();

    // FPS display (large)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.3f, 1.0f));
    char fps_text[32];
    snprintf(fps_text, sizeof(fps_text), "%.0f FPS", fps);
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("%s", fps_text);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    char ms_text[32];
    snprintf(ms_text, sizeof(ms_text), "%.2f ms", frame_ms);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", ms_text);

    // Frame time graph
    if (!frame_history.empty()) {
        float values[120];
        int count = static_cast<int>(frame_history.size());
        if (count > 120) count = 120;
        float max_val = 0;
        for (int i = 0; i < count; i++) {
            values[i] = static_cast<float>(frame_history[frame_history.size() - count + i]);
            if (values[i] > max_val) max_val = values[i];
        }
        if (max_val < 16.67f) max_val = 16.67f; // At least 60 FPS scale
        ImGui::PlotLines("##frametime", values, count, 0, nullptr, 0, max_val * 1.2f, ImVec2(-1, 40));
    }

    // Overall progress
    float overall = (total_tiers > 0)
        ? (tier_index + tier_progress) / total_tiers : 0;
    ImGui::Separator();
    ImGui::Text("Overall:");
    ImGui::SameLine();
    ImGui::ProgressBar(overall, ImVec2(-1, 0), "");

    ImGui::End();
    ctx->imguiRender();
}

void DemoUI::drawResults(RenderContext* ctx, const DemoResults& results) {
    ctx->imguiNewFrame();

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 0));
    ImGui::Begin("Demo Results", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse);

    ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "GPU: %s", results.gpu_name.c_str());
    if (results.render_w > 0 && results.render_h > 0)
        ImGui::TextDisabled("Render: %dx%d", results.render_w, results.render_h);
    ImGui::Separator();

    // Demo Score (large)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
    ImGui::SetWindowFontScale(2.0f);
    char score_text[32];
    snprintf(score_text, sizeof(score_text), "%.0f", results.demo_score);
    ImGui::Text("Demo Score: %s", score_text);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Tier results table
    if (ImGui::BeginTable("TierResults", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Tier");
        ImGui::TableSetupColumn("Avg FPS");
        ImGui::TableSetupColumn("Min FPS");
        ImGui::TableSetupColumn("P1% FPS");
        ImGui::TableSetupColumn("P99% FPS");
        ImGui::TableSetupColumn("Score");
        ImGui::TableHeadersRow();

        for (const auto& t : results.tiers) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("T%d %s", static_cast<int>(t.tier), tierName(t.tier));
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", t.avg_fps);
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", t.min_fps);
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", t.p99_fps);
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", t.p1_fps);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", t.normalized_score);
        }

        ImGui::EndTable();
    }

    ImGui::End();
    ctx->imguiRender();
}
