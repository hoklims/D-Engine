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

- flow fields pour les masses,
- regles locales pour casser les impasses,
- officiers et exceptions hors du flux principal si necessaire.

### Avoidance

- ORCA ou variante compatible budget,
- voisinage plafonne,
- precision degressive avec la distance.

### Animation

- heros: animation riche,
- proximite: skinning GPU complet,
- distance moyenne: LOD animation,
- lointain: VAT,
- tres lointain: representation encore plus simple si necessaire.

### Combat

- capsules et volumes simples,
- aggregation d'evenements,
- reaction groupee,
- budget explicite pour la physique spectaculaire.

## Choix avant-gardistes surveilles

- D3D12 Work Graphs,
- continuum crowds,
- temporal smearing controle,
- batching VFX entierement GPU,
- outillage automatique de divergence.

Ils n'entrent dans le coeur qu'apres gain mesure.
