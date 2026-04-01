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

add_executable(RenderFrameTest
    RenderFrameTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Render/RenderFrame.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/SimState.cpp
    ${CMAKE_SOURCE_DIR}/Source/Runtime/CrowdSystems.cpp
)
target_link_libraries(RenderFrameTest PRIVATE DEcs)
target_include_directories(RenderFrameTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME RenderFrameTest COMMAND RenderFrameTest)
