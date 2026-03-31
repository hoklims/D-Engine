# D-Engine 2.0 Systems Spec

## Objectif

Lister les systemes indispensables de D-Engine 2.0, leur role et leur contrat de
performance.

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

## 2. ECS

### Storage

Role:

- stocker les agents et objets du hot path de maniere compacte.

Contrat:

- layout favorisant l'iteration massive,
- mutations controlees,
- cout de parcours visible.

### Query Layer

Role:

- exposer les ensembles necessaires aux systemes critiques.

Contrat:

- eviter les requetes trop generiques,
- privilegier les chemins specialises foule.

## 3. Job System

Role:

- distribuer la simulation,
- respecter les dependances,
- tenir les coeurs disponibles sans casser la reproductibilite.

Contrat:

- sorties indexees de facon stable,
- dependances explicites,
- contention minimale.

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

## 5. Navigation de masse

### Flow Fields

Role:

- guider rapidement de larges groupes vers un objectif partage.

Contrat:

- recalculs rares et maitrises,
- requete agent tres bon marche.

### Local Avoidance

Role:

- eviter les collisions absurdes de proximite,
- garder du mouvement dans la masse.

Contrat:

- precision plafonnee par budget,
- cout borne par voisinage,
- comportement credible en melee.

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

## 7. Combat

### Broadphase

Role:

- reduire le nombre de tests couteux.

Contrat:

- cout quasi lineaire,
- bon comportement en densite forte.

### Hit Resolution

Role:

- traiter coups, overlaps, degats et reactions.

Contrat:

- aggregation d'evenements,
- cout plafonne par tick,
- priorite au gameplay lisible.

### Reaction System

Role:

- convertir les impacts en reponses comprehensibles.

Contrat:

- pas de physique libre generalisee,
- budgets serres pour les cas spectaculaires.

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

### Visibility and Submission

Role:

- ne dessiner que ce qui compte,
- reduire le cout CPU de submission.

Contrat:

- culling GPU,
- pipeline GPU-driven,
- batching maximal sur la foule.

### PSO and Shader Strategy

Role:

- supprimer le stutter evitable.

Contrat:

- precaching,
- permutations limitees,
- conventions fortes sur les materiaux crowd.

### Far-Field

Role:

- conserver la sensation de masse a distance.

Contrat:

- VAT ou equivalent,
- cout CPU quasi nul,
- compatibilite avec le budget global.

## 10. Telemetry and Perf Gates

Role:

- transformer les sensations en chiffres exploitables.

Contrat:

- timings CPU par systeme,
- timings GPU par passe,
- histogrammes de frame-time,
- compteurs par tier de foule,
- scenes de benchmark figees.

## 11. Budgets structurants

Les budgets exacts seront calibres par benchmark, mais les principes sont fixes:

- le heros ne degrade pas avant la foule lointaine,
- la simulation proche prime sur le spectacle lointain,
- le p99 compte plus que le pic isole,
- la degradation doit etre visible dans les metriques avant de devenir visible a
  l'oeil.
