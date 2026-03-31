# D-Engine 2.0

Nouvelle generation de D-Engine sur la branche `de-engine-2.0`.

D-Engine 2.0 est un moteur from scratch, programmer-first, Windows-first,
specialise dans les jeux d'action crowd-first de type Musou. Sa promesse n'est
pas d'etre generaliste. Sa promesse est de tenir des foules denses, un combat
lisible, un frame-time stable et une simulation reproductible.

## Prerequisites

- Windows 10/11
- Visual Studio 2022 (with C++ Desktop workload)
- CMake 3.28+

## Build

```bash
cmake --preset default
cmake --build Build --config Debug
```

## Test

```bash
cmake --build Build --config Debug --target FixedStepTest
ctest --test-dir Build --build-config Debug
```

## Run

```bash
./Build/Source/Debug/DEngine.exe
```

A 1280x720 window opens. Close it to exit.

## Read next

- Handbook 2.0: `D-Engine_2.0_Handbook.md`
- Docs 2.0: `Docs/2.0/INDEX.md`

## Repository map

- Source/Runtime/: engine core (entry point, main loop, clock, fixed-step)
- Source/Platform/: platform abstraction (Win32 window)
- Tests/: non-graphical smoke tests
- Docs/2.0/: specs and architecture documents
