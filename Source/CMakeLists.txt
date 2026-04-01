add_executable(DEngine WIN32
    Runtime/Main.cpp
    Runtime/Engine.h
    Runtime/Engine.cpp
    Runtime/EngineConfig.h
    Runtime/DebugControls.h
    Runtime/DebugControls.cpp
    Runtime/FrameInfo.h
    Runtime/Clock.h
    Runtime/Clock.cpp
    Runtime/FixedStep.h
    Runtime/FixedStep.cpp
    Runtime/FrameTelemetry.h
    Runtime/ScopeTimer.h
    Runtime/ScopeTimer.cpp
    Runtime/CommandBuffer.h
    Runtime/Components.h
    Runtime/CrowdComponents.h
    Runtime/CrowdSystems.h
    Runtime/CrowdSystems.cpp
    Runtime/SpatialGrid.h
    Runtime/BattlefieldGrid.h
    Runtime/SimState.h
    Runtime/SimState.cpp
    Platform/Window.h
    Platform/Window.cpp
    Render/RenderCamera.h
    Render/RenderCamera.cpp
    Render/RenderFrame.h
    Render/RenderStats.h
    Render/RenderFrame.cpp
    Render/Renderer.h
    Render/Renderer.cpp
    Render/WorldDebugPass.h
    Render/WorldDebugPass.cpp
    Render/BitmapFont.h
    Render/DebugOverlay.h
    Render/DebugOverlay.cpp
)

target_include_directories(DEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DEngine PRIVATE DEcs d3d12 dxgi d3dcompiler)

# --- ECS static library (standalone, no Win32 dependency) ---
add_library(DEcs STATIC
    ECS/EntityPool.h
    ECS/EntityPool.cpp
    ECS/ComponentId.h
    ECS/Archetype.h
    ECS/Archetype.cpp
    ECS/World.h
    ECS/World.cpp
    ECS/WorldView.h
)
target_include_directories(DEcs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
