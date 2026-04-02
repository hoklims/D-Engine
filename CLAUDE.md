# CLAUDE.md — D-Engine 2.0

## Langue
Reponses en francais. Code, commits, variables, noms de fichiers en anglais.

## Branche
Tout le travail 2.0 sur `de-engine-2.0`. Ne jamais modifier `main`.

## Source de verite
- Handbook : `D-Engine_2.0_Handbook.md`
- Specs : `Docs/2.0/INDEX.md`
- Decisions : `Docs/2.0/Decision_Log.md`
- Architecture detaillee : `Docs/2.0/Architecture_Claude.md`

## Stack
- C++23, MSVC (Visual Studio 2022), CMake 3.28+
- Win32 API (fenetre/platform)
- DirectX 12 + HLSL (futur)
- Namespace `de::`

## Build / Test / Run

```bash
# Configure (une seule fois)
cmake --preset default

# Build Debug
cmake --build Build --config Debug

# Build Release
cmake --build Build --config Release

# Tous les tests
ctest --test-dir Build --build-config Debug

# Un seul test (build + run)
cmake --build Build --config Debug --target EcsTest && ./Build/Tests/Debug/EcsTest.exe

# Run
./Build/Source/Debug/DEngine.exe
```

Tests disponibles : `FixedStepTest`, `TimelineTest`, `TelemetryTest`, `EcsTest`, `RuntimeEcsTest`, `CrowdTest`, `NavTest`, `LodTest`, `MeleeTest`, `SimHashTest`, `RenderFrameTest`, `EngineBootTest`, `EngineRuntimeTest`, `DebugOverlayTest`, `AvoidanceTest`, `BenchmarkTest`, `BudgetTest`, `BudgetResponseTest`, `CameraTest`, `CullingTest`, `DebugControlsTest`, `DemoPresetTest`, `InstanceTest`, `RenderStatsTest`, `WorldDebugTest`, `DebugHudTest`, `HudContractTest`.

## Conventions code
- `/W4 /WX /permissive-` -- zero warnings obligatoire
- ASCII only dans les sources
- Pas de sur-ingenierie, pas de code speculatif
- Fonctions pures par defaut, effets de bord isoles

## Commits
Format conventionnel : `feat:` / `fix:` / `refactor:` / `docs:` / `test:` / `chore:`
Toujours atomiques. Diff propre.

## Architecture (resume)

3 couches : **ECS** (lib statique `DEcs`), **Runtime** (simulation + systemes), **Platform** (Win32).

- **ECS** : archetypal SoA, EntityPool generationnel, World/WorldView (read-only), mutation via CommandBuffer
- **Runtime** : boucle fixe (`begin_frame` > `tick_fixed_steps` > `render` > `end_frame`). SimState orchestre World + 13 systemes crowd (classify_lod > select_targets > ... > integrate_position). SpatialGrid + BattlefieldGrid + BehaviorLod + LocalAvoidance.
- **Render** : DX12 minimal instancie, extraction RenderFrame/DebugOverlayData, debug HUD multi-section (Hidden/Compact/Full), ViewCulling, WorldDebugPass. Deux geometries : quad + kite.
- **Platform** : Window Win32 (HWND, message pump, 1280x720)
- **Tests** : framework custom `check(expr)`, executables independants via CMake `add_test()`

Pour les details complets (composants, pipeline crowd, renderer, contrats) -> `Docs/2.0/Architecture_Claude.md`
