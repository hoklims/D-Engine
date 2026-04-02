# D-Engine 2.0 Roadmap

## Ligne directrice

La roadmap suit une logique simple: rendre inevitable un vertical slice
crowd-first convaincant.

## Etat au 2026-04-02

Resume:

- Phase 0: terminee
- Phase 1: largement en place, mais encore incomplete
- Phase 2: en cours avance
- Phase 3: en cours avance
- Phase 4 a 5: non demarrees

Ce qui existe deja dans la branche `de-engine-2.0`:

- bootstrap Win32 minimal,
- runtime a tick fixe,
- timeline et telemetry CPU,
- ECS archetypal custom,
- mutations structurelles differees,
- pipeline ordonnee de systemes fixes,
- bootstrap crowd configurable,
- ciblage spatial via `SpatialGrid`,
- combat simultane avec morts differees,
- separation locale,
- battle goals + `EngageRadius`,
- navigation battlefield via grille statique + BFS integration field,
- LOD comportemental a 4 tiers avec telemetry dediee,
- centre LOD explicite, independant de l'origine monde,
- broadphase melee dedie avec telemetry de combat,
- hash de simulation par tick + historique recent + comparaison headless,
- budget contracts explicites par tick,
- reponse budget-aware progressive sur le LOD strategique,
- extraction de `RenderFrame`,
- renderer DX12 minimal,
- camera ortho auto-framee,
- crowd instanciee debug,
- world debug pass,
- overlay debug runtime + HUD structure,
- silhouette crowd orientee,
- culling CPU render-side avec `RenderFrame` pre-cull preserve,
- `RenderStats`,
- debug controls runtime pour pause, step, reset et switch de scene,
- presets demo + stress,
- benchmark headless structurel / budget,
- avoidance locale TTC avec biais lateral, snapshot de vitesses et nav safety,
- couverture de tests dediee (`CrowdTest`, `NavTest`, `LodTest`,
  `MeleeTest`, `SimHashTest`, `BudgetTest`, `BudgetResponseTest`,
  `RenderFrameTest`, `CameraTest`, `InstanceTest`, `RenderStatsTest`,
  `DebugControlsTest`, `EngineRuntimeTest`, `DebugHudTest`,
  `HudContractTest`, `BenchmarkTest`, `CullingTest`, `AvoidanceTest`).

Ce qui manque encore avant de sortir du bloc runtime/crowd:

- job system,
- allocateurs temps reel,
- capture/replay complet des inputs,
- rendu crowd encore plus presentable,
- presentation/demo polish,
- perf renderer active-path plus serieuse,
- culling GPU.

## Phase 0 - Cadre 2.0

Statut: termine

Objectif:

- fixer la vision,
- figer les decisions de base,
- reorganiser la documentation,
- ouvrir la branche 2.0 proprement.

Livrables:

- handbook 2.0,
- specs fondatrices,
- points d'entree du repo mis a jour.

## Phase 1 - Fondation runtime

Statut: en cours avance

Objectif:

- rendre possible une boucle fiable et mesurable.

Livrables atteints:

- boucle Win32 minimale,
- `EngineConfig`, `Clock`, `FixedStep`,
- `FrameInfo` et `FrameTelemetry`,
- ECS archetypal custom (`EntityPool`, `Archetype`, `World`, `WorldView`),
- `CommandBuffer`,
- `SimState` et pipeline de systemes fixes,
- hash de simulation par tick + historique,
- comparaison headless de sequences de hash,
- budget contracts runtime explicites,
- batterie de tests runtime et ECS.

Livrables encore ouverts:

- jobs,
- memoire,
- telemetry GPU,
- replay complet des inputs.

Critere de sortie:

- benchmark fondation stable,
- capture reproductible,
- premiers couts visibles.

## Phase 2 - Noyau de foule

Statut: en cours avance

Objectif:

- prouver que la simulation de masse tient debout.

Livrables atteints:

- ciblage nearest-enemy via `SpatialGrid`,
- `BattleGoal` et `EngageRadius`,
- navigation obstacle-aware via `BattlefieldGrid`,
- separation locale soft,
- LOD comportemental a 4 tiers, avec gating des systemes strategiques,
- telemetry crowd et LOD coherent post-tick,
- broadphase melee dedie et telemetre,
- degradation budget-aware progressive avec hysteresis,
- avoidance locale TTC anticipee avec snapshot de vitesses,
- garde-fou nav segmentaire pour l'avoidance battlefield,
- combat simple avec resolution simultanee,
- scenes de test crowd et battlefield.

Livrables encore ouverts:

- capture/replay complet au-dela du hash.

## Phase 3 - Pipeline visuel crowd-first

Statut: demarree

Objectif:

- rendre la masse lisible et scalable.

Livrables atteints:

- extraction de `RenderFrame` depuis l'etat publie,
- renderer DX12 minimal,
- camera ortho auto-framee avec mode manuel debug,
- crowd instanciee debug,
- world debug pass (sol, grille, axes),
- overlay debug runtime + HUD structure,
- silhouette crowd orientee en kite,
- culling CPU render-side,
- `RenderStats`,
- build interne testable (`pause`, `step`, `reset`, `switch scene`,
  presets demo/stress, HUD),
- stabilisation du lifecycle fenetre/tests runtime.

Livrables encore ouverts:

- rendu crowd plus presentable qu'une simple silhouette debug,
- presentation/demo polish,
- perf renderer active-path plus serieuse,
- culling GPU,
- skinning compute,
- first pass VAT,
- pipeline de materiaux crowd strict.

## Phase 4 - Vertical slice jouable

Statut: non demarree

Objectif:

- transformer le moteur en preuve produit.

Livrables:

- une arene,
- un heros,
- 2 a 3 classes d'ennemis,
- boucles de combat,
- VFX d'impact,
- metriques de reference,
- replays de demonstration.

## Phase 5 - Longueur d'avance

Statut: non demarree

Objectif:

- exploiter les sujets avant-gardistes qui donnent un vrai avantage.

Pistes:

- Work Graphs,
- continuum crowds,
- VFX GPU batches,
- ordonnancement temporel des updates lointaines,
- amelioration du pipeline far-field.

Regle:

aucun pari avance n'entre dans le coeur du moteur sans gain mesure.

## Prochain verrou recommande

Le prochain lot utile n'est plus l'ouverture du pipeline visuel crowd-first.
Elle existe deja. Le verrou suivant est de rendre la build plus lisible comme
demo interne, puis de durcir le chemin renderer actif sous charge.

Le chemin recommande a court terme est:

1. presentation/demo polish,
2. perf renderer active-path,
3. puis replay/tooling plus riches.
