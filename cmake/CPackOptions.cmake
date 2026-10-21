# Read by cpack as it runs, which is after the build: by then this build has
# stamped itself with the time it was made, and that stamp is what separates
# one package of a version from the next. Setting it in CMakeLists would
# freeze it at the moment the project was configured, which may be weeks of
# builds ago.
#
# L'empreinte va dans la version, derrière un `~`, et non dans le champ de
# release de Debian. Deux raisons:
#
#  - `~` se classe avant tout, le vide compris: `1.1.0~261021143413` est
#    *antérieur* à `1.1.0`, ce qu'un build de travail est. Dans le champ de
#    release il serait postérieur, et un build de travail installé empêcherait
#    la version de s'installer par-dessus;
#  - le champ de release, c'est la énième mise en paquet d'une même version --
#    une dépendance corrigée, un résumé réécrit. L'y mettre, c'est occuper
#    une case qui sert à autre chose.
#
# La cible release, elle, ne laisse pas d'empreinte du tout: le paquet
# s'appelle `1.1.0` et c'est la version. Rien à faire ici pour cela, le test
# ci-dessous ne trouve qu'une empreinte vide.

if(EXISTS "${CPACK_BUILD_VERSION_FILE}")
    file(READ "${CPACK_BUILD_VERSION_FILE}" _pdcartes_rami_build)
    string(STRIP "${_pdcartes_rami_build}" _pdcartes_rami_build)
    if(_pdcartes_rami_build)
        set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION}~${_pdcartes_rami_build}")
    endif()
endif()

# Le paquet Debian porte sa version dans son nom, que dpkg comprend et sait
# ordonner; une archive n'a pas de version, elle n'a qu'un nom -- alors la
# version y va en entier, empreinte comprise:
# pdcartes-rami-1.1.0~261021143412-windows-x86_64.zip, et
# pdcartes-rami-1.1.0-windows-x86_64.zip pour une version.
if(CPACK_GENERATOR STREQUAL "ZIP")
    set(CPACK_PACKAGE_FILE_NAME
        "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-windows-x86_64")
endif()
