#pragma once

#include <cstdint>

namespace de {

// Which scene to bootstrap at engine startup.
enum class StartScene : uint8_t {
    Basic,        // non-crowd (4 physics entities)
    Crowd,        // 2-team crowd, default CrowdConfig
    Battlefield,  // crowd + navigation grid, default BattlefieldConfig
};

struct EngineConfig {
    // Fixed simulation rate in Hz.
    double sim_rate_hz = 60.0;

    // Maximum wall-clock delta allowed per frame (seconds).
    // Larger spikes are clamped to this value.
    double max_frame_delta = 0.25;

    // Maximum number of fixed steps consumed per frame.
    // Prevents spiral-of-death when the sim can't keep up.
    uint32_t max_steps_per_frame = 8;

    // Scene bootstrapped at init. Default = Crowd so that DEngine.exe
    // shows a visible crowd out of the box.
    StartScene start_scene = StartScene::Crowd;

    // Set to false to skip DX12 renderer creation (headless mode).
    bool enable_renderer = true;
};

} // namespace de
