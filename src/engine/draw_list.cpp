#include "engine/draw_list.h"
#include <algorithm>
#include <cmath>

void DrawList::push(const SceneObject& obj, uint8_t shader_id,
                    uint8_t material_id, float depth) {
    // Clamp depth to [0, 1] range, then quantize to 16-bit
    float d = depth;
    if (d < 0.0f) d = 0.0f;
    if (d > 1.0f) d = 1.0f;
    uint16_t depth_bits = static_cast<uint16_t>(d * 65535.0f);

    uint32_t key = (static_cast<uint32_t>(shader_id) << 24)
                 | (static_cast<uint32_t>(material_id) << 16)
                 | static_cast<uint32_t>(depth_bits);

    DrawCmd cmd;
    cmd.sort_key = key;
    cmd.obj = &obj;
    cmds_.push_back(cmd);
}

void DrawList::sort() {
    std::sort(cmds_.begin(), cmds_.end());
}
