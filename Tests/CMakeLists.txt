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
)
target_link_libraries(RuntimeEcsTest PRIVATE DEcs)
target_include_directories(RuntimeEcsTest PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME RuntimeEcsTest COMMAND RuntimeEcsTest)
