# WARLINK 1.0.0 — par STARZISMIK

Version personnalisée de CB Servers Launcher v3.6.1, commit
2579d365a2168c61dff6271c1e54bab67f606c45.

Les textes de marque, le titre Windows, les raccourcis et les informations de
version ont été adaptés. La mise en page, les illustrations, les icônes et les
fonctions de jeu proviennent du projet original. Le logo de WARLINK est un W carré vert pomme. Le thème optionnel STARZISMIK reprend la disposition tactique avec des fonds
gris foncé neutre et un vert #69D875. Les thèmes d’origine, dont le tactique
orange par défaut, sont conservés. La barre de titre affiche par STARZISMIK
en gris avec une police plus petite.

Les données WARLINK utilisent un dossier distinct `warlink`. Les liens du
launcher utilisent `warlink://`. Les adresses des fournisseurs et des services
CB Servers restent celles de l'amont : ce fork n'est pas une infrastructure
indépendante. Les crédits des auteurs et les licences ont été conservés.

Les mises à jour automatiques du launcher sont désactivées pour éviter qu'une
version CB ne remplace WARLINK et ses textes. Le bouton de vérification explique
cette limitation. Les mises à jour des jeux/clients restent celles de l'amont.
WARLINK ne possède pas le certificat de signature de CB Servers.

## Utilisation de la version portable

Extraire l'archive entière puis lancer `WARLINK.exe`. Conserver le dossier
`warlink` à côté de l'exécutable : il contient les bibliothèques et l'interface.
Ne pas déplacer uniquement l'exécutable. Les installations de jeux existantes
peuvent être sélectionnées depuis le launcher.

## Jeux déjà installés avec CB Launcher

À chaque démarrage, WARLINK recherche la configuration CB dans
`%LOCALAPPDATA%/cbservers`, dans un dossier `cbservers` voisin de son exécutable,
et sur le Bureau. Il récupère les chemins de jeux valides, les sélections de
composants/clients et les réglages de lancement manquants. Les réglages déjà
définis dans WARLINK restent prioritaires, y compris une installation retirée
explicitement. Les caches de manifestes/clients manquants sont copiés localement.

Les jeux restent dans leurs dossiers existants : aucune copie des gros fichiers
de jeu et aucune réinstallation n'est nécessaire. Les fichiers de CB Launcher
ne sont pas modifiés. Les comptes, jetons et préférences générales CB ne sont
pas importés. Plutonium garde sa configuration partagée habituelle.

Pour une installation CB portable ailleurs, ajouter à un raccourci :
`-cb-library-root "X:\chemin\cbservers"`.
Un diagnostic sans lancement de jeu est disponible avec `-library-selftest` :
il génère `warlink/library-check.json` en mode portable. Il contrôle les chemins,
manifestes et exécutables disponibles, sans garantir une partie en jeu.

Sur le PC vérifié, World at War, Black Ops, MW2 Remastered, MWR et HorizonMW
ont des chemins valides. Le client autonome h1-mod de MWR manque dans le dossier
actuel ; HorizonMW est présent. Les mises à jour des clients restent disponibles
au lancement en ligne, comme dans CB Launcher.

## Compilation des sources

Prérequis : Windows, Git, Visual Studio 2022 avec C++ et SDK Windows, 7-Zip.
Les scripts d'origine téléchargent CEF et Discord Social SDK.

L'archive des sources inclut les sources des sous-modules. Si elle est utilisée
hors d'un dépôt Git, initialiser d'abord son historique local (les métadonnées
de version du système de compilation s'appuient sur Git) :

```powershell
git init
git add .
git -c user.name=WARLINK -c user.email=build@example.invalid commit -m "WARLINK 1.0.0 sources"
git tag v1.0.0
.\tools\premake5.exe vs2022
```

Ouvrir `build/cb-launcher.sln` et compiler en Release / x64. Le résultat est
`build/bin/x64/Release/WARLINK.exe`. Pour le lancement portable, placer le runtime
CEF compilé dans `warlink/data/cef/release`, le runtime Discord dans
`warlink/data/discord` et `src/launcher-ui` dans `warlink/data/launcher-ui`, puis
créer un fichier vide `warlink/portable.marker`.

## Licence et origine

GPL-3.0 : voir `LICENSE`. Modifications WARLINK : 30 octobre 2026.
Sources originales : https://github.com/CBServers/cb-launcher/tree/v3.6.1
CB Servers et les auteurs cités dans le README original conservent leurs crédits.

## Langue des contenus BO3
Dans Mods > Black Ops 3 > Installés, cliquer Langue EN / FR. Choisir la langue source et celle du jeu, puis Appliquer (jeu fermé). Même système que T7 Workshop : renommage des préfixes des fichiers .ff et .xpak à la racine et un niveau en dessous, sans traduction. Les fichiers cibles existants sont préservés ; refaire la conversion inverse pour revenir en arrière. Les mises à jour Steam ou du contenu peuvent rétablir les noms anglais.
Les téléchargements WARLINK sont installés dans le dossier du jeu/usermaps/<id> ou mods/<id>. Les abonnements Steam restent dans steamapps/workshop/content/311210/<id>. Le dossier cbl_workshop du jeu est un espace temporaire de téléchargement.


## Correctifs serveur et logo fourni
Le total, les places humaines et les bots se synchronisent. Zéro bot désactive la difficulté. Les anciens profils trop remplis sont normalisés pour préserver une place humaine. Une seconde ouverture sans lien ne provoque plus le message WARLINK is already running. Logo géométrique blanc fourni par STARZISMIK adapté à la barre latérale et aux icônes Windows.


## Mes serveurs et messages en jeu
Serveurs > HorizonMW affiche les profils locaux indépendamment du référencement officiel, avec accès Créer / gérer. Le gestionnaire configure le message de bienvenue, un lien Discord, le réaffichage à chaque réapparition et une annonce périodique (minimum 10 secondes). Enregistrer puis redémarrer le serveur pour charger les réglages.
Le choix Public lance le serveur mais ne fournit pas de clé officielle HMW et ne prouve pas son accessibilité Internet. Le port doit être autorisé et redirigé en TCP et UDP. Documentation : https://docs.horizonmw.org/hmw-game-server-setup-guide-dedicated/ . Sans clé, utiliser la connexion directe. La liste Mes serveurs n'est pas une preuve du référencement Internet.


## Communauté WARLINK
Annuaire indépendant hébergé sur Railway, projet WARLINK, service warlink-community. Dans Serveurs > HorizonMW : Guide réseau, Corriger la visée, Communauté WARLINK et Rejoindre. Dans le gestionnaire : démarrer un profil Public, attendre prêt, puis Publier sur WARLINK. Les ports TCP et UDP sont vérifiés depuis Railway. Aucun mot de passe RCON n'est envoyé. Retirer de WARLINK masque l'annonce ; un serveur qui ne répond plus disparaît après environ 100 secondes.
Nouvelles options : hardcore, radar permanent, sprint, troisième personne, collisions, régénération, délai de réapparition, vies et manches. Messages dans le chat, au centre ou dans un bandeau gauche. Correction de visée : cg_fovScale à 1, FOV conservé, fichier sauvegardé. Crédits GPL et composants d'origine conservés.


## Présentation des serveurs et nom coloré
La liste HorizonMW est repliée sous Afficher les serveurs HorizonMW. La communauté WARLINK reste au-dessus. Mes serveurs affiche Actif uniquement quand getinfo confirme sv_running=1 ; Chargement pour un processus qui ne répond pas encore, Arrêté sinon. Rejoindre un serveur communautaire vérifie une réponse réseau récente avant de lancer le jeu.
Le gestionnaire permet de sélectionner chaque lettre du nom (Ctrl pour sélection multiple), appliquer une couleur HMW et prévisualiser le résultat. Les codes ^0 à ^9 sont conservés dans le nom transmis au jeu et rendus sans HTML arbitraire dans WARLINK. Enregistrer le profil nécessite d'arrêter le serveur ; redémarrer applique le nouveau nom. L'annuaire le récupère à sa vérification suivante.


## Actualisation communautaire
La section Communauté WARLINK dispose d'un bouton Actualiser et se rafraîchit toutes les 15 secondes lorsque la page Serveurs HorizonMW est visible. Elle se met aussi à jour au retour dans la fenêtre et immédiatement après une publication ou un retrait. Une réponse réseau ancienne ne peut plus écraser une réponse plus récente ; une panne temporaire conserve la dernière liste avec un état d'erreur. Les autres utilisateurs doivent disposer de cette version pour bénéficier de l'actualisation automatique.


## Gestionnaire HorizonMW

La communauté WARLINK utilise un port TCP/UDP distinct par serveur simultané. Une règle 27018 ne couvre pas 27017. Le bouton Aide réseau / publication affiche le port du profil sélectionné. Aucun réglage de box n’est modifié automatiquement.

Les cartes sont détectées dans les fastfiles installés (paire carte + chargement) et hmw-usermaps. La sélection propose recherche, origine MWR/MW2/MW3, cases à cocher et rotation ordonnée. Les aperçus empaquetés du jeu ne sont pas encore extraits.

Équipement bots : classes normales, sniper, mêlée sans munitions (arme encore visible), sans arme. Appliquer les bots permet de changer les réglages pendant la partie. Le rééquilibrage déplace uniquement les bots, sans déplacer de force les humains. Le bandeau Milieu à gauche correspond à l’exemple fourni. Le changement du véritable tag de clan des bots déjà connectés n’est pas pris en charge par cette version.

Validation native : node tests/hmw-options.cjs lance un serveur local isolé sur 27020 dans le dossier de jeu et vérifie chargement réel, HUD, équipements, rééquilibrage, RCON et changement de carte. Le script de production est restauré à la fin.
