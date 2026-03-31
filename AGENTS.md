# D-Engine 2.0 - Agent Notes

Single source of truth for policies and roadmap:

- `D-Engine_2.0_Handbook.md`

Docs complementaires:

- `Docs/2.0/INDEX.md`

Les documents `v0.x` restent lisibles mais sont historiques sur cette branche.

## Quick commands

Historique build/runtime actuel:

- `msbuild D-Engine.sln /p:Configuration=Debug /p:Platform=x64 /m`
- `msbuild D-Engine.sln /p:Configuration=Release /p:Platform=x64 /m`

Run local gates:

- `powershell -ExecutionPolicy Bypass -File tools/run_all_gates.ps1`

Run smokes:

- `x64\Debug\AllSmokes.exe`
- `x64\Release\AllSmokes.exe`
- `x64\Release\MemoryStressSmokes.exe`

Run benchmarks:

- `x64\Release\D-Engine-BenchRunner.exe --warmup 1 --target-rsd 3 --max-repeat 20 --cpu-info`

Regle de branche:

- tout le travail 2.0 se fait sur `de-engine-2.0`
- `main` sert d'etat historique et ne doit pas etre modifiee par ce chantier
