# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Langue
Reponses en francais. Code, commits, variables, noms de fichiers en anglais.

## Branche
Tout le travail 2.0 se fait sur `de-engine-2.0`. Ne jamais modifier `main`.

## Source de verite
- Handbook : `D-Engine_2.0_Handbook.md`
- Specs : `Docs/2.0/INDEX.md`
- Decisions : `Docs/2.0/Decision_Log.md`

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

Tests disponibles : `FixedStepTest`, `TimelineTest`, `TelemetryTest`, `EcsTest`, `RuntimeEcsTest`, `CrowdTest`, `NavTest`, `LodTest`, `MeleeTest`, `SimHashTest`, `RenderFrameTest`, `EngineBootTest`, `EngineRuntimeTest`, `DebugOverlayTest`.

## Conventions code
- `/W4 /WX /permissive-` -- zero warnings obligatoire
- ASCII only dans les sources
- Pas de sur-ingenierie, pas de code speculatif
- Fonctions pures par defaut, effets de bord isoles

## Commits
Format conventionnel : `feat:` / `fix:` / `refactor:` / `docs:` / `test:` / `chore:`
Toujours atomiques. Diff propre.

---

## Architecture

Le moteur est organise en 3 couches : **ECS** (lib statique `DEcs`), **Runtime** (logique moteur + simulation), **Platform** (Win32).

### ECS (Source/ECS/) -- lib statique `DEcs`

ECS archetypal avec stockage SoA (Structure of Arrays). Aucune dependance Win32.

- **EntityPool** : allocation generationnelle (index + generation) pour reutilisation safe des IDs
- **ComponentId** : identification type-erased via adresse de fonction (pas de RTTI)
- **Archetype** : stocke les entites ayant la meme signature de composants dans des colonnes SoA. L'ajout/retrait d'un composant migre l'entite vers un autre archetype.
- **World** : facade ECS -- create/destroy entity, set/get/remove component, iteration via `each<Cs...>(fn)`
- **WorldView** : vue read-only sur World passee aux systemes. Interdit les mutations structurelles (create/destroy/add/remove) pendant l'iteration -- garantit un etat coherent.

### Runtime (Source/Runtime/)

**Boucle principale** (Engine) : `begin_frame` -> `tick_fixed_steps` (1+ ticks fixes) -> `render` -> `end_frame`

**Simulation deterministe** :
- `Clock` : temps haute resolution via QueryPerformanceCounter
- `FixedStep` : accumulateur a pas fixe avec clamp max_frame_delta et cap max_steps_per_frame (anti spiral-of-death)
- `EngineConfig` : sim_rate_hz, max_frame_delta, max_steps_per_frame
- `FrameInfo` : snapshot par frame (frame_index, sim_tick_index, steps_this_frame, alpha d'interpolation)
- `FrameTelemetry` + `ScopeTimer` : telemetrie RAII par phase

**Contrat de mutation differee** (CommandBuffer) :
- Les systemes ne font JAMAIS de mutations structurelles directement. Ils recoivent un `WorldView` (read-only) et un `CommandBuffer`.
- `CommandBuffer::spawn<F>(init)` / `destroy(id)` mettent en file d'attente.
- `CommandBuffer::apply(world)` applique tout d'un coup apres tous les systemes (spawns d'abord, puis destroys).
- Resultat deterministe independant de l'ordre d'iteration.

**SimState** : orchestre World + pipeline de systemes. Chaque tick :
1. Clear CommandBuffer
2. Creer WorldView
3. Executer chaque FixedSystemFn en ordre avec (WorldView, dt, CommandBuffer)
4. CommandBuffer::apply()
5. Incrementer tick_count

**Pipeline crowd (CrowdSystems) -- 12 etapes ordonnees** :
1. `classify_behavior_lod` -- tier 0-3 par distance au battle center + engagement
2. `select_targets` -- scan spatial grid, find_nearest_enemy -> Target
3. `compute_battle_goal` -- direction via flow field (BattlefieldGrid) ou direct vers BattleGoal
4. `compute_desired_movement` -- si ennemi dans EngageRadius, override direction vers ennemi
5. `apply_crowd_steering` -- Velocity = DesiredDirection * MoveSpeed
6. `apply_separation` -- repulsion soft des allies proches (anti-stacking)
7. `gather_melee_candidates` -- broadphase spatial: paires attaquant-cible validees
8. `attack_targets` -- tick cooldown, emit 1 hit max par agent si broadphase confirme
9. `resolve_damage` -- applique tous les degats simultanement (atomique)
10. `remove_dead` -- queue destroy pour Health <= 0
11. `integrate_velocity` -- physique
12. `integrate_position` -- physique

**SpatialGrid** : grille de hash 2D pour la selection de cibles. Recherche par anneau expansif (Chebyshev) avec early exit. Reconstruite a chaque tick.

**BattlefieldGrid** : grille statique 2D avec champ d'integration BFS. Cellules FREE/BLOCKED. Les agents echantillonnent une direction de flux au lieu de pointer directement vers le BattleGoal. Contrat : si sample_flow() echoue, direction ZERO (pas de fallback line-of-sight).

**BehaviorLod** : systeme de LOD comportemental a 4 tiers (T0=chaque tick, T1=1/2, T2=1/4, T3=1/8). Les agents engages sont toujours T0. Classification par distance au battle center explicite. Combat + physique toujours full fidelity, seuls navigation/separation sont gates.

**Composants** :
- Core : `Position`, `Velocity`, `Acceleration`
- Crowd : `CrowdAgent` (tag), `Team`, `MoveSpeed`, `Target`, `DesiredDirection`, `Health`, `AttackRange`, `AttackDamage`, `AttackCooldown`, `BattleGoal`, `EngageRadius`, `Separation`, `BehaviorLod`
- Config : `BehaviorLodConfig` (seuils LOD + battle center)

### Render (Source/Render/)

**Frame extraction** :
- `RenderFrame` : snapshot lecture seule de la foule (positions, equipe, LOD, HP). Extrait depuis le World apres le dernier tick du frame.
- `CrowdRenderItem` : donnees par agent (x, y, team_id, lod_tier, health_pct).
- `extract_render_frame()` : itere les CrowdAgent du World, remplit RenderFrame. Cap a k_max_render_agents (4096). Deterministe (meme World = meme frame).

**Renderer DX12 minimal** :
- Device DX12 (feature level 11_0, fallback WARP).
- Swap chain double-buffered, flip-discard.
- Command queue direct, allocator unique, synchrone (CPU wait GPU chaque frame).
- Root signature : 16 root constants (matrice ortho 4x4).
- PSO : triangle list, VS/PS compiles a l'init via D3DCompile (HLSL inline).
- Vertex buffer upload heap, map persistant. 6 vertices/agent (quad 2 triangles).
- Couleur par equipe (rouge, bleu, vert, jaune), modulee par HP.
- Projection orthographique centree (50 unites demi-largeur).
- Pas de depth, pas de MSAA, pas de textures, pas d'instancing avance.

**Debug overlay** :
- `DebugOverlayData` : lignes de texte extraites du runtime (scene, paused, tick, agents, budget).
- `extract_debug_overlay()` : remplit DebugOverlayData depuis l'etat Engine courant (pas de lag).
- `BitmapFont.h` : police 5x7 pixels statique (ASCII 32-126, 95 glyphes).
- `generate_overlay_instances()` : genere des pixel-quads (1 instance par dot allume) + 1 panneau fond.
- Rendu en 3e pass apres world+crowd, avec matrice ortho screen-space.
- Cap a k_max_overlay_instances (2048). Pas de texture, reutilise le PSO instancie existant.
- Contrat overlay : drawn/dropped viennent de `RenderFrame` (extraction), pas des stats renderer.

**RenderStats** :
- `draw_call_count` = total reel GPU (world + crowd + overlay).
- `overlay_draw_call_count` : 0 ou 1 selon presence overlay.
- Invariant : `draw_call_count == world_draw_call_count + overlay_draw_call_count + (instance_count > 0 ? 1 : 0)`.

**Integration Engine** :
- `update_presentation()` extrait la RenderFrame.
- `render()` extrait l'overlay puis soumet au Renderer DX12.
- Si le Renderer echoue a l'init, le moteur continue en mode headless.

### Platform (Source/Platform/)

`Window` : wrapper Win32 (HWND, message pump, is_open, width/height). Ouvre une fenetre 1280x720.

### Tests (Tests/)

Framework custom minimal : macro `check(expr)` avec compteurs `g_pass`/`g_fail`. Pas de framework externe. Chaque test est un executable independant enregistre via `add_test()` dans CMake.

---

## Yoyo -- Intelligence de code AST

Yoyo tourne dans un conteneur Docker (`yoyo:latest`, montage `-v .:/repo`).

### Regles obligatoires
- Ne JAMAIS passer de chemin Windows (`H:/...`, `C:/...`) aux outils yoyo
- Appeler `boot` et `index` en parallele sur premier contact avec le repo
- Chemins internes : `/repo` ou `/repo/sous-dossier`

### Fraicheur de l'index -- CRITIQUE

L'index AST devient perime des qu'un fichier source est cree, modifie ou
supprime. Un index perime rend `inspect`, `search`, `ask`, `impact`, etc.
aveugles aux changements recents.

**Quand re-indexer** (appel `index` sans argument) :
- Apres toute creation de fichier(s) source (.h, .cpp)
- Apres tout rename/move/delete de fichier(s) source
- Apres un step d'implementation qui a modifie >= 3 fichiers
- Avant toute phase d'exploration yoyo si des fichiers ont change depuis
  le dernier `index`
- En cas de doute -> re-indexer. Le cout est faible (~2s).

**Quand NE PAS re-indexer** :
- Si seuls des fichiers non-source ont change (CMakeLists, .md, .json)
- Si on vient juste d'indexer et qu'aucun source n'a change depuis

### Usage obligatoire de yoyo -- PRIORITAIRE sur Grep/Glob/Read bruts

Les outils yoyo ne sont pas optionnels. Ils DOIVENT etre utilises quand
l'intention correspond au tableau ci-dessous. Utiliser Grep/Glob/Read bruts
a la place de yoyo dans ces cas est une VIOLATION du protocole.

| Intention | Outil yoyo | Pourquoi |
|---|---|---|
| Trouver un symbole (fonction, classe, struct) | `inspect` | Resolution AST exacte, pas de faux positifs textuels |
| Chercher du code par intent ou mot-cle | `search` | Recherche semantique dans l'index AST |
| Comprendre un fichier ou une zone de code | `inspect` | Montre structure + contexte sans bruit |
| Poser une question sur le code | `ask` | Repond en s'appuyant sur l'AST, pas du grep naif |
| Lister les fichiers du projet | `map` | Carte structuree par scope, mieux que glob brut |
| Voir les routes/points d'entree | `routes` | Detecte entry points automatiquement |
| Analyser l'impact d'un changement | `impact` | Trace les dependances (callers, callees, refs) |
| Juger un changement avant de l'appliquer | `judge_change` | Ownership, invariants, risque de regression |
| Verifier la sante globale du code | `health` | Metriques de complexite, couplage, couverture |
| Appliquer une modification safe | `change` | Edit avec bornes d'erreur, mieux que Edit brut pour refactors |
| Enchainer plusieurs lectures | `script` | Compose plusieurs outils en un seul appel |

### Integration dans le workflow OCO

Yoyo s'insere dans le protocole OCO aux moments suivants :
- **Phase Classify** : `health` pour evaluer l'etat du code avant de commencer
- **Phase Plan (exploration)** : `map` + `search` + `inspect` pour comprendre
  la zone impactee AVANT de planifier. Ne jamais planifier a l'aveugle.
- **Phase Execute (avant un step d'implementation)** : `judge_change` si le
  step modifie du code existant (pas pour du code 100% nouveau)
- **Phase Execute (apres un step d'implementation)** : `index` pour rafraichir
  l'AST, puis `impact` si d'autres steps dependent du code modifie
- **Phase Verify** : `health` en complement du build/test

### Quand NE PAS utiliser yoyo
- Lecture d'un fichier specifique deja connu -> `Read` direct
- Recherche d'une chaine exacte dans 1-2 fichiers -> `Grep` direct
- Operations git, build, tests -> `Bash` direct

## OCO Headless -- Protocole obligatoire

Chaque requete de travail DOIT suivre le protocole OCO en mode headless (sans
appels dashboard/emit_events MCP). Aucun raccourci, aucun champ saute.

### 1. Classify
- Type (feat, fix, refactor, test, docs, chore)
- Complexite (low, medium, high)
- Routing (implementation directe, refactor, bug, trace)
- Raison courte

### 2. Plan Exploration
Evaluer au minimum 2 candidats avec TOUS les champs :
- strategy (speed / safety / autre)
- step_count
- estimated_tokens
- verify_count
- parallel_groups
- team (null ou composition d'agents)
- score (0.0 - 1.0)

Pour chaque candidat, decrire l'approche en une ligne.
Declarer le winner avec justification.

#### Regles contraignantes sur parallel_groups et team

`parallel_groups` et `team` sont des ENGAGEMENTS, pas des souhaits.
Le plan annonce doit correspondre exactement a l'execution reelle.

**parallel_groups** = nombre de groupes de steps qui s'executent en parallele.
- Si `parallel_groups: 1` -> tout est sequentiel, pas de sub-agent obligatoire.
- Si `parallel_groups: N` (N > 1) -> le plan DOIT contenir des steps avec le
  meme `group` ID, et ces steps DOIVENT etre lances en parallele via l'outil
  Agent (un sub-agent par step du meme groupe).
- Un step `mode: agent` DOIT etre execute via l'outil Agent (sub-agent).
- Un step `mode: inline` DOIT etre execute dans le contexte principal.
- Ne JAMAIS annoncer `parallel_groups: N > 1` puis tout executer en sequence.
  Si la parallelisation n'apporte rien, annoncer `parallel_groups: 1`.

**team** = composition de sub-agents specialises.
- Si `team: null` -> tout le travail est fait par le contexte principal
  (eventuellement avec des sub-agents Explore/researcher ponctuels).
- Si `team` est definie (ex: `[implementer, verifier]`) -> les roles declares
  DOIVENT etre remplis par des sub-agents lances via l'outil Agent avec le
  `subagent_type` correspondant :
  - `researcher` -> Agent(subagent_type=Explore ou researcher)
  - `implementer` -> Agent(subagent_type=general-purpose, mode=bypassPermissions)
  - `verifier` -> Agent(subagent_type=patch-verifier ou code-reviewer)
  - `architect` -> Agent(subagent_type=architect)
- Ne JAMAIS annoncer une team puis faire tout le travail inline.

**Coherence plan/execution** :
- Si a l'execution on realise que la parallelisation est inutile, le declarer
  explicitement : "Plan ajuste : parallel_groups 2 -> 1, raison : {motif}".
- Toute deviation du plan doit etre annoncee AVANT l'execution du step concerne.

### 3. Plan Generated
Table de steps avec :
- ID (S1, S2, ...)
- Step name
- Description
- Role (researcher, implementer, verifier)
- Mode (inline, agent)
- Group (G1, G2, ... -- steps du meme groupe s'executent en parallele)
- Depends (quels steps/groupes precedents)
- Verify (yes/no -- gate de verification apres ce step)
- Est. tokens
- Model (-- si default, sinon haiku/sonnet si step simple)

Le nombre de groupes distincts DOIT correspondre a `parallel_groups`.
Les steps d'un meme groupe ne doivent avoir aucune dependance entre eux.

### 4. Execute
Pour chaque groupe :
- Si le groupe contient 1 step -> executer inline ou agent selon `mode`
- Si le groupe contient N steps -> lancer N appels Agent EN PARALLELE
  (un seul message avec N tool calls Agent)
- Pour chaque step : afficher "### S{N} : {name}" avant, "S{N} done." apres
- Les steps agent retournent leur resultat au contexte principal qui
  synthetise et passe au groupe suivant

### 5. Verify Gate
Apres chaque step marque verify=yes :
- Build Debug (+ Release si step final)
- Tests existants
- Zero warnings
- Afficher table de resultats

### 6. Complete (Run Stopped)
- Total steps et total fichiers
- Resume court
- Fichiers crees/modifies (table)
- Commandes exactes build/test/run
- Hash du commit (si commit demande)
- Decisions prises
- Decisions reportees
- Deviations du plan (si le plan a ete ajuste en cours de route)

## Skills OCO -- Utilisation obligatoire

Les skills `/oco-*` sont le point d'entree principal pour les workflows
structures. Ils DOIVENT etre utilises quand l'intention correspond, via
l'outil `Skill`. Ne JAMAIS reproduire manuellement ce qu'un skill fait.

### Routing par intention (contraignant)

| Situation detectee | Skill a invoquer | Obligatoire |
|---|---|---|
| Apres TOUT changement de fichier source | `/oco-verify-fix` | **OUI** |
| Refactoring, rename, restructure, extract, move | `/oco-safe-refactor` | **OUI** |
| Bug, comportement casse, regression (sans stacktrace) | `/oco-investigate-bug` | **OUI** |
| Stacktrace, panic, exception, crash, erreur runtime | `/oco-trace-stack` | **OUI** |
| Explorer, comprendre un module, un flux, une archi | `/oco-inspect-repo-area` | **OUI** |
| Orchestration complexe multi-etapes | `/oco` | recommande |

### Regles

1. **`/oco-verify-fix` est NON NEGOCIABLE** apres tout changement de code.
   Ne jamais considerer un step d'implementation comme termine sans avoir
   execute ce skill (ou, si indisponible, build -> types -> lint -> tests
   manuellement).

2. **`/oco-safe-refactor` AVANT tout refactoring**. Ne jamais commencer a
   renommer/deplacer/extraire du code sans passer par ce skill qui fait
   l'analyse d'impact en amont.

3. **`/oco-investigate-bug` apres 2 tentatives de fix echouees** sur le
   meme probleme. Ne pas continuer a deviner -- laisser le skill structurer
   l'investigation.

4. **`/oco-trace-stack` des qu'une stacktrace apparait** dans la sortie
   d'un build, test, ou run. Ne pas parser la stacktrace a la main.

5. **`/oco-inspect-repo-area` pour l'exploration initiale** d'une zone de
   code inconnue. Ne pas sauter l'exploration pour aller directement coder.

6. Les skills OCO s'integrent dans le protocole OCO headless :
   - Phase Classify -> le routing determine quel skill utiliser
   - Phase Execute -> les steps de type `verify` DOIVENT appeler `/oco-verify-fix`
   - Phase Verify Gate -> `/oco-verify-fix` remplace le build/test/lint manuel

### Fallback si skills indisponibles

Si les skills `/oco-*` ne sont pas listes dans les skills disponibles :
- `/oco-verify-fix` -> build Debug + ctest + zero warnings (manuel)
- `/oco-safe-refactor` -> @refactorer puis @refactor-reviewer (sub-agents)
- `/oco-investigate-bug` -> @debugger (sub-agent)
- `/oco-trace-stack` -> @debugger (sub-agent)
- `/oco-inspect-repo-area` -> Agent(subagent_type=Explore)
