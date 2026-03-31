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

# --- ECS static library (standalone, no Win32 dependency) ---
add_library(DEcs STATIC
    ECS/EntityPool.h
    ECS/EntityPool.cpp
    ECS/ComponentId.h
    ECS/Archetype.h
    ECS/Archetype.cpp
    ECS/World.h
    ECS/World.cpp
)
target_include_directories(DEcs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
