#pragma once

#include <cstdint>

namespace de {

// Per-frame render telemetry snapshot.
struct RenderStats {
    uint32_t extracted_count    = 0;  // agents in RenderFrame
    uint32_t instance_count     = 0;  // instances actually submitted to GPU
    uint32_t draw_call_count    = 0;  // DrawInstanced calls this frame
    uint32_t dropped_count      = 0;  // agents over instance cap
};

}  // namespace de
