# D-Engine 2.0 Systems Spec

## Objectif

Lister les systemes indispensables de D-Engine 2.0, leur role et leur contrat de
performance.

## Snapshot implemente au 2026-04-02

Les blocs suivants existent deja dans la branche:

- runtime a tick fixe,
- `FrameInfo` et `FrameTelemetry`,
- ECS archetypal custom,
- `WorldView` + `CommandBuffer`,
- pipeline ordonnee de systemes fixes,
- runtime crowd configurable,
- ciblage spatial via `SpatialGrid`,
- navigation strategique via `BattleGoal`,
- navigation obstacle-aware via `BattlefieldGrid`,
- separation locale soft,
- LOD comportemental a 4 tiers,
- broadphase melee dedie,
- combat simultane avec morts differees,
- hash de simulation par tick + historique recent,
- budget contracts explicites,
- reponse budget-aware progressive,
- extraction de `RenderFrame`,
- renderer DX12 minimal,
- camera ortho auto-framee,
- crowd instanciee debug,
- world debug pass,
- overlay debug runtime + HUD structure,
- silhouette crowd orientee,
- culling CPU render-side,
- presets demo et stress,
- benchmark headless structurel / budget,
- avoidance locale TTC avec snapshot de vitesses et nav safety,
- `RenderStats`,
- debug controls runtime,
- tests dedies runtime, ECS, crowd, navigation, LOD, melee, hash, budget et
  rendu debug.

Les blocs suivants restent des cibles, pas encore des realites:

- job system,
- allocateurs temps reel,
- capture/replay complet des inputs,
- culling GPU,
- skinning compute,
- presentation/demo polish,
- perf renderer active-path plus serieuse,
- VAT.

## 1. Runtime

### Main Loop

Role:

- piloter le tick fixe,
- decoupler simulation et rendu,
- maintenir un ordre stable.

Contrat:

- une seule source de temps de simulation,
- aucun travail lourd cache dans la presentation.

### Replay

Role:

- enregistrer les entrees et seeds utiles,
- rejouer un scenario,
- comparer les resultats.

Contrat:

- hash de simulation par tick,
- divergence detectee tot,
- mode debug lisible.

Etat:

- implemente en version 1,
- hash de simulation par tick,
- historique recent via ring buffer,
- comparaison headless de sequences de hash,
- capture/replay complet des inputs encore absent.

### Debug Controls

Role:

- transformer l'executable en banc de test interne,
- permettre pause, single-step, reset et switch de scene,
- rendre l'etat courant lisible sans debugger.

Contrat:

- `single-step` = exactement un tick,
- reset et switch de scene repartent sans reliquat fixed-step,
- les toggles one-shot resistent a l'auto-repeat clavier.

Etat:

- implemente en version 1,
- pause, step, reset, switch de scene et camera debug en place,
- presets demo/stress en place,
- feedback runtime via titre de fenetre en place,
- overlay debug runtime et HUD structure en place.

### Benchmark Harness

Role:

- comparer des runs headless,
- figer des presets de stress,
- separer les signaux deterministes des signaux wall-clock.

Contrat:

- comparaison structurelle stable entre deux runs identiques,
- comparaison budget/perf separee,
- zero dependance au renderer.

Etat:

- implemente en version 1,
- presets de stress en place,
- benchmark headless structurel / budget en place.

## 2. ECS

### Storage

Role:

- stocker les agents et objets du hot path de maniere compacte.

Contrat:

- layout favorisant l'iteration massive,
- mutations controlees,
- cout de parcours visible.

Etat:

- implemente.

### Query Layer

Role:

- exposer les ensembles necessaires aux systemes critiques.

Contrat:

- eviter les requetes trop generiques,
- privilegier les chemins specialises foule.

Etat:

- implemente via `WorldView` + `each<Cs...>()`.

## 3. Job System

Role:

- distribuer la simulation,
- respecter les dependances,
- tenir les coeurs disponibles sans casser la reproductibilite.

Contrat:

- sorties indexees de facon stable,
- dependances explicites,
- contention minimale.

Etat:

- non implemente.

## 4. Memory

Role:

- stabiliser les couts d'allocation,
- fournir des espaces transitoires fiables,
- rendre la pression memoire visible.

Contrat:

- pas d'allocation cachee dans le hot path,
- pools types,
- frame arenas par worker,
- budgets memoire observables.

Etat:

- non implemente en tant que sous-systeme dedie.

## 5. Navigation de masse

### Flow Fields

Role:

- guider rapidement de larges groupes vers un objectif partage.

Contrat:

- recalculs rares et maitrises,
- requete agent tres bon marche.

Etat:

- version simple en place via `BattlefieldGrid`,
- grille statique,
- BFS uniform-cost,
- obstacles statiques uniquement.

### Local Avoidance

Role:

- eviter les collisions absurdes de proximite,
- garder du mouvement dans la masse.

Contrat:

- precision plafonnee par budget,
- cout borne par voisinage,
- comportement credible en melee.

Etat:

- separation locale soft + avoidance TTC avec biais lateral,
- vitesses voisines lues depuis un snapshot commun,
- garde-fou nav segmentaire sur battlefield,
- pas encore d'avoidance type ORCA,
- broadphase melee dedie deja separe du simple targeting,
- avoidance melee plus intelligente encore absente.

## 6. LOD comportemental

Role:

- reduire le cout des agents lointains sans casser la bataille.

Contrat:

- les agents proches restent prioritaires,
- la degradation est explicite,
- chaque tier a son budget et sa frequence.

Tiers cibles:

- `T0`: pleine fidelite,
- `T1`: logique simplifiee,
- `T2`: simulation echantillonnee,
- `T3`: representation quasi visuelle.

Etat:

- implemente en version 1,
- 4 tiers a stride `1/2/4/8`,
- agents engages forces en `T0`,
- classification par distance a un centre de bataille explicite,
- gating applique aux systemes strategiques,
- telemetry de tiers et de skips en place,
- reponse budget-aware avec hysteresis en place,
- separation explicite entre etat applique au tick courant et etat decide pour
  le tick suivant.

## 7. Combat

### Broadphase

Role:

- reduire le nombre de tests couteux.

Contrat:

- cout quasi lineaire,
- bon comportement en densite forte.

Etat:

- implemente en version 1,
- broadphase spatial borne via `SpatialGrid`,
- generation de candidats melee dedies,
- telemetry melee de base en place.

### Hit Resolution

Role:

- traiter coups, overlaps, degats et reactions.

Contrat:

- aggregation d'evenements,
- cout plafonne par tick,
- priorite au gameplay lisible.

Etat:

- implemente via broadphase melee puis buffer de hit events,
- resolution simultanee conservee,
- contrat "au plus un hit par agent et par tick" explicite.

### Reaction System

Role:

- convertir les impacts en reponses comprehensibles.

Contrat:

- pas de physique libre generalisee,
- budgets serres pour les cas spectaculaires.

Etat:

- non implemente.

## 8. Animation

### Hero Animation

Role:

- conserver un haut niveau de qualite pour le personnage principal.

Contrat:

- priorite absolue a la lisibilite et au feedback.

### Crowd Animation

Role:

- animer une masse importante a cout maitrise.

Contrat:

- partage agressif de clips,
- skinning GPU,
- baisse progressive de fidelite avec la distance,
- representation far-field dediee.

## 9. Renderer

### Frame Extraction and Debug View

Role:

- projeter un etat de simulation publie vers le rendu sans le polluer,
- garder un chemin headless testable,
- fournir une premiere visibilite runtime.

Contrat:

- extraction read-only depuis l'etat committe,
- aucun pointeur du renderer vers le `World`,
- fallback headless si le renderer n'est pas disponible.

Etat:

- implemente en version 1,
- `RenderFrame` en place,
- `RenderCamera` ortho auto-framee en place,
- `render_frame()` pre-cull preserve,
- build interne testable en place,
- HUD structure en place,
- presentation encore strictement debug.

### Visibility and Submission

Role:

- ne dessiner que ce qui compte,
- reduire le cout CPU de submission.

Contrat:

- culling CPU simple aujourd'hui, GPU ensuite,
- pipeline GPU-driven a terme,
- batching maximal sur la foule.

Etat:

- implemente en version 1,
- un draw instancie crowd en place,
- culling CPU ortho render-side en place,
- `RenderStats` en place,
- pas encore de culling GPU,
- pas encore de pipeline GPU-driven complet.

### PSO and Shader Strategy

Role:

- supprimer le stutter evitable.

Contrat:

- precaching,
- permutations limitees,
- conventions fortes sur les materiaux crowd.

Etat:

- un seul PSO debug en place,
- shaders HLSL minimaux en place,
- pas encore de vrai systeme de materiaux.

### Far-Field

Role:

- conserver la sensation de masse a distance.

Contrat:

- VAT ou equivalent,
- cout CPU quasi nul,
- compatibilite avec le budget global.

Etat:

- non implemente.

## 10. Telemetry and Perf Gates

Role:

- transformer les sensations en chiffres exploitables.

Contrat:

- timings CPU par systeme,
- timings GPU par passe,
- histogrammes de frame-time,
- compteurs par tier de foule,
- scenes de benchmark figees.

Etat:

- timings CPU runtime et par systeme en place,
- compteurs crowd de base en place,
- compteurs navigation de base en place,
- compteurs LOD de base en place,
- compteurs melee de base en place,
- hash de simulation par tick en place,
- budget contracts explicites en place,
- reponse budget-aware visible dans le snapshot en place,
- benchmark headless et presets de stress en place,
- compteurs render debug (`RenderStats`, culling, HUD) en place,
- timings GPU et perf gates non implementes.

## 11. Budgets structurants

Les budgets exacts seront calibres par benchmark, mais les principes sont fixes:

- le heros ne degrade pas avant la foule lointaine,
- la simulation proche prime sur le spectacle lointain,
- le p99 compte plus que le pic isole,
- la degradation doit etre visible dans les metriques avant de devenir visible a
  l'oeil.

Etat:

- contrats de budget runtime en place,
- evaluation par tick en place,
- premiere degradation automatique crowd en place,
- auto-throttle plus riche encore absent.
