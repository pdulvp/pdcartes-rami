cmake_minimum_required(VERSION 3.16)

# The version, in one place and in the plainest form there is: three numbers
# in a file, which the release moves on and which nothing has to parse out
# of source code.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PDCARTES_RAMI_VERSION)
string(STRIP "${PDCARTES_RAMI_VERSION}" PDCARTES_RAMI_VERSION)

project(pdcartes-rami VERSION ${PDCARTES_RAMI_VERSION} LANGUAGES CXX)

# So that moving it on is enough: the next build reconfigures rather than
# carrying yesterday's number.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

# En croisant la compilation: les outils de Qt sont des programmes, et un
# programme Windows ne tourne pas sur la machine qui compile.
#
# Le Qt officiel pour Windows est un Qt natif -- bâti là-bas, pour là-bas --
# et ses outils sont donc des .exe. Il ne sait pas non plus qu'on le croise,
# si bien que QT_HOST_PATH, qui est la façon prévue de dire « les outils, va
# les prendre ici », le laisse indifférent. On les lui désigne un par un. Les
# deux Qt portent la même version, ce qui est exigé par ailleurs: le moc
# d'ici produit du code que les en-têtes de là-bas acceptent.
if(CMAKE_CROSSCOMPILING AND QT_HOST_PATH)
    foreach(tool moc rcc uic)
        if(NOT TARGET Qt6::${tool})
            continue()
        endif()
        find_program(QT_HOST_TOOL_${tool} NAMES ${tool}
            HINTS "${QT_HOST_PATH}/lib/qt6/libexec" "${QT_HOST_PATH}/libexec/qt6"
                  "${QT_HOST_PATH}/lib/x86_64-linux-gnu/qt6/libexec"
                  "${QT_HOST_PATH}/bin"
            REQUIRED)
        set_property(TARGET Qt6::${tool} PROPERTY IMPORTED_LOCATION
                     "${QT_HOST_TOOL_${tool}}")
    endforeach()
endif()

# Le format des jeux de cartes, la liste des jeux installés et le style
# partagé. Bibliothèque statique, liée dans le binaire.
add_subdirectory(../cartes-set-creator/lib cartotheque)

# What build this is. Written at each build rather than at each configure, so
# that the version on the command line and on the package is the build in
# front of you; see cmake/GenerateVersion.cmake.
set(BUILD_VERSION_CPP ${CMAKE_CURRENT_BINARY_DIR}/BuildVersion.cpp)
set(BUILD_VERSION_TXT ${CMAKE_CURRENT_BINARY_DIR}/BuildVersion.txt)
set(GENERATE_VERSION ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateVersion.cmake)

# Once now, so the file exists before anything is compiled.
execute_process(COMMAND ${CMAKE_COMMAND} -DVERSION=${PDCARTES_RAMI_VERSION}
    -DOUT_CPP=${BUILD_VERSION_CPP} -DOUT_TXT=${BUILD_VERSION_TXT} -P ${GENERATE_VERSION})

add_custom_target(build-version ALL
    BYPRODUCTS ${BUILD_VERSION_CPP} ${BUILD_VERSION_TXT}
    COMMAND ${CMAKE_COMMAND} -DVERSION=${PDCARTES_RAMI_VERSION}
        -DOUT_CPP=${BUILD_VERSION_CPP} -DOUT_TXT=${BUILD_VERSION_TXT} -P ${GENERATE_VERSION}
    COMMENT "Stamping the build with the time it was made")
set_source_files_properties(${BUILD_VERSION_CPP} PROPERTIES GENERATED TRUE)

add_executable(pdcartes-rami
    ${BUILD_VERSION_CPP}
    src/Version.h
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    # Palette et DialogChrome sont dans la cartothèque.
    src/TileButton.cpp
    src/TileButton.h
    src/ProfileChip.cpp
    src/ProfileChip.h
    src/MenuStyle.cpp
    src/MenuStyle.h
    # The cards themselves: what they are, what they are worth, and how they
    # are drawn. Nothing below this line knows the rules.
    src/Cards.cpp
    src/Cards.h
    src/CardArt.cpp
    src/CardArt.h
    src/DrawnDeck.cpp
    src/DrawnDeck.h
    # The rules: what makes a combination, and the game that is played with
    # them. Nothing here knows there is a screen.
    src/Melds.cpp
    src/Melds.h
    src/Tables.cpp
    src/Tables.h
    src/TableLook.cpp
    src/TableLook.h
    src/Rami.cpp
    src/Rami.h
    # The six around the table: who they are, what they have learnt, and how
    # they decide.
    src/RivalSkills.cpp
    src/RivalSkills.h
    src/Rivals.cpp
    src/Rivals.h
    src/Profiles.cpp
    src/Profiles.h
    src/Brain.cpp
    src/Brain.h
    # L'écran titre, qui est le seuil de la séance, et le logo qu'il partage
    # avec le menu.
    src/Fonts.cpp
    src/Fonts.h
    src/TitleMark.cpp
    src/TitleMark.h
    src/TitleFlight.cpp
    src/TitleFlight.h
    src/TitlePage.cpp
    src/TitlePage.h
    # The table on screen, and the page that runs a game on it.
    src/TableView.cpp
    src/TableView.h
    src/TablePage.cpp
    src/TablePage.h
    src/ScoreBoard.cpp
    src/ScoreBoard.h
    src/VictoryEffect.cpp
    src/VictoryEffect.h
    # The opening menu and the two dialogs.
    src/RivalCard.cpp
    src/RivalCard.h
    src/RivalSheet.cpp
    src/RivalSheet.h
    src/RivalGlance.cpp
    src/RivalGlance.h
    src/SkillRow.cpp
    src/SkillRow.h
    src/TablePicker.cpp
    src/TablePicker.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/NameDialog.cpp
    src/NameDialog.h
    src/HelpDialog.cpp
    src/HelpDialog.h
)

# So the stamp is written before the sources that carry it are compiled.
add_dependencies(pdcartes-rami build-version)

# Les deux fontes d'affichage du logo, dans le binaire.
#
# Dedans et non à côté: le jeu est un fichier, le zip de Windows n'a rien de
# plus à emporter, et une fonte qu'on ne trouve pas au démarrage ne laisse pas
# le logo en Sans.
#
# Un seul appel pour les deux, et non un chacune: le deuxième argument est le
# nom de la ressource, dont est tiré celui du fichier que rcc écrit. Deux
# appels du même nom sur la même cible se disputent le même fichier.
#
# `BASE assets` est ce qui fait que le chemin de la ressource est
# `:/fonts/ZenDots/ZenDots-Regular.ttf` et non `:/fonts/assets/ZenDots/...`:
# le dossier de chaque fonte est gardé, sa licence vivant à côté d'elle.
qt_add_resources(pdcartes-rami "titre"
    PREFIX "/fonts"
    BASE assets
    FILES
        assets/ZenDots/ZenDots-Regular.ttf
        assets/ZenTokyoZoo/ZenTokyoZoo-Regular.ttf
        # Deux des neuf graisses livrées: la fonte de toute la fenêtre veut
        # son normal et son gras, et rien de plus. Chacune pèse presque deux
        # mégaoctets -- une fonte japonaise porte ses milliers d'idéogrammes,
        # qu'on les écrive ou non.
        #
        # Zen Kaku Gothic New reste dans `assets/` sans être embarquée: elle a
        # tenu ce rôle un moment, y revenir est deux lignes, et une fonte
        # compilée dans le binaire que rien ne demande est quatre mégaoctets
        # que tout le monde télécharge.
        assets/M_PLUS_2/static/MPLUS2-Regular.ttf
        assets/M_PLUS_2/static/MPLUS2-Bold.ttf)

# Les icônes des boutons, dans le binaire elles aussi.
#
# Un bureau Linux apporte son thème et le jeu le préfère, pour rester chez lui
# sur la machine de quelqu'un; Windows n'en a pas, et les boutons y étaient
# nus. Celles-ci sont le second recours -- voir `appIcon()` dans MenuStyle.
#
# Elles viennent du thème Adwaita de GNOME: voir assets/icons/PROVENANCE.txt,
# dont la licence et les auteurs s'installent à côté du jeu.
qt_add_resources(pdcartes-rami "icones"
    PREFIX "/icons"
    BASE assets/icons
    FILES
        assets/icons/display-brightness-symbolic.svg
        assets/icons/document-edit-symbolic.svg
        assets/icons/document-open-symbolic.svg
        assets/icons/edit-undo-symbolic.svg
        assets/icons/folder-open-symbolic.svg
        assets/icons/folder-symbolic.svg
        assets/icons/go-home-symbolic.svg
        assets/icons/go-previous-symbolic.svg
        assets/icons/help-about-symbolic.svg
        assets/icons/help-contents-symbolic.svg
        assets/icons/night-light-symbolic.svg
        assets/icons/preferences-system-symbolic.svg
        assets/icons/user-home-symbolic.svg
        assets/icons/view-refresh-symbolic.svg
        assets/icons/weather-clear-night-symbolic.svg
        assets/icons/weather-clear-symbolic.svg)

target_link_libraries(pdcartes-rami PRIVATE cartotheque Qt6::Widgets)

# Sur Windows, un programme à fenêtres et non un programme de console: sans
# cela une console noire s'ouvre derrière le tapis et reste là.
set_target_properties(pdcartes-rami PROPERTIES WIN32_EXECUTABLE ON)

# Là-bas l'exe est à la racine du dossier, avec ses DLL autour -- c'est ce
# qu'attend quelqu'un qui décompresse un jeu; ici, dans bin.
if(WIN32)
    install(TARGETS pdcartes-rami RUNTIME DESTINATION .)
else()
    install(TARGETS pdcartes-rami RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# La licence de chaque fonte voyage avec elle: les deux sont compilées dans le
# binaire, et l'OFL demande que sa licence accompagne ce qui l'emporte. Une par
# fonte, et nommée d'après elle: deux fichiers « OFL.txt » dans le même dossier
# n'en feraient qu'un.
foreach(_font ZenDots ZenTokyoZoo M_PLUS_2)
    # Le dossier porte les tirets bas de chez Google Fonts; le fichier
    # installé porte le nom de la fonte.
    string(REPLACE "_" "" _named "${_font}")
    if(WIN32)
        install(FILES assets/${_font}/OFL.txt DESTINATION .
                RENAME OFL-${_named}.txt)
    else()
        install(FILES assets/${_font}/OFL.txt
                DESTINATION ${CMAKE_INSTALL_DOCDIR}
                RENAME OFL-${_named}.txt)
    endif()
endforeach()

# Et celle des icônes, avec les auteurs du thème dont elles viennent: l'une
# comme l'autre sont demandées par les termes sous lesquels Adwaita les donne.
foreach(_paper LICENSE-CC-BY-SA-3.0 AUTHORS PROVENANCE)
    if(WIN32)
        install(FILES assets/icons/${_paper}.txt DESTINATION .
                RENAME icones-${_paper}.txt)
    else()
        install(FILES assets/icons/${_paper}.txt
                DESTINATION ${CMAKE_INSTALL_DOCDIR}
                RENAME icones-${_paper}.txt)
    endif()
endforeach()

# L'arène : le moteur et les cerveaux sans l'écran, pour mesurer.
#
# Elle ne s'installe pas et ne se lance pas toute seule -- c'est un instrument,
# qu'on fait tourner avant et après avoir touché à Brain.cpp pour voir si les
# manches finissent encore. Elle ne lie que Qt6::Core, ce qui est la preuve
# qu'elle cherche: tout ce qu'elle compile ne sait pas qu'il y a une fenêtre.
add_executable(pdcartes-rami-arene EXCLUDE_FROM_ALL
    src/Arene.cpp
    src/Cards.cpp
    src/Cards.h
    src/Melds.cpp
    src/Melds.h
    src/Tables.cpp
    src/Tables.h
    src/Rami.cpp
    src/Rami.h
    src/RivalSkills.cpp
    src/RivalSkills.h
    src/Rivals.cpp
    src/Rivals.h
    src/Profiles.cpp
    src/Profiles.h
    src/Brain.cpp
    src/Brain.h
)

target_link_libraries(pdcartes-rami-arene PRIVATE Qt6::Core)

# So a desktop shell can tell which application a window belongs to. The
# file's name must match the app_id the application sets with
# setDesktopFileName().
if(UNIX AND NOT APPLE)
    install(FILES data/pdcartes-rami.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
endif()

# ---------------------------------------------------------------------------
# Windows: ce qu'il faut à côté de l'exe, et le zip qui les emporte.
#
# Pas de windeployqt ici, qui est un programme Windows et ne tourne pas sur la
# machine qui compile: la liste est écrite, ce qui a l'avantage de dire tout
# haut de quoi le jeu dépend. Trois DLL de Qt, le greffon de la plateforme --
# sans lui le programme s'arrête en disant qu'il ne trouve pas « windows » --
# celui du style, les formats d'image que la cartothèque sait lire, et les
# trois DLL du compilateur.
if(WIN32)
    # Où Qt a posé ses affaires, dit par Qt lui-même plutôt que deviné.
    set(QT_BIN "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
    set(QT_PLUGINS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}")

    install(FILES
        "${QT_BIN}/Qt6Core.dll"
        "${QT_BIN}/Qt6Gui.dll"
        "${QT_BIN}/Qt6Widgets.dll"
        DESTINATION .)
    # Les icônes que le jeu emporte sont des SVG, et un SVG se lit par un
    # greffon qui dépend de cette bibliothèque-là. Sans elle le greffon ne se
    # charge pas, et les boutons de Windows seraient nus comme avant -- sans
    # un mot, puisqu'une icône absente n'est pas une erreur.
    if(EXISTS "${QT_BIN}/Qt6Svg.dll")
        install(FILES "${QT_BIN}/Qt6Svg.dll" DESTINATION .)
    endif()
    install(FILES "${QT_PLUGINS}/platforms/qwindows.dll" DESTINATION platforms)
    install(FILES "${QT_PLUGINS}/styles/qmodernwindowsstyle.dll" DESTINATION styles)
    install(DIRECTORY "${QT_PLUGINS}/imageformats/" DESTINATION imageformats
            FILES_MATCHING PATTERN "*.dll")

    # Celles du compilateur, quand c'est le mingw de Debian qui a compilé:
    # Qt officiel en dépend autant que le jeu.
    foreach(_runtime libstdc++-6 libgcc_s_seh-1)
        if(EXISTS "${MINGW_RUNTIME_DIR}/${_runtime}.dll")
            install(FILES "${MINGW_RUNTIME_DIR}/${_runtime}.dll" DESTINATION .)
        endif()
    endforeach()
    if(EXISTS "${MINGW_SYSROOT_LIB}/libwinpthread-1.dll")
        install(FILES "${MINGW_SYSROOT_LIB}/libwinpthread-1.dll" DESTINATION .)
    endif()

    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_NAME "pdcartes-rami")
    set(CPACK_PACKAGE_VERSION "${PDCARTES_RAMI_VERSION}")
    # L'archive porte la version et l'empreinte du build, comme le paquet
    # Debian porte la sienne. Le nom se décide au moment de l'empaquetage et
    # non ici -- l'empreinte n'est écrite qu'à la construction, et celle de
    # la configuration a des semaines de builds de retard; voir
    # cmake/CPackOptions.cmake. Ce nom-ci n'est que le recours, si l'empreinte
    # manque.
    set(CPACK_PACKAGE_FILE_NAME "pdcartes-rami-${PDCARTES_RAMI_VERSION}-windows-x86_64")
    set(CPACK_BUILD_VERSION_FILE "${BUILD_VERSION_TXT}")
    set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPackOptions.cmake")
    # L'exe et ses DLL à la racine de l'archive, et non dans un dossier de
    # plus: le dossier, c'est celui où l'on décompresse.
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
    include(CPack)

    # Un pas du dossier de travail à l'archive, comme la cible deb de l'autre
    # côté: `cmake --build build-win --target zip`.
    add_custom_target(zip
        COMMAND ${CMAKE_COMMAND} -E env PDCARTES_RAMI_KEEP_VERSION=1
            ${CMAKE_CPACK_COMMAND} -G ZIP --config ${CMAKE_CURRENT_BINARY_DIR}/CPackConfig.cmake
        DEPENDS pdcartes-rami
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Fabrication de l'archive Windows")
    return()
endif()

# The Debian package: `cmake --build build --target deb`.
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "pdcartes-rami")
set(CPACK_PACKAGE_VENDOR "pdcartes-rami")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Le rami sur tapis vert, à six autour de la table")
# One string, not three: a list would reach the control file with the
# semicolons that join it still in place.
set(CPACK_PACKAGE_DESCRIPTION
    "Le rami français, joué contre cinq adversaires qui apprennent d'une partie
à l'autre : chacun garde son expérience, débutant, intermédiaire ou expert, et
trois profils séparent la vôtre de celle des autres.")
set(CPACK_PACKAGE_CONTACT "pdulvp <pdulvp@laposte.net>")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
# The Qt libraries it was built against, worked out from the binary itself
# rather than written down and left to go stale.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# Debug symbols are for the build tree, not for everyone's disk.
set(CPACK_STRIP_FILES TRUE)
# The icons the buttons along the top wear come out of the desktop's icon
# theme, and a current theme ships them as SVG: without the plugin that reads
# one they are bare. Not a dependency -- every button carries its word as
# well -- but close to one.
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-svg-plugins")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_BUILD_VERSION_FILE "${BUILD_VERSION_TXT}")
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPackOptions.cmake")
# The version is the project's; the build stamp goes on as the Debian
# release, so that two packages of 1.0.0 built on different days are told
# apart and the later one is the newer one.
set(CPACK_PACKAGE_VERSION "${PDCARTES_RAMI_VERSION}")
include(CPack)

# One step from a working tree to a package.
add_custom_target(deb
    COMMAND ${CMAKE_COMMAND} -E env PDCARTES_RAMI_KEEP_VERSION=1
        ${CMAKE_CPACK_COMMAND} -G DEB --config ${CMAKE_CURRENT_BINARY_DIR}/CPackConfig.cmake
    DEPENDS pdcartes-rami
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Building the Debian package")

# ---------------------------------------------------------------------------
# Les deux paquets d'une version, en un pas:
#
#     cmake --build build --target release
#
# Le deb d'ici et le zip de là-bas, tous deux nommés par la version seule --
# pdcartes-rami_1.1.0_amd64.deb, pdcartes-rami-1.1.0-windows-x86_64.zip. Un
# build ordinaire garde son empreinte, lui, derrière un `~` qui le classe
# avant la version vers laquelle il va; voir cmake/CPackOptions.cmake.
#
# Deux arbres, parce que deux compilateurs: un seul cmake ne fabrique pas les
# deux côtés. Celui-ci mène donc l'autre, en l'appelant.
#
# L'absence d'empreinte est à la cible et non à l'arbre: pas d'option de
# configuration qu'on laisse allumée sans le voir et qui donnerait un jour
# deux 1.1.0 que dpkg ne sait pas départager.

# Le Qt de Windows, contre lequel l'autre côté se lie. Vide par défaut: le
# chemin est celui d'une machine, il n'a rien à faire dans le dépôt. Il se
# donne une fois, et le cache de l'arbre le retient:
#
#     cmake -S . -B build -DQT_WIN_PREFIX=/chemin/vers/qt6/6.8.2/mingw_64
set(QT_WIN_PREFIX "" CACHE PATH
    "Le Qt pour Windows contre lequel la cible release lie l'autre côté")
# Celui de la machine, dont la compilation croisée ne prend que les outils.
set(QT_HOST_PREFIX "/usr" CACHE PATH
    "Le Qt de cette machine, dont la cible release prend moc, rcc et uic")

set(WIN_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build-win")

if(NOT QT_WIN_PREFIX)
    # La cible existe quand même, pour que ce soit elle qui le dise plutôt
    # qu'un « No rule to make target ». Ni apostrophe ni chevron dans ces
    # lignes: elles traversent un shell, qui verrait une chaîne restée ouverte
    # dans la première et une redirection dans le second.
    add_custom_target(release
        COMMAND ${CMAKE_COMMAND} -E echo
            "release: QT_WIN_PREFIX manque -- le côté Windows ne peut pas être lié."
        COMMAND ${CMAKE_COMMAND} -E echo
            "À donner une fois pour cet arbre:"
        COMMAND ${CMAKE_COMMAND} -E echo
            "    cmake -S . -B build -DQT_WIN_PREFIX=/chemin/vers/qt6/6.8.2/mingw_64"
        COMMAND ${CMAKE_COMMAND} -E false)
else()

add_custom_target(release
    # 1. L'arbre d'en face, configuré s'il ne l'est pas déjà. Avant d'écrire
    #    les versions: sur un arbre neuf, configurer en écrit une au passage.
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR} -B ${WIN_BUILD_DIR}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/mingw-w64.cmake
        -DCMAKE_PREFIX_PATH=${QT_WIN_PREFIX}
        -DQT_HOST_PATH=${QT_HOST_PREFIX}

    # 2. Les deux versions, sans empreinte. PDCARTES_RAMI_KEEP_VERSION tient
    #    ensuite la cible build-version tranquille, des deux côtés: sans quoi
    #    le build qui suit les réécrirait, horodatées.
    COMMAND ${CMAKE_COMMAND} -DVERSION=${PDCARTES_RAMI_VERSION} -DRELEASE=ON
        -DOUT_CPP=${BUILD_VERSION_CPP} -DOUT_TXT=${BUILD_VERSION_TXT}
        -P ${GENERATE_VERSION}
    COMMAND ${CMAKE_COMMAND} -DVERSION=${PDCARTES_RAMI_VERSION} -DRELEASE=ON
        -DOUT_CPP=${WIN_BUILD_DIR}/BuildVersion.cpp
        -DOUT_TXT=${WIN_BUILD_DIR}/BuildVersion.txt
        -P ${GENERATE_VERSION}

    # 3. Les deux binaires avant le premier paquet. Un Qt mal désigné arrête
    #    tout ici, et il ne reste pas la moitié d'une version sur le disque.
    COMMAND ${CMAKE_COMMAND} -E env PDCARTES_RAMI_KEEP_VERSION=1
        ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target pdcartes-rami
    COMMAND ${CMAKE_COMMAND} -E env PDCARTES_RAMI_KEEP_VERSION=1
        ${CMAKE_COMMAND} --build ${WIN_BUILD_DIR} --target pdcartes-rami

    # 4. Les deux paquets, par les cibles qui les font déjà.
    COMMAND ${CMAKE_COMMAND} -E env PDCARTES_RAMI_KEEP_VERSION=1
        ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target deb
    COMMAND ${CMAKE_COMMAND} -E env PDCARTES_RAMI_KEEP_VERSION=1
        ${CMAKE_COMMAND} --build ${WIN_BUILD_DIR} --target zip

    COMMENT "Les deux paquets de la version ${PDCARTES_RAMI_VERSION}")

endif()
