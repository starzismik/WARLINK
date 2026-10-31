# WARLINK — dossier de développement

Dossier canonique : D:\[ Applications ]\GitHub\WARLINK
Application à tester : app/WARLINK.exe
Sources : src/ ; dépendances et licences : deps/ ; outils locaux : dev/.

Compiler : node dev/build.cjs
Mettre à jour et rouvrir l’unique application : powershell -NoProfile -ExecutionPolicy Bypass -File dev/update-single.ps1

Préparer un paquet propre après validation : scripts/prepare-release.ps1 (RAR et ZIP). Le script ne publie rien. Il copie uniquement le binaire, les dépendances et ressources nécessaires, sans user, servers, jetons, profils ou chemins locaux. Ne jamais joindre app/warlink/user ou app/warlink/servers à une release.

GitHub : https://github.com/starzismik/WARLINK (privé pendant la validation). Ne publier le brouillon v1.0.0 qu’après accord explicite de STARZISMIK et ajout du paquet vérifié. Pour activer les mises à jour publiques, renseigner update.json après disponibilité réelle de l’archive GitHub. Le fichier actuellement released=false ne déclenche aucune mise à jour.

Railway : projet WARLINK, service warlink-community. API https://warlink-community-production.up.railway.app . Déployer depuis community/. Volume persistant /data, PORT=3000, une seule instance.

Les crédits GPL-3.0 sont conservés dans CREDITS.md et LICENSE. Les anciens outils de recherche restent locaux et sont exclus de GitHub.
