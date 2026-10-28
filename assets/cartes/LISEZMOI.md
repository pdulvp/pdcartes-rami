# Les jeux de cartes livrés avec le programme

Tout fichier `.cartes` déposé ici est emporté par le paquet et installé avec
le programme :

- sous Debian, dans `<préfixe>/share/pdcartes-rami/cartes`, soit
  `/usr/share/pdcartes-rami/cartes` pour le paquet `.deb`, dont le préfixe
  est `/usr` ;
- sous Windows, dans le dossier `cartes` voisin de l'exécutable, à l'intérieur
  de l'archive.

Le programme lit ces dossiers au démarrage, après celui de l'utilisateur
(`~/.config/pdulvp/pdcartes-rami/cartes` sous Linux), et les jeux qui s'y
trouvent apparaissent dans la liste des réglages comme les autres, avec leurs
dos. Ils ne peuvent être ni modifiés ni effacés depuis le programme : un jeu
de l'utilisateur portant le même nom de fichier les masque.

Aucune règle de nommage : l'identifiant d'un jeu est le nom de son fichier, et
son titre affiché est celui qu'il porte à l'intérieur.

Seuls les `*.cartes` sont installés ; ce fichier-ci ne l'est pas.

Les jeux déposés ici sont redistribués par le paquet : leurs images doivent
donc être libres de l'être, et la licence du jeu, que le format sait porter,
doit le dire.
