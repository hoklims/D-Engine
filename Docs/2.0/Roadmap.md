# D-Engine 2.0 Roadmap

## Ligne directrice

La roadmap suit une logique simple: rendre inevitable un vertical slice
crowd-first convaincant.

## Phase 0 - Cadre 2.0

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

Objectif:

- rendre possible une boucle fiable et mesurable.

Livrables:

- tick fixe,
- ECS archetypal,
- jobs,
- memoire,
- telemetry,
- replay minimal,
- DX12 minimal.

Critere de sortie:

- benchmark fondation stable,
- capture reproductible,
- premiers couts visibles.

## Phase 2 - Noyau de foule

Objectif:

- prouver que la simulation de masse tient debout.

Livrables:

- flow fields,
- avoidance locale,
- broadphase,
- combat simple,
- LOD comportemental,
- extraction crowd vers le rendu.

## Phase 3 - Pipeline visuel crowd-first

Objectif:

- rendre la masse lisible et scalable.

Livrables:

- skinning GPU,
- culling GPU,
- batching crowd,
- first pass VAT,
- pipeline de materiaux crowd strict.

## Phase 4 - Vertical slice jouable

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
