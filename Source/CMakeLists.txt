add_executable(DEngine WIN32
    Runtime/Main.cpp
    Runtime/Engine.h
    Runtime/Engine.cpp
    Runtime/EngineConfig.h
    Runtime/FrameInfo.h
    Runtime/Clock.h
    Runtime/Clock.cpp
    Runtime/FixedStep.h
    Runtime/FixedStep.cpp
    Runtime/FrameTelemetry.h
    Runtime/ScopeTimer.h
    Runtime/ScopeTimer.cpp
    Platform/Window.h
    Platform/Window.cpp
)

target_include_directories(DEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
