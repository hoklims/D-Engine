# D-Engine 2.0 Execution Plan

## But immediat

Transformer la branche `de-engine-2.0` en base de travail propre pour la
nouvelle generation du moteur.

## Snapshot d'execution

Etat au 2026-03-31:

- l'etape de cadrage est terminee,
- le socle runtime est largement pose,
- la simulation crowd existe deja sous forme jouable headless,
- la navigation battlefield obstacle-aware existe deja en version statique,
- le premier LOD comportemental crowd est en place,
- le premier broadphase melee crowd est en place,
- le rendu crowd-first n'a pas encore commence.

## Etape 1 - Cadrage

Statut: termine

- installer le handbook 2.0,
- creer les specs fondatrices,
- rediriger `README.md`, `AGENTS.md`, `Docs/INDEX.md` et `Roadmap.md`,
- garder les documents historiques comme archive.

## Etape 2 - Socle runtime

Statut: largement en place

- definir l'arborescence cible du code 2.0,
- creer les dossiers et points d'entree du runtime,
- poser les interfaces minimales du moteur 2.0,
- introduire le premier benchmark fondation.

Ce qui existe deja:

- Win32 minimal,
- fixed-step runtime,
- timeline et telemetry CPU,
- ECS archetypal,
- pipeline de systemes fixes,
- `WorldView` et `CommandBuffer`.

Ce qui manque encore:

- job system,
- allocateurs temps reel,
- replay/hash,
- DX12 minimal.

## Etape 3 - Simulation de foule

Statut: en cours avance

- ECS,
- jobs,
- memoire,
- navigation de masse,
- avoidance,
- broadphase,
- combat simple.

Ce qui existe deja:

- crowd bootstrap configurable,
- ciblage via `SpatialGrid`,
- `BattleGoal` + `EngageRadius`,
- combat simultane avec morts differees,
- separation locale,
- `BattlefieldGrid` statique + BFS integration field,
- LOD comportemental a 4 tiers avec centre explicite,
- broadphase melee dedie avec telemetry de base,
- telemetry LOD coherente post-tick.

Ce qui reste a faire dans cette etape:

- avoidance plus credible,
- replay/hash de simulation,
- budgets crowd plus explicites.

## Etape 4 - Rendu crowd-first

Statut: non demarree

- DX12 minimal,
- upload et extraction de frame,
- skinning compute,
- culling,
- submission,
- LOD visuels,
- VAT.

## Etape 5 - Vertical slice

Statut: non demarree

- environnement de bataille reduit,
- personnage principal,
- ennemis,
- feedback de combat,
- capture de metriques,
- capture replay.

## Prochaine tranche de travail recommandee

Ordre recommande a court terme:

1. verrouiller la preuve technique:
   replay/hash, divergence detection, captures de reference;
2. borner plus finement le runtime crowd:
   budgets explicites, telemetry plus proche du gameplay, avoidance plus credible;
3. ouvrir le rendu crowd-first:
   extraction de frame, DX12 minimal, premiere visibilite runtime.

## Criteres de pilotage

Le travail sur la branche 2.0 doit toujours repondre a l'une de ces questions:

- est-ce que cela rapproche le moteur d'une bataille jouable,
- est-ce que cela reduit une incertitude technique majeure,
- est-ce que cela ameliore la preuve de performance,
- est-ce que cela ameliore la reproductibilite.

Si la reponse est non, la tache est probablement secondaire.
