add_executable(FixedStepTest
    FixedStepTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/FixedStep.cpp
)
target_include_directories(FixedStepTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME FixedStepTest COMMAND FixedStepTest)

add_executable(TimelineTest
    TimelineTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/FixedStep.cpp
)
target_include_directories(TimelineTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME TimelineTest COMMAND TimelineTest)

add_executable(TelemetryTest
    TelemetryTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/ScopeTimer.cpp
)
target_include_directories(TelemetryTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME TelemetryTest COMMAND TelemetryTest)

add_executable(EcsTest
    EcsTest.cpp
)
target_link_libraries(EcsTest PRIVATE DEcs)
add_test(NAME EcsTest COMMAND EcsTest)

add_executable(RuntimeEcsTest
    RuntimeEcsTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/SimState.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/CrowdSystems.cpp
)
target_link_libraries(RuntimeEcsTest PRIVATE DEcs)
target_include_directories(RuntimeEcsTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME RuntimeEcsTest COMMAND RuntimeEcsTest)

add_executable(CrowdTest
    CrowdTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/SimState.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/CrowdSystems.cpp
)
target_link_libraries(CrowdTest PRIVATE DEcs)
target_include_directories(CrowdTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME CrowdTest COMMAND CrowdTest)

add_executable(LodTest
    LodTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/SimState.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/CrowdSystems.cpp
)
target_link_libraries(LodTest PRIVATE DEcs)
target_include_directories(LodTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME LodTest COMMAND LodTest)

add_executable(NavTest
    NavTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/SimState.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/CrowdSystems.cpp
)
target_link_libraries(NavTest PRIVATE DEcs)
target_include_directories(NavTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME NavTest COMMAND NavTest)

add_executable(MeleeTest
    MeleeTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/SimState.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/CrowdSystems.cpp
)
target_link_libraries(MeleeTest PRIVATE DEcs)
target_include_directories(MeleeTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME MeleeTest COMMAND MeleeTest)

add_executable(SimHashTest
    SimHashTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/SimState.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/CrowdSystems.cpp
)
target_link_libraries(SimHashTest PRIVATE DEcs)
target_include_directories(SimHashTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME SimHashTest COMMAND SimHashTest)

add_executable(BudgetTest
    BudgetTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/SimState.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/CrowdSystems.cpp
)
target_link_libraries(BudgetTest PRIVATE DEcs)
target_include_directories(BudgetTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME BudgetTest COMMAND BudgetTest)

add_executable(BudgetResponseTest
    BudgetResponseTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/SimState.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/CrowdSystems.cpp
)
target_link_libraries(BudgetResponseTest PRIVATE DEcs)
target_include_directories(BudgetResponseTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME BudgetResponseTest COMMAND BudgetResponseTest)

add_executable(CameraTest
    CameraTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/RenderCamera.cpp
)
target_include_directories(CameraTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME CameraTest COMMAND CameraTest)

add_executable(InstanceTest
    InstanceTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/RenderFrame.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/SimState.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/CrowdSystems.cpp
)
target_link_libraries(InstanceTest PRIVATE DEcs)
target_include_directories(InstanceTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME InstanceTest COMMAND InstanceTest)

add_executable(RenderFrameTest
    RenderFrameTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/RenderFrame.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/SimState.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/CrowdSystems.cpp
)
target_link_libraries(RenderFrameTest PRIVATE DEcs)
target_include_directories(RenderFrameTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME RenderFrameTest COMMAND RenderFrameTest)

add_executable(EngineBootTest
    EngineBootTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/RenderFrame.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/SimState.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/CrowdSystems.cpp
)
target_link_libraries(EngineBootTest PRIVATE DEcs)
target_include_directories(EngineBootTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME EngineBootTest COMMAND EngineBootTest)

add_executable(DebugControlsTest
    DebugControlsTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/DebugControls.cpp
)
target_include_directories(DebugControlsTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME DebugControlsTest COMMAND DebugControlsTest)

add_executable(RenderStatsTest
    RenderStatsTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/Engine.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/DebugControls.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/SimState.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/CrowdSystems.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/Clock.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/FixedStep.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/ScopeTimer.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/RenderFrame.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/RenderCamera.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/Renderer.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/WorldDebugPass.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/DebugOverlay.cpp
    ${CMAKE_SOURCE_DIR}/Source/Platform/Window.cpp
)
target_link_libraries(RenderStatsTest PRIVATE DEcs d3d12 dxgi d3dcompiler)
target_include_directories(RenderStatsTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME RenderStatsTest COMMAND RenderStatsTest)

add_executable(WorldDebugTest
    WorldDebugTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/WorldDebugPass.cpp
)
target_include_directories(WorldDebugTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME WorldDebugTest COMMAND WorldDebugTest)

add_executable(DebugOverlayTest
    DebugOverlayTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/DebugOverlay.cpp
)
target_include_directories(DebugOverlayTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME DebugOverlayTest COMMAND DebugOverlayTest)

add_executable(EngineRuntimeTest
    EngineRuntimeTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/Engine.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/DebugControls.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/SimState.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/CrowdSystems.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/Clock.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/FixedStep.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/ScopeTimer.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/RenderFrame.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/RenderCamera.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/Renderer.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/WorldDebugPass.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/DebugOverlay.cpp
    ${CMAKE_SOURCE_DIR}/Source/Platform/Window.cpp
)
target_link_libraries(EngineRuntimeTest PRIVATE DEcs d3d12 dxgi d3dcompiler)
target_include_directories(EngineRuntimeTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME EngineRuntimeTest COMMAND EngineRuntimeTest)
