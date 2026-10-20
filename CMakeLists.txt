cmake_minimum_required(VERSION 3.16)

# The version, in one place and in the plainest form there is: three numbers
# in a file, which the bump tasks move on and which nothing has to parse out
# of source code.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" RAMISOL_VERSION)
string(STRIP "${RAMISOL_VERSION}" RAMISOL_VERSION)

project(ramisol VERSION ${RAMISOL_VERSION} LANGUAGES CXX)

# So that bumping it is enough: the next build reconfigures rather than
# carrying yesterday's number.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

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
execute_process(COMMAND ${CMAKE_COMMAND} -DVERSION=${RAMISOL_VERSION}
    -DOUT_CPP=${BUILD_VERSION_CPP} -DOUT_TXT=${BUILD_VERSION_TXT} -P ${GENERATE_VERSION})

add_custom_target(build-version ALL
    BYPRODUCTS ${BUILD_VERSION_CPP} ${BUILD_VERSION_TXT}
    COMMAND ${CMAKE_COMMAND} -DVERSION=${RAMISOL_VERSION}
        -DOUT_CPP=${BUILD_VERSION_CPP} -DOUT_TXT=${BUILD_VERSION_TXT} -P ${GENERATE_VERSION}
    COMMENT "Stamping the build with the time it was made")
set_source_files_properties(${BUILD_VERSION_CPP} PROPERTIES GENERATED TRUE)

add_executable(ramisol
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
add_dependencies(ramisol build-version)

target_link_libraries(ramisol PRIVATE cartotheque Qt6::Widgets)

install(TARGETS ramisol RUNTIME DESTINATION bin)

# L'arène : le moteur et les cerveaux sans l'écran, pour mesurer.
#
# Elle ne s'installe pas et ne se lance pas toute seule -- c'est un instrument,
# qu'on fait tourner avant et après avoir touché à Brain.cpp pour voir si les
# manches finissent encore. Elle ne lie que Qt6::Core, ce qui est la preuve
# qu'elle cherche: tout ce qu'elle compile ne sait pas qu'il y a une fenêtre.
add_executable(ramisol-arene EXCLUDE_FROM_ALL
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

target_link_libraries(ramisol-arene PRIVATE Qt6::Core)

# So a desktop shell can tell which application a window belongs to. The
# file's name must match the app_id the application sets with
# setDesktopFileName().
install(FILES data/ramisol.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

# The Debian package: `cmake --build build --target deb`.
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "ramisol")
set(CPACK_PACKAGE_VENDOR "ramisol")
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
set(CPACK_PACKAGE_VERSION "${RAMISOL_VERSION}")
include(CPack)

# One step from a working tree to a package.
add_custom_target(deb
    COMMAND ${CMAKE_COMMAND} -E env RAMISOL_KEEP_VERSION=1
        ${CMAKE_CPACK_COMMAND} -G DEB --config ${CMAKE_CURRENT_BINARY_DIR}/CPackConfig.cmake
    DEPENDS ramisol
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Building the Debian package")
