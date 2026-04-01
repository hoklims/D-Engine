#pragma once

#include "Runtime/EngineConfig.h"
#include "Runtime/SimState.h"
#include "Render/WorldDebugPass.h"

#include <cstdint>

namespace de {

// A demo preset bundles crowd config, bootstrap path, world debug
// presentation, and camera hint into a single switchable package.
struct DemoPreset {
    const char*  name;
    StartScene   scene_type;
    CrowdConfig  crowd;
    // Battlefield extensions (ignored unless scene_type == Battlefield).
    int          bf_grid_w         = 60;
    int          bf_grid_h         = 40;
    float        bf_cell           = 1.0f;
    float        bf_ox             = -30.0f;
    float        bf_oy             = -20.0f;
    const ObstacleDef* bf_obstacles      = nullptr;
    int                bf_obstacle_count = 0;
    // World debug presentation.
    WorldDebugConfig world_debug;
    // Camera initial half-width hint.
    float        camera_hw         = 50.0f;
};

// -- WallGap obstacle table --------------------------------------------------
// Vertical wall at grid column 30 (world x=0), gap at rows 17-22 (world y~0).

static constexpr ObstacleDef k_wall_gap_obstacles[] = {
    {30,  0}, {30,  1}, {30,  2}, {30,  3}, {30,  4},
    {30,  5}, {30,  6}, {30,  7}, {30,  8}, {30,  9},
    {30, 10}, {30, 11}, {30, 12}, {30, 13}, {30, 14},
    {30, 15}, {30, 16},
    // gap: rows 17-22
    {30, 23}, {30, 24}, {30, 25}, {30, 26}, {30, 27},
    {30, 28}, {30, 29}, {30, 30}, {30, 31}, {30, 32},
    {30, 33}, {30, 34}, {30, 35}, {30, 36}, {30, 37},
    {30, 38}, {30, 39},
};

static constexpr int k_wall_gap_obstacle_count =
    static_cast<int>(sizeof(k_wall_gap_obstacles) / sizeof(k_wall_gap_obstacles[0]));

// -- Preset table ------------------------------------------------------------

static constexpr DemoPreset k_demo_presets[] = {
    // 0: LaneClash -- classic head-on two-army charge with avoidance.
    {
        "LaneClash",
        StartScene::Crowd,
        /* crowd */ { 40, 30.0f, 1.5f, 3.5f, 100.0f, 2.0f, 10.0f, 1.0f, 0.8f, 5.0f, 20.0f,
                      /* avoidance */ 3.5f, 0.9f, 3.0f },
        /* bf */ 60, 40, 1.0f, -30.0f, -20.0f, nullptr, 0,
        /* world_debug */ { true, true, 60.0f, 10.0f },
        /* camera_hw */ 45.0f,
    },
    // 1: DenseMelee -- packed brawl, instant engagement.
    {
        "DenseMelee",
        StartScene::Crowd,
        /* crowd */ { 60, 10.0f, 0.8f, 2.0f, 80.0f, 1.5f, 8.0f, 0.5f, 0.6f, 6.0f, 8.0f,
                      /* avoidance */ 2.0f, 0.5f, 2.0f },
        /* bf */ 60, 40, 1.0f, -30.0f, -20.0f, nullptr, 0,
        /* world_debug */ { true, true, 30.0f, 5.0f },
        /* camera_hw */ 25.0f,
    },
    // 2: WallGap -- battlefield with obstacle wall, funnel through gap.
    {
        "WallGap",
        StartScene::Battlefield,
        /* crowd */ { 30, 25.0f, 1.5f, 3.0f, 100.0f, 2.0f, 10.0f, 1.0f, 0.8f, 5.0f, 15.0f,
                      /* avoidance */ 3.0f, 0.8f, 2.5f },
        /* bf */ 60, 40, 1.0f, -30.0f, -20.0f,
                 k_wall_gap_obstacles, k_wall_gap_obstacle_count,
        /* world_debug */ { true, true, 40.0f, 10.0f },
        /* camera_hw */ 35.0f,
    },
    // 3: SparseApproach -- few agents, long approach, shows LOD tiers.
    {
        "SparseApproach",
        StartScene::Crowd,
        /* crowd */ { 12, 50.0f, 3.0f, 4.0f, 150.0f, 2.5f, 12.0f, 1.2f, 1.0f, 4.0f, 25.0f,
                      /* avoidance */ 3.0f, 0.8f, 2.5f },
        /* bf */ 60, 40, 1.0f, -30.0f, -20.0f, nullptr, 0,
        /* world_debug */ { true, true, 80.0f, 20.0f },
        /* camera_hw */ 65.0f,
    },

    // -- Stress presets ---------------------------------------------------------
    // Designed to push specific axes for perf measurement and budget testing.

    // 4: StressLane -- 200 agents/team, wide front, long approach.
    //    Axis: raw agent count + targeting scan cost.
    {
        "StressLane",
        StartScene::Crowd,
        /* crowd */ { 200, 40.0f, 0.8f, 3.0f, 100.0f, 2.0f, 10.0f, 1.0f, 0.6f, 5.0f, 25.0f,
                      /* avoidance */ 3.0f, 0.8f, 2.5f },
        /* bf */ 60, 40, 1.0f, -30.0f, -20.0f, nullptr, 0,
        /* world_debug */ { true, true, 80.0f, 10.0f },
        /* camera_hw */ 55.0f,
    },
    // 5: StressDenseMelee -- 300 agents/team, tight spawn, instant brawl.
    //    Axis: melee broadphase + separation density.
    {
        "StressDenseMelee",
        StartScene::Crowd,
        /* crowd */ { 300, 8.0f, 0.5f, 2.0f, 60.0f, 1.5f, 6.0f, 0.4f, 0.5f, 6.0f, 6.0f,
                      /* avoidance */ 2.0f, 0.5f, 2.0f },
        /* bf */ 60, 40, 1.0f, -30.0f, -20.0f, nullptr, 0,
        /* world_debug */ { true, true, 30.0f, 5.0f },
        /* camera_hw */ 25.0f,
    },
    // 6: StressWallGap -- 150 agents/team through a funnel.
    //    Axis: nav queries + congestion at chokepoint.
    {
        "StressWallGap",
        StartScene::Battlefield,
        /* crowd */ { 150, 25.0f, 0.6f, 3.0f, 100.0f, 2.0f, 10.0f, 1.0f, 0.6f, 5.0f, 15.0f,
                      /* avoidance */ 3.0f, 0.8f, 2.5f },
        /* bf */ 60, 40, 1.0f, -30.0f, -20.0f,
                 k_wall_gap_obstacles, k_wall_gap_obstacle_count,
        /* world_debug */ { true, true, 50.0f, 10.0f },
        /* camera_hw */ 45.0f,
    },
};

static constexpr int k_demo_preset_count =
    static_cast<int>(sizeof(k_demo_presets) / sizeof(k_demo_presets[0]));

// Apply a preset to a SimState (bootstrap with the right path and config).
inline void apply_demo_preset(SimState& sim, const DemoPreset& p) {
    if (p.scene_type == StartScene::Battlefield) {
        BattlefieldConfig bf;
        bf.crowd          = p.crowd;
        bf.grid_width     = p.bf_grid_w;
        bf.grid_height    = p.bf_grid_h;
        bf.grid_cell      = p.bf_cell;
        bf.grid_ox        = p.bf_ox;
        bf.grid_oy        = p.bf_oy;
        bf.obstacles      = p.bf_obstacles;
        bf.obstacle_count = p.bf_obstacle_count;
        sim.bootstrap_battlefield(bf);
    } else {
        sim.bootstrap_crowd(p.crowd);
    }
}

// Preset name by index. Returns nullptr if out of range.
inline const char* preset_name(int8_t index) {
    if (index < 0 || index >= k_demo_preset_count) return nullptr;
    return k_demo_presets[index].name;
}

} // namespace de
