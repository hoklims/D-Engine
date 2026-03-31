# D-Engine 2.0 Execution Plan

## But immediat

Transformer la branche `de-engine-2.0` en base de travail propre pour la
nouvelle generation du moteur.

## Snapshot d'execution

Etat au 2026-04-01:

- l'etape de cadrage est terminee,
- le socle runtime est largement pose,
- la simulation crowd existe deja sous forme jouable headless,
- la navigation battlefield obstacle-aware existe deja en version statique,
- le premier LOD comportemental crowd est en place,
- le premier broadphase melee crowd est en place,
- la preuve minimale de simulation existe deja via hash par tick,
- les budget contracts et la premiere reponse budget-aware sont en place,
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
- `WorldView` et `CommandBuffer`,
- hash de simulation par tick + historique recent,
- comparaison headless de sequences de hash,
- budget contracts runtime explicites.

Ce qui manque encore:

- job system,
- allocateurs temps reel,
- replay complet des inputs,
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
- telemetry LOD coherente post-tick,
- reponse budget-aware progressive avec hysteresis.

Ce qui reste a faire dans cette etape:

- avoidance plus credible,
- capture/replay complet au-dela du hash,
- budgets gameplay plus riches et plus proches du ressenti joueur.

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

1. ouvrir le pipeline visuel crowd-first:
   extraction de frame, DX12 minimal, premiere visibilite runtime;
2. rendre la foule visible a l'ecran:
   rendu crowd-first, batching, premiers LOD visuels;
3. poursuivre le durcissement simulation:
   avoidance plus credible, replay complet des inputs, telemetry gameplay plus riche.

## Criteres de pilotage

Le travail sur la branche 2.0 doit toujours repondre a l'une de ces questions:

- est-ce que cela rapproche le moteur d'une bataille jouable,
- est-ce que cela reduit une incertitude technique majeure,
- est-ce que cela ameliore la preuve de performance,
- est-ce que cela ameliore la reproductibilite.

Si la reponse est non, la tache est probablement secondaire.
