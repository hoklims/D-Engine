# D-Engine 2.0 - Agent Notes

Single source of truth for policies and roadmap:

- `D-Engine_2.0_Handbook.md`

Docs complementaires:

- `Docs/2.0/INDEX.md`

## Quick commands

Configure (once):

- `cmake --preset default`

Build:

- `cmake --build Build --config Debug`
- `cmake --build Build --config Release`

Run:

- `./Build/Source/Debug/DEngine.exe`
- `./Build/Source/Release/DEngine.exe`

## Stack

- C++23, MSVC, CMake 3.28+
- Win32 API (window/platform)
- DirectX 12 + HLSL (future)

## Repository map

- Source/Runtime/: entry point, engine loop, clock
- Source/Platform/: Win32 window

## Regle de branche

- tout le travail 2.0 se fait sur `de-engine-2.0`
- `main` sert d'etat historique et ne doit pas etre modifiee par ce chantier
