# D-Engine 2.0 -- Project Instructions

## Langue
Reponses en francais. Code, commits, variables, noms de fichiers en anglais.

## Branche
Tout le travail 2.0 se fait sur `de-engine-2.0`. Ne jamais modifier `main`.

## Source de verite
- Handbook : `D-Engine_2.0_Handbook.md`
- Specs : `Docs/2.0/INDEX.md`
- Decisions : `Docs/2.0/Decision_Log.md`

## Stack
- C++23, MSVC, CMake 3.28+
- Win32 API (fenetre/platform)
- DirectX 12 + HLSL (futur)
- Namespace `de::`

## Build / Test / Run
```bash
cmake --preset default
cmake --build Build --config Debug
ctest --test-dir Build --build-config Debug
./Build/Source/Debug/DEngine.exe
```

## Conventions code
- `/W4 /WX /permissive-` -- zero warnings
- ASCII only dans les sources
- Pas de sur-ingenierie, pas de code speculatif
- Pas de legacy -- tout est from scratch
- Fonctions pures par defaut, effets de bord isoles

## Commits
Format conventionnel : `feat:` / `fix:` / `refactor:` / `docs:` / `test:` / `chore:`
Toujours atomiques. Diff propre.

## Yoyo -- Intelligence de code AST

Yoyo tourne dans un conteneur Docker (`yoyo:latest`, montage `-v .:/repo`).
L'index AST est deja construit -- les outils read-indexed sont prets.

### Regles obligatoires
- Ne JAMAIS passer de chemin Windows (`H:/...`, `C:/...`) aux outils yoyo
- Appeler `boot` et `index` en parallele sur premier contact (deja fait si cette session)
- Chemins internes : `/repo` ou `/repo/sous-dossier`

### Quand utiliser yoyo (PRIORITAIRE sur Grep/Glob/Read bruts)

| Intention | Outil yoyo | Pourquoi |
|---|---|---|
| Trouver un symbole (fonction, classe, struct) | `inspect` | Resolution AST exacte, pas de faux positifs textuels |
| Chercher du code par intent ou mot-cle | `search` | Recherche semantique dans l'index AST |
| Comprendre un fichier ou une zone de code | `inspect` | Montre structure + contexte sans bruit |
| Poser une question sur le code | `ask` | Repond en s'appuyant sur l'AST, pas du grep naif |
| Lister les fichiers du projet | `map` | Carte structuree par scope, mieux que glob brut |
| Voir les routes/points d'entree | `routes` | Detecte entry points automatiquement |
| Analyser l'impact d'un changement | `impact` | Trace les dependances (callers, callees, refs) |
| Juger un changement avant de l'appliquer | `judge_change` | Ownership, invariants, risque de regression |
| Verifier la sante globale du code | `health` | Metriques de complexite, couplage, couverture |
| Appliquer une modification safe | `change` | Edit avec bornes d'erreur, mieux que Edit brut pour refactors |
| Enchainer plusieurs lectures | `script` | Compose plusieurs outils en un seul appel |

### Quand NE PAS utiliser yoyo
- Lecture d'un fichier specifique deja connu → `Read` direct
- Recherche d'une chaine exacte dans 1-2 fichiers → `Grep` direct
- Operations git, build, tests → `Bash` direct

## OCO Headless -- Protocole obligatoire

Chaque requete de travail DOIT suivre le protocole OCO en mode headless (sans
appels dashboard/emit_events MCP). Aucun raccourci, aucun champ saute.

### 1. Classify
- Type (feat, fix, refactor, test, docs, chore)
- Complexite (low, medium, high)
- Routing (implementation directe, refactor, bug, trace)
- Raison courte

### 2. Plan Exploration
Evaluer au minimum 2 candidats avec TOUS les champs :
- strategy (speed / safety / autre)
- step_count
- estimated_tokens
- verify_count
- parallel_groups
- team (null ou composition d'agents)
- score (0.0 - 1.0)

Pour chaque candidat, decrire l'approche en une ligne.
Declarer le winner avec justification.

#### Regles contraignantes sur parallel_groups et team

`parallel_groups` et `team` sont des ENGAGEMENTS, pas des souhaits.
Le plan annonce doit correspondre exactement a l'execution reelle.

**parallel_groups** = nombre de groupes de steps qui s'executent en parallele.
- Si `parallel_groups: 1` → tout est sequentiel, pas de sub-agent obligatoire.
- Si `parallel_groups: N` (N > 1) → le plan DOIT contenir des steps avec le
  meme `group` ID, et ces steps DOIVENT etre lances en parallele via l'outil
  Agent (un sub-agent par step du meme groupe).
- Un step `mode: agent` DOIT etre execute via l'outil Agent (sub-agent).
- Un step `mode: inline` DOIT etre execute dans le contexte principal.
- Ne JAMAIS annoncer `parallel_groups: N > 1` puis tout executer en sequence.
  Si la parallelisation n'apporte rien, annoncer `parallel_groups: 1`.

**team** = composition de sub-agents specialises.
- Si `team: null` → tout le travail est fait par le contexte principal
  (eventuellement avec des sub-agents Explore/researcher ponctuels).
- Si `team` est definie (ex: `[implementer, verifier]`) → les roles declares
  DOIVENT etre remplis par des sub-agents lances via l'outil Agent avec le
  `subagent_type` correspondant :
  - `researcher` → Agent(subagent_type=Explore ou researcher)
  - `implementer` → Agent(subagent_type=general-purpose, mode=bypassPermissions)
  - `verifier` → Agent(subagent_type=patch-verifier ou code-reviewer)
  - `architect` → Agent(subagent_type=architect)
- Ne JAMAIS annoncer une team puis faire tout le travail inline.

**Coherence plan/execution** :
- Si a l'execution on realise que la parallelisation est inutile, le declarer
  explicitement : "Plan ajuste : parallel_groups 2 → 1, raison : {motif}".
- Toute deviation du plan doit etre annoncee AVANT l'execution du step concerne.

### 3. Plan Generated
Table de steps avec :
- ID (S1, S2, ...)
- Step name
- Description
- Role (researcher, implementer, verifier)
- Mode (inline, agent)
- Group (G1, G2, ... -- steps du meme groupe s'executent en parallele)
- Depends (quels steps/groupes precedents)
- Verify (yes/no -- gate de verification apres ce step)
- Est. tokens
- Model (-- si default, sinon haiku/sonnet si step simple)

Le nombre de groupes distincts DOIT correspondre a `parallel_groups`.
Les steps d'un meme groupe ne doivent avoir aucune dependance entre eux.

### 4. Execute
Pour chaque groupe :
- Si le groupe contient 1 step → executer inline ou agent selon `mode`
- Si le groupe contient N steps → lancer N appels Agent EN PARALLELE
  (un seul message avec N tool calls Agent)
- Pour chaque step : afficher "### S{N} : {name}" avant, "S{N} done." apres
- Les steps agent retournent leur resultat au contexte principal qui
  synthetise et passe au groupe suivant

### 5. Verify Gate
Apres chaque step marque verify=yes :
- Build Debug (+ Release si step final)
- Tests existants
- Zero warnings
- Afficher table de resultats

### 6. Complete (Run Stopped)
- Total steps et total fichiers
- Resume court
- Fichiers crees/modifies (table)
- Commandes exactes build/test/run
- Hash du commit (si commit demande)
- Decisions prises
- Decisions reportees
- Deviations du plan (si le plan a ete ajuste en cours de route)
