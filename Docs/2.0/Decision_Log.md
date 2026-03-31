# D-Engine 2.0 Decision Log

## DL-001 - Le moteur reste specialise

Decision:

- D-Engine 2.0 est un moteur crowd-first pour jeux d'action de masse.

Pourquoi:

- c'est la proposition de valeur la plus forte et la plus defendable.

## DL-002 - Windows-first

Decision:

- le scope initial reste Windows-first.

Pourquoi:

- cela reduit la dispersion,
- permet un usage direct de DX12,
- et aligne le moteur avec son wedge.

## DL-003 - C++23 au coeur

Decision:

- le coeur moteur est ecrit en C++23.

Pourquoi:

- maturite,
- controle,
- outillage,
- alignement avec DX12 et les besoins runtime.

## DL-004 - DirectX 12 et HLSL

Decision:

- le rendu cible est construit sur DX12 et HLSL.

Pourquoi:

- Windows-first,
- meilleur acces aux capacites avancees visees,
- chaine de profiling plus directe.

## DL-005 - Archetype ECS et SoA

Decision:

- la simulation de foule repose sur un ECS archetypal avec stockage SoA.

Pourquoi:

- adequation forte avec les agents homogenes,
- excellentes proprietes de parcours et de parallelisation.

## DL-006 - Tick fixe et replay-first

Decision:

- la simulation tourne sur tick fixe avec capture et replay au coeur du design.

Pourquoi:

- reproductibilite,
- analyse,
- regression detectable,
- base saine pour le benchmarking.

## DL-007 - GPU-first pour l'echelle visuelle

Decision:

- la masse visuelle repose sur skinning GPU, culling GPU et pipeline GPU-driven.

Pourquoi:

- l'echelle visee l'impose.

## DL-008 - LOD comportemental de premier rang

Decision:

- le comportement de foule degrade explicitement selon des tiers definis.

Pourquoi:

- la stabilite en charge ne peut pas dependre seulement des LOD visuels.

## DL-009 - Avant-gardisme sous controle

Decision:

- Work Graphs et autres paris avances sont surveilles, mais integres seulement
  apres preuve de gain.

Pourquoi:

- D-Engine 2.0 cherche une longueur d'avance, pas une instabilite inutile.

## DL-010 - CMake comme systeme de build

Decision:

- le systeme de build 2.0 est CMake 3.28+ avec le generateur Visual Studio 2022.

Pourquoi:

- standard industrie pour C++ multi-fichiers,
- support natif de C++23 avec MSVC,
- produit un .sln compatible avec l'IDE Visual Studio,
- simple a etendre avec add_subdirectory quand le moteur grandit,
- presets JSON pour des builds reproductibles.

## DL-011 - Win32 API directe pour la fenetre

Decision:

- la fenetre est creee via Win32 API sans wrapper tiers (pas GLFW, pas SDL).

Pourquoi:

- Windows-first elimine le besoin d'abstraction portable,
- controle total sur la boucle de messages,
- zero dependance externe,
- alignement direct avec le futur swapchain DX12.

## DL-012 - Mutations structurelles differees

Decision:

- les systemes de simulation n'appliquent pas de mutations structurelles
  directement sur le `World`,
- ils passent par `WorldView` et `CommandBuffer`.

Pourquoi:

- iteration stable,
- contrat deterministe,
- base saine pour la parallelisation future.

## DL-013 - Pipeline de simulation crowd ordonnee

Decision:

- la foule tourne dans une pipeline de systemes fixes a ordre explicite.

Pourquoi:

- lisibilite du contrat de tick,
- telemetry par systeme,
- comportement reproductible,
- evolution plus sure vers budgets et LOD.

## DL-014 - Combat simultane a l'echelle du tick

Decision:

- les attaques produisent des evenements,
- les degats sont resolus en lot,
- les morts sont detruites de maniere differee.

Pourquoi:

- independance a l'ordre d'iteration,
- gameplay plus defendable,
- contrat `alive-at-tick-start` explicite.

## DL-015 - Navigation battlefield via grille statique

Decision:

- la navigation strategique repose d'abord sur une grille 2D statique avec BFS
  integration field et flow vectors par equipe.

Pourquoi:

- premier contournement d'obstacles simple et deterministe,
- meilleur fit crowd-first qu'un pathfinding individuel premature,
- base extensible pour des flow fields plus riches plus tard.

## DL-016 - Contrat fail-safe de navigation

Decision:

- quand la grille de navigation est active et que `sample_flow()` echoue
  (agent hors grille, cellule bloquee ou inatteignable), l'agent recoit une
  direction zero au lieu de tomber en fallback ligne directe vers le goal.
- les scenes battlefield invalides (grille 0x0, cell size <= 0) retombent
  sur un bootstrap crowd classique sans navigation.

Pourquoi:

- un fallback silencieux vers la ligne directe permettait de bypasser les
  obstacles sans aucun signal,
- direction zero = fail-safe visible et debuggable via telemetrie
  (`nav_failures_this_tick`),
- validation de la config empeche les scenes absurdes de produire un
  comportement imprevisible.

## DL-017 - Premier LOD comportemental runtime

Decision:

- la foule utilise un premier LOD comportemental a 4 tiers (`T0` a `T3`),
- les agents engages restent forces en `T0`,
- seuls les systemes strategiques et de navigation locale sont gates a ce stade.

Pourquoi:

- il fallait commencer a reduire le cout crowd sans attendre le rendu,
- le gating des systemes les moins critiques donne un premier gain simple,
- cela rend visible la degradation de simulation dans le runtime headless.

## DL-018 - Centre LOD explicite, pas origine monde

Decision:

- la classification LOD ne depend plus de la distance a `(0,0)`,
- elle depend d'un centre de bataille explicite stocke dans `BehaviorLodConfig`.

Pourquoi:

- une bataille translatee dans l'espace ne doit pas changer de tiers LOD,
- l'origine monde n'est pas une notion gameplay defendable,
- cela prepare une future gestion explicite du centre d'interet de bataille.

## DL-019 - Broadphase melee dedie avant la resolution des degats

Decision:

- le melee ne repose plus seulement sur la cible courante et un test de
  distance implicite,
- un broadphase melee dedie prefiltre les paires attaquant/defenseur avant
  `AttackTargets`.

Pourquoi:

- il fallait borner explicitement le cout du combat crowd,
- separer targeting et melee rend le contrat plus lisible,
- cela fournit une telemetry melee propre avant les optimisations suivantes.

## DL-020 - Contrat melee: cible coherente et un seul hit par agent

Decision:

- `AttackTargets` frappe toujours `Target.entity` si cette cible est validee
  par le broadphase,
- un agent n'emet au plus qu'un seul hit par tick, meme avec `interval <= 0`.

Pourquoi:

- le broadphase ne devait pas remplacer la semantique de ciblage,
- un multi-hit implicite par voisin aurait cree une derive gameplay non voulue,
- ce contrat garde le melee simple, deterministe et defendable.
