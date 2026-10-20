---
status: complete
updated: 2026-10-20 20:56
---

# Plan — zip Windows avec les DLL de Qt (2026-10-20)

But: `cmake --build build-win --target zip` produit
`ramisol-<version>-windows-x86_64.zip` contenant l'exe, les DLL de Qt, les
greffons et les trois DLL du compilateur.

État de départ: CMakeLists.txt (non commité) contient déjà les règles
`install()` Windows + CPack ZIP; vérifié que tous les chemins existent et que
la fermeture des dépendances DLL est complète (objdump: rien hors système
n'est manquant; pas de Qt6Svg dans le kit, et QIcon::fromTheme ne trouve rien
sous Windows de toute façon -- les boutons portent leur mot).

Étapes:
1. [x] Vérifier les chemins Qt/mingw et les imports DLL (objdump).
2. [x] CMakeLists.txt: ajouter la cible `zip` (pendant de la cible `deb`),
       dépendant de ramisol, lançant cpack -G ZIP sur CPackConfig.cmake.
3. [x] Configurer build-win (cmake -B build-win ... ) -- en série, premier plan.
4. [x] Construire la cible zip -- en série, premier plan, attendre la fin.
       Premier essai tué (exit 137, mémoire): ramisol.exe a bien été reconstruit
       (20:52), cpack s'est arrêté en route. Reprise: lancer cpack seul, l'exe
       étant à jour.
5. [x] Vérifier le contenu de l'archive (unzip -l).

Aussi fait, à la demande: le nom de l'archive porte l'empreinte du build comme
le paquet Debian -- cmake/CPackOptions.cmake, branche ZIP.

Fait: ramisol-1.0.0-261020205247-windows-x86_64.zip, 15 entrées, vérifié.
