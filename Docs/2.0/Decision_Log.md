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
