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
