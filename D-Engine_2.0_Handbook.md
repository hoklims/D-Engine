# D-Engine 2.0 Handbook (Single Source of Truth)

> Ce document est la source de verite de la branche 2.0.
>
> Il remplace, pour cette generation, le role du handbook actuel.
> Les autres docs 2.0 detaillent et executent cette vision; ils ne doivent pas la contredire.

Last updated: 2026-03-31

-------------------------------------------------------------------------------

## Comment lire ce document

D-Engine 2.0 n'est pas une iteration cosmetique. C'est un changement de nature.

La 1.x a valide plusieurs intuitions justes:

- le determinisme comme outil de production,
- la lisibilite des couts,
- la discipline d'architecture,
- l'ambition crowd-first.

La 2.0 garde ces intuitions, mais refuse la derive vers un moteur generaliste.

Lecture en quatre couches:

1. vision produit,
2. positionnement et promesse,
3. principes d'architecture,
4. execution.

Regle de coherence:

- ce handbook est la source de verite,
- si code, roadmap et docs divergent, c'est un defaut de pilotage.

-------------------------------------------------------------------------------

## Table des matieres

0. [Pourquoi D-Engine 2.0 existe](#0-pourquoi-d-engine-20-existe)
1. [La these 2.0](#1-la-these-20)
2. [Ce que D-Engine 2.0 ne sera pas](#2-ce-que-d-engine-20-ne-sera-pas)
3. [Les promesses du produit](#3-les-promesses-du-produit)
4. [Le moteur que nous construisons](#4-le-moteur-que-nous-construisons)
5. [Le slice de verite](#5-le-slice-de-verite)
6. [Le positionnement strategique](#6-le-positionnement-strategique)
7. [Decisions fondatrices deja actees](#7-decisions-fondatrices-deja-actees)
8. [Non-negociables de la 2.0](#8-non-negociables-de-la-20)
9. [Doctrine d'architecture](#9-doctrine-darchitecture)
10. [Doctrine simulation, rendu, animation](#10-doctrine-simulation-rendu-animation)
11. [Doctrine outillage, replay, budgets](#11-doctrine-outillage-replay-budgets)
12. [Doctrine d'execution](#12-doctrine-dexecution)
13. [Roadmap de generation](#13-roadmap-de-generation)
14. [Definition de reussite](#14-definition-de-reussite)

-------------------------------------------------------------------------------

## 0. Pourquoi D-Engine 2.0 existe

La 1.x a prouve que l'intention etait juste, mais qu'un moteur ne devient pas
reel en accumulant des sous-systemes propres.

Le moteur devient reel quand il rend possible une experience qu'on ne maitrise
pas aussi bien ailleurs.

D-Engine 2.0 existe pour franchir cette ligne.

Formule directrice:

**faire de D-Engine un moteur crowd-first de nouvelle generation, concu pour des
batailles de foule lisibles, stables, deterministes et massives.**

-------------------------------------------------------------------------------

## 1. La these 2.0

Un Musou moderne n'est pas d'abord un probleme de rendu.
C'est un probleme de:

- simulation de foule,
- animation de masse,
- lisibilite de combat,
- stabilite de frame-time.

Le moteur n'est pas:

- un moteur 3D generaliste,
- un concurrent frontal d'Unreal sur tous les fronts,
- un socle editor-first,
- un laboratoire de purete architecturale.

Le moteur est:

- crowd-first,
- battle-first,
- programmer-first,
- cost-visible,
- mesurable en charge reelle.

-------------------------------------------------------------------------------

## 2. Ce que D-Engine 2.0 ne sera pas

- Pas un moteur universel.
- Pas un moteur pour designers en premier.
- Pas une vitrine technologique dispersee.
- Pas un musee de backends.
- Pas un moteur qui generalise trop tot.

Nous voulons etre avant-gardistes, mais pas impressionnistes.

-------------------------------------------------------------------------------

## 3. Les promesses du produit

D-Engine 2.0 est:

- `Crowd-first`
- `Replay-first`
- `GPU-first`
- `Budget-first`
- `Programmer-first`

`Budget-first` signifie que le moteur rend visibles:

- le cout CPU,
- le cout GPU,
- le cout memoire,
- la frequence de mise a jour,
- et la qualite degradee autorisee.

-------------------------------------------------------------------------------

## 4. Le moteur que nous construisons

Nous construisons un moteur specialise pour:

- action de foule,
- arenes a forte pression d'ennemis,
- centaines a milliers d'agents partageant le meme espace,
- stabilite de frame-time avant pic de FPS,
- lisibilite et sensation de controle malgre la densite.

Le moteur doit orchestrer:

- agents interactifs complets,
- agents simplifies,
- representations lointaines,
- VFX d'impact,
- collisions de melee,
- transitions d'animation partagees,
- instrumentation continue.

-------------------------------------------------------------------------------

## 5. Le slice de verite

Un seul vertical slice compte au debut.

Contenu minimal:

- arene lisible,
- heros complet,
- 2 a 3 familles d'ennemis,
- vagues croissantes,
- centaines d'agents simultanes,
- milliers de representations visuelles en LOD ou far-field,
- impacts, stagger et morts,
- budgets CPU et GPU visibles,
- capture et relecture deterministe.

Le slice n'est pas une demo marketing. C'est l'epreuve de verite du moteur.

-------------------------------------------------------------------------------

## 6. Le positionnement strategique

Gagner tres fort sur un cas precis vaut mieux qu'etre moyen partout.

La promesse n'est pas:

- plus flexible,
- plus generique,
- plus complete.

La promesse est:

**plus lisible, plus stable et plus massif sur les batailles de foule.**

L'avance doit venir de:

- la simulation dense,
- l'animation de masse,
- un pipeline GPU de foule,
- et un outillage replay et budget tres solide.

-------------------------------------------------------------------------------

## 7. Decisions fondatrices deja actees

- Windows-first.
- C++23 pour le coeur moteur.
- DirectX 12 et HLSL.
- Tick fixe.
- Simulation decouplee du rendu.
- Replay-first.
- Budgets explicites.
- ECS archetypal.
- SoA.
- Identite stable pour les sorties paralleles.
- Pipeline crowd GPU-first.
- GPU compute skinning.
- Hi-Z, GPU-driven et indirect d'abord.
- Work Graphs comme axe d'evolution, pas comme dependance de phase 0.
- VAT pour le far-field.

-------------------------------------------------------------------------------

## 8. Non-negociables de la 2.0

- Le moteur sert le wedge, pas l'inverse.
- La densite d'agents est un objectif de premier ordre.
- Le frame-time est une fonctionnalite.
- Le replay est natif.
- Les degradations sont prevues.
- Le contenu est contraint en faveur du batching.
- Une seule voie de verite par phase.

-------------------------------------------------------------------------------

## 9. Doctrine d'architecture

Slice-first, puis engineization.

Nous n'extrayons une abstraction qu'apres avoir prouve qu'elle sert le slice.

Le runtime est centre bataille:

- agents,
- groupes,
- navigation,
- avoidance,
- collisions,
- animation de foule,
- soumission GPU,
- budgetisation.

L'architecture doit rester utile sous charge reelle.

Elle doit aussi rendre simple la reponse a quatre questions:

- qu'est-ce qui coute,
- pourquoi,
- a quel tick,
- et est-ce reproductible.

-------------------------------------------------------------------------------

## 10. Doctrine simulation, rendu, animation

### Simulation

La simulation est:

- fixe,
- ordonnee,
- parallele mais deterministe dans ses sorties,
- dominee par des donnees chaudes compactes,
- degradable par tiers comportementaux.

### Navigation et crowd motion

La foule est pensee comme une masse orientee par:

- objectifs communs,
- pression locale,
- trafic de proximite,
- hierarchie de decision.

### Animation

- le heros peut couter cher,
- la foule privilegie partage, clips reutilises, frequence variable,
- skinning GPU et representations lointaines.

### Rendu

Le rendu est pense pour des armees, pas des pieces uniques:

- batching agressif,
- peu de permutations,
- preparation runtime,
- culling GPU,
- dispatch coherent,
- chemin stable sous charge.

-------------------------------------------------------------------------------

## 11. Doctrine outillage, replay, budgets

La 2.0 n'a pas besoin d'un gros editeur d'abord.
Elle a besoin d'outils de verite.

Priorites:

- capture et relecture d'inputs,
- hash de simulation par tick,
- timeline de budgets,
- affichage des tiers de LOD,
- compteur d'agents actifs,
- budget VFX,
- budget collisions,
- budget animation,
- budget soumission GPU,
- histogramme de frame-time.

-------------------------------------------------------------------------------

## 12. Doctrine d'execution

Ordre cible:

1. etablir la verite produit,
2. verrouiller le runtime de simulation,
3. verrouiller le runtime de rendu de foule,
4. rendre le slice jouable,
5. industrialiser seulement ce qui a prouve sa valeur.

Ordre refuse:

- architecture parfaite,
- multiplication des sous-systemes,
- generalisation,
- puis recherche tardive d'un cas d'usage.

-------------------------------------------------------------------------------

## 13. Roadmap de generation

- Phase 0: refondation canonique
- Phase 1: runtime noyau
- Phase 2: crowd runtime
- Phase 3: crowd rendering
- Phase 4: vertical slice
- Phase 5: engineization utile

Checkpoint actuel:

- Phase 0 terminee
- Phase 1 largement en place
- Phase 2 en cours avance
- Phase 3 a 5 non engagees

En clair:

- le moteur headless crowd-first existe deja,
- la foule sait cibler, se deplacer, se separer, combattre et contourner des
  obstacles statiques,
- le runtime crowd sait aussi degrader son cout via un premier LOD
  comportemental a 4 tiers, pilote par un centre de bataille explicite,
- le rendu crowd-first, le replay complet, les jobs et le broadphase melee
  dedie restent devant nous.

-------------------------------------------------------------------------------

## 14. Definition de reussite

La reussite sera atteinte quand nous aurons:

- un vertical slice crowd-first existant,
- une simulation rejouable et verifiable,
- une densite d'agents non cosmetique,
- un rendu et une animation qui tiennent sous charge,
- des degradations controlees,
- des budgets visibles,
- une proposition de valeur immediatement lisible.

La reussite sera un moteur capable de faire vivre une bataille de foule que l'on
peut jouer, mesurer, rejouer, pousser, et comprendre.
