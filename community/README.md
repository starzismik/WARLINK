# WARLINK Community — par STARZISMIK

Annuaire indépendant du référencement officiel HorizonMW. Node 22, sans dépendance externe. Déploiement Railway : Dockerfile, PORT=3000, volume /data, une seule instance.

Projet Railway : WARLINK (dd97f2b9-1610-4eda-99f9-c000d1416cc6)
Service : warlink-community
API : https://warlink-community-production.up.railway.app
GET /health ; GET /v1/servers ; POST /v1/publish ; POST /v1/unpublish.

Publication : l’adresse vient du X-Real-IP posé par Railway. L’hébergeur prouve son contrôle via une empreinte temporaire dans sv_motd placée par RCON ; le mot de passe RCON ne quitte jamais le launcher. Un jeton propriétaire aléatoire est conservé dans directory-owner.json et son empreinte dans l’annuaire. Les requêtes TCP et UDP sont effectuées depuis Railway. Les données publiques sont filtrées. Revalidation toutes les 30 s ; exclusion après 100 s sans réponse ; suppression après 24 h hors ligne. Polling séquentiel destiné à une petite communauté.

Déployer uniquement derrière le proxy Railway qui fixe X-Real-IP. Ne pas exposer directement le serveur HTTP ailleurs sans adapter la politique de proxy. Une seule instance pour le fichier JSON. Garder les sauvegardes du volume privées.

Le propriétaire clique Publier sur WARLINK une fois le serveur prêt et public. Le serveur reste vérifié lorsque le launcher se ferme. Une nouvelle IP publique demande une nouvelle publication. Le retrait retire seulement de WARLINK.
