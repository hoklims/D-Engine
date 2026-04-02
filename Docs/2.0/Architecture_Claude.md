# D-Engine 2.0 — Architecture detaillee

Ce fichier est la reference d'architecture pour Claude Code.
Charge a la demande, pas a chaque turn.

---

## ECS (Source/ECS/) -- lib statique `DEcs`

ECS archetypal avec stockage SoA (Structure of Arrays). Aucune dependance Win32.

- **EntityPool** : allocation generationnelle (index + generation) pour reutilisation safe des IDs
- **ComponentId** : identification type-erased via adresse de fonction (pas de RTTI)
- **Archetype** : stocke les entites ayant la meme signature de composants dans des colonnes SoA. L'ajout/retrait d'un composant migre l'entite vers un autre archetype.
- **World** : facade ECS -- create/destroy entity, set/get/remove component, iteration via `each<Cs...>(fn)`
- **WorldView** : vue read-only sur World passee aux systemes. Interdit les mutations structurelles (create/destroy/add/remove) pendant l'iteration -- garantit un etat coherent.

## Runtime (Source/Runtime/)

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

**Pipeline crowd (CrowdSystems) -- 13 etapes ordonnees** :
1. `classify_behavior_lod` -- tier 0-3 par distance au battle center + engagement
2. `select_targets` -- scan spatial grid, find_nearest_enemy -> Target
3. `compute_battle_goal` -- direction via flow field (BattlefieldGrid) ou direct vers BattleGoal
4. `compute_desired_movement` -- si ennemi dans EngageRadius, override direction vers ennemi
5. `apply_crowd_steering` -- Velocity = DesiredDirection * MoveSpeed
6. `apply_local_avoidance` -- anticipation TTC + biais lateral (evitement collisions proches)
7. `apply_separation` -- repulsion soft des allies proches (anti-stacking)
8. `gather_melee_candidates` -- broadphase spatial: paires attaquant-cible validees
9. `attack_targets` -- tick cooldown, emit 1 hit max par agent si broadphase confirme
10. `resolve_damage` -- applique tous les degats simultanement (atomique)
11. `remove_dead` -- queue destroy pour Health <= 0
12. `integrate_velocity` -- physique
13. `integrate_position` -- physique

**SpatialGrid** : grille de hash 2D pour la selection de cibles. Recherche par anneau expansif (Chebyshev) avec early exit. Reconstruite a chaque tick.

**BattlefieldGrid** : grille statique 2D avec champ d'integration BFS. Cellules FREE/BLOCKED. Les agents echantillonnent une direction de flux au lieu de pointer directement vers le BattleGoal. Contrat : si sample_flow() echoue, direction ZERO (pas de fallback line-of-sight).

**BehaviorLod** : systeme de LOD comportemental a 4 tiers (T0=chaque tick, T1=1/2, T2=1/4, T3=1/8). Les agents engages sont toujours T0. Classification par distance au battle center explicite. Combat + physique toujours full fidelity, seuls navigation/separation/avoidance sont gates.

**LocalAvoidance** : evitement anticipe par Time-To-Closest-Approach (TTC). Pour chaque agent, scan des voisins dans `radius`. Si deux agents convergent (vitesse relative positive), calcul du TTC. Si TTC < `horizon`, application d'une force laterale perpendiculaire a l'axe d'approche. Cote deterministe : EntityId inferieur dodge a gauche, superieur a droite. Clamp final a MoveSpeed.max. LOD gate. Contrat de simultaneite : toutes les vitesses sont lues depuis un snapshot pre-pass (pas le monde live), resultat independant de l'ordre d'iteration. Contrat nav : si BattlefieldGrid installee, un dodge dont le segment de deplacement (pos -> pos + vel * dt) traverserait une cellule BLOCKED (DDA grid walk) est rejete integralement -- pas de landing dans un mur ni de saut par-dessus.

**Composants** :
- Core : `Position`, `Velocity`, `Acceleration`
- Crowd : `CrowdAgent` (tag), `Team`, `MoveSpeed`, `Target`, `DesiredDirection`, `Health`, `AttackRange`, `AttackDamage`, `AttackCooldown`, `BattleGoal`, `EngageRadius`, `Separation`, `LocalAvoidance`, `BehaviorLod`
- Config : `BehaviorLodConfig` (seuils LOD + battle center)

**DemoPresets** : scenes crowd preconfigurees (small skirmish, shield wall, etc.). Chaque preset definit spawn counts, positions, configs.

**DebugControls** : pause/resume, step-by-step, preset switching. Gere l'etat debug du runtime (paused, step_once, current_preset).

**Benchmark** : harness de benchmark headless avec stress presets. Mesure frame budget, tick timing, agent throughput. Resultats comparables.

## Render (Source/Render/)

**Frame extraction** :
- `RenderFrame` : snapshot lecture seule de la foule (positions, equipe, LOD, HP, direction, engaged). Extrait depuis le World apres le dernier tick du frame.
- `CrowdRenderItem` : donnees par agent (x, y, team_id, lod_tier, health_pct, dir_x, dir_y, engaged).
- Direction : Velocity > DesiredDirection > (0,1). Orientation stable a l'arret.
- `has_target` : vrai si `Target.has_target` dans l'ECS (signal tactique, pas velocity).
- `extract_render_frame()` : itere les CrowdAgent du World, remplit RenderFrame. Cap a k_max_render_agents (4096). Deterministe (meme World = meme frame).

**Renderer DX12 minimal** :
- Device DX12 (feature level 11_0, fallback WARP).
- Swap chain double-buffered, flip-discard.
- Command queue direct, allocator unique, synchrone (CPU wait GPU chaque frame).
- Root signature : 16 root constants (matrice ortho 4x4).
- PSO : triangle list, VS/PS compiles a l'init via D3DCompile (HLSL inline).
- Deux geometries statiques : quad (world debug + overlay) et kite (agents crowd).
- Kite = forme fleche/losange 4 verts (nose, right wing, tail, left wing) orientee par dir.
- InstanceData 40 bytes : pos, half_size, color, dir (float2). Rotation 2D dans le VS.
- Couleur par equipe (rouge, bleu, vert, jaune), modulee par HP, boost 1.15x si has_target.
- Taille par LOD tier : T0=1.0x, T1=0.9x, T2=0.8x, T3=0.7x de k_half_size (0.3).
- Projection orthographique centree (50 unites demi-largeur).
- Pas de depth, pas de MSAA, pas de textures.

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

**RenderCamera** : camera orthographique centree. Matrice view-projection generee a partir de demi-largeur et aspect ratio. Utilisee par le renderer et le culling.

**ViewCulling** : frustum culling CPU en espace ortho. Filtre les CrowdRenderItem hors du viewport avant soumission GPU. Applique cote render uniquement (ne modifie pas la RenderFrame).

**WorldDebugPass** : rendu de la grille de debug world (battlefield grid, axes). Pass separee avant crowd et overlay.

**DebugHud** : HUD debug structure multi-section. `DebugHudData` avec sections titrees (Runtime, Crowd, Budget, Controls). Trois modes : `Hidden`, `Compact` (1 panneau condense), `Full` (4 panneaux). Toggle H=show/hide, Tab=compact/full. Genere des pixel-quads via `generate_hud_instances()`. Titres en ambre, contenu en vert, fond sombre par panneau.

**Integration Engine** :
- `update_presentation()` extrait la RenderFrame.
- `render()` extrait l'overlay puis soumet au Renderer DX12.
- Si le Renderer echoue a l'init, le moteur continue en mode headless.

## Platform (Source/Platform/)

`Window` : wrapper Win32 (HWND, message pump, is_open, width/height). Ouvre une fenetre 1280x720.

## Tests (Tests/)

Framework custom minimal : macro `check(expr)` avec compteurs `g_pass`/`g_fail`. Pas de framework externe. Chaque test est un executable independant enregistre via `add_test()` dans CMake.
