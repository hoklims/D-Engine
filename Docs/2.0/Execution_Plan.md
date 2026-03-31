# D-Engine 2.0 Execution Plan

## But immediat

Transformer la branche `de-engine-2.0` en base de travail propre pour la
nouvelle generation du moteur.

## Etape 1 - Cadrage

- installer le handbook 2.0,
- creer les specs fondatrices,
- rediriger `README.md`, `AGENTS.md`, `Docs/INDEX.md` et `Roadmap.md`,
- garder les documents historiques comme archive.

## Etape 2 - Socle runtime

- definir l'arborescence cible du code 2.0,
- creer les dossiers et points d'entree du runtime,
- poser les interfaces minimales du moteur 2.0,
- introduire le premier benchmark fondation.

## Etape 3 - Simulation de foule

- ECS,
- jobs,
- memoire,
- navigation de masse,
- avoidance,
- broadphase,
- combat simple.

## Etape 4 - Rendu crowd-first

- DX12 minimal,
- upload et extraction de frame,
- skinning compute,
- culling,
- submission,
- LOD visuels,
- VAT.

## Etape 5 - Vertical slice

- environnement de bataille reduit,
- personnage principal,
- ennemis,
- feedback de combat,
- capture de metriques,
- capture replay.

## Criteres de pilotage

Le travail sur la branche 2.0 doit toujours repondre a l'une de ces questions:

- est-ce que cela rapproche le moteur d'une bataille jouable,
- est-ce que cela reduit une incertitude technique majeure,
- est-ce que cela ameliore la preuve de performance,
- est-ce que cela ameliore la reproductibilite.

Si la reponse est non, la tache est probablement secondaire.
