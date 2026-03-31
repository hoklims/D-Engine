# D-Engine 2.0 Architecture Blueprint

## But

Definir la forme cible de D-Engine 2.0 comme moteur from scratch specialise
crowd-first.

## Vue d'ensemble

Le moteur est organise autour d'un runtime a tick fixe et d'une separation nette
entre:

- la decision de simulation,
- l'orchestration du travail,
- la representation visuelle,
- la preuve et l'outillage.

Le CPU decide. Le GPU absorbe l'echelle.

## Snapshot actuel

Etat reel de la branche au 2026-03-31:

- runtime Win32 minimal en place,
- tick fixe, timeline et telemetry CPU en place,
- ECS archetypal custom en place,
- `WorldView` et `CommandBuffer` en place,
- `SimState` orchestre deja une pipeline crowd complete,
- ciblage nearest-enemy via `SpatialGrid`,
- navigation strategique via `BattleGoal`,
- navigation obstacle-aware via `BattlefieldGrid`,
- separation locale soft en place,
- LOD comportemental a 4 tiers en place,
- centre LOD explicite en place,
- broadphase melee dedie en place,
- combat simultane avec morts differees en place,
- tests dedies runtime, ECS, crowd, navigation, LOD et melee en place.

Ce qui reste hors du code aujourd'hui:

- job system,
- allocateurs temps reel,
- replay/hash de simulation,
- rendu DX12 et extraction de frame crowd-first.

## Couches

### Runtime Core

Responsabilites:

- boucle principale,
- temps,
- jobs,
- memoire,
- telemetry,
- replay,
- benchmarks.

### Simulation Core

Responsabilites:

- ECS archetypal,
- navigation,
- avoidance,
- crowd logic,
- combat,
- animation logique,
- LOD comportemental.

### Render Core

Responsabilites:

- extraction de frame,
- skinning compute,
- culling GPU,
- submission indirecte,
- VAT,
- VFX crowd-scale.

### Asset and Tooling Core

Responsabilites:

- import,
- preprocessing,
- catalogues PSO,
- baking VAT,
- captures replay,
- overlays de perf.

## Boucle cible

Boucle actuellement implementee:

1. `begin_frame`,
2. accumulation fixed-step,
3. `SimState::tick`,
4. apply des commandes differees,
5. snapshot runtime et telemetry,
6. `render` placeholder,
7. `end_frame`.

Pipeline crowd actuellement implementee:

1. `SelectTargets`,
2. `ClassifyLod`,
3. `ComputeBattleGoal`,
4. `ComputeDesiredMove`,
5. `ApplyCrowdSteer`,
6. `ApplySeparation`,
7. `MeleeBroadphase`,
8. `AttackTargets`,
9. `ResolveDamage`,
10. `RemoveDead`,
11. `IntegrateVelocity`,
12. `IntegratePosition`.

Boucle cible a moyen terme:

1. lire les entrees du tick,
2. mettre a jour objectifs et flow fields,
3. executer les jobs de simulation,
4. resoudre deplacement, collisions et combat,
5. produire l'etat d'animation logique,
6. extraire la frame,
7. laisser le GPU absorber skinning, culling et LOD,
8. capturer metriques et hashes.

## Frontiere CPU / GPU

Le CPU garde:

- l'identite des agents,
- les regles de combat,
- le determinisme,
- les budgets de simulation.

Le GPU prend:

- skinning,
- visibilite,
- submission,
- LOD visuels,
- representation far-field,
- VFX de masse quand c'est rentable.

## Architecture de donnees

### ECS

Choix cible:

- archetype ECS,
- stockage SoA,
- composants compacts,
- requetes specialisees hot path.

### Jobs

Choix cible:

- workers fixes,
- dependances explicites,
- resultats ecrits dans des emplacements stables.

### Memoire

Choix cible:

- allocateur temps reel,
- frame arenas par thread,
- pools types,
- zero allocation cachee sur le hot path.

## Pipeline crowd-first

### Navigation

- `BattlefieldGrid` statique + BFS integration field aujourd'hui,
- `SpatialGrid` pour le nearest-enemy aujourd'hui,
- contrat fail-safe explicite quand la navigation n'a pas de solution,
- flow fields plus riches pour les masses ensuite,
- regles locales pour casser les impasses ensuite,
- officiers et exceptions hors du flux principal si necessaire.

### Avoidance

- separation locale soft aujourd'hui,
- voisinage borne via grille aujourd'hui,
- LOD comportemental crowd aujourd'hui,
- ORCA ou variante compatible budget plus tard,
- precision degressive avec la distance plus tard.

### Animation

- heros: animation riche,
- proximite: skinning GPU complet,
- distance moyenne: LOD animation,
- lointain: VAT,
- tres lointain: representation encore plus simple si necessaire.

### Combat

- selection de cible spatiale aujourd'hui,
- broadphase melee spatial dedie aujourd'hui,
- aggregation d'evenements de degats aujourd'hui,
- morts differees aujourd'hui,
- capsules et volumes simples ensuite,
- reaction groupee ensuite,
- budget explicite pour la physique spectaculaire.

## Choix avant-gardistes surveilles

- D3D12 Work Graphs,
- continuum crowds,
- temporal smearing controle,
- batching VFX entierement GPU,
- outillage automatique de divergence.

Ils n'entrent dans le coeur qu'apres gain mesure.
