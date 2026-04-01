#include "Render/RenderFrame.h"
#include "Runtime/SimState.h"
#include "Runtime/EngineConfig.h"
#include "Runtime/CrowdComponents.h"

#include <cstdio>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* name) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::printf("FAIL: %s\n", name);
    }
}

// =================================================================
//  Default scene (Crowd) produces extractable agents
// =================================================================

static void test_default_scene_is_crowd() {
    // Mirrors Engine::init() default path: StartScene::Crowd -> bootstrap_crowd()
    de::SimState sim;
    sim.bootstrap_crowd();

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 0, 0, frame);

    check(frame.agent_count > 0,
          "default scene: crowd agents present");
    check(frame.extracted_count == frame.agent_count,
          "default scene: all agents extracted");
}

// =================================================================
//  Battlefield scene also produces extractable agents
// =================================================================

static void test_battlefield_scene() {
    de::SimState sim;
    sim.bootstrap_battlefield({});

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 0, 0, frame);

    check(frame.agent_count > 0,
          "battlefield scene: crowd agents present");
}

// =================================================================
//  Basic (non-crowd) scene produces zero crowd render items
// =================================================================

static void test_basic_scene_no_crowd() {
    de::SimState sim;
    sim.bootstrap();

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 0, 0, frame);

    check(frame.agent_count == 0,
          "basic scene: no crowd agents");
}

// =================================================================
//  Extraction works after ticks without renderer (headless path)
// =================================================================

static void test_headless_extraction() {
    de::SimState sim;
    sim.bootstrap_crowd();

    for (int i = 0; i < 5; ++i)
        sim.tick(1.0 / 60.0);

    de::RenderFrame frame;
    uint32_t n = de::extract_render_frame(sim.world, 5, 5, frame);

    check(n > 0, "headless: agents extracted after 5 ticks");
    check(frame.sim_tick == 5, "headless: sim_tick correct");
    check(frame.frame_id == 5, "headless: frame_id correct");
}

// =================================================================
//  Extraction is stable across consecutive calls (no side effects)
// =================================================================

static void test_extraction_idempotent() {
    de::SimState sim;
    sim.bootstrap_crowd();
    sim.tick(1.0 / 60.0);

    de::RenderFrame f1, f2;
    de::extract_render_frame(sim.world, 1, 1, f1);
    de::extract_render_frame(sim.world, 1, 1, f2);

    check(f1.extracted_count == f2.extracted_count,
          "idempotent: same count");

    bool same = true;
    for (uint32_t i = 0; i < f1.extracted_count; ++i) {
        if (f1.agents[i].x != f2.agents[i].x ||
            f1.agents[i].y != f2.agents[i].y) {
            same = false;
            break;
        }
    }
    check(same, "idempotent: same positions");
}

// =================================================================
//  EngineConfig defaults: start_scene == Crowd, enable_renderer == true
// =================================================================

static void test_engine_config_defaults() {
    de::EngineConfig cfg;
    check(cfg.start_scene == de::StartScene::Crowd,
          "config default: start_scene == Crowd");
    check(cfg.enable_renderer == true,
          "config default: enable_renderer == true");
}

// =================================================================

int main() {
    test_default_scene_is_crowd();
    test_battlefield_scene();
    test_basic_scene_no_crowd();
    test_headless_extraction();
    test_extraction_idempotent();
    test_engine_config_defaults();

    std::printf("\nEngineBootTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
