# Compiler le jeu pour Windows, depuis Linux.
#
#     cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake \
#           -DCMAKE_PREFIX_PATH=/mnt/Data/Works/qt6/6.8.2/mingw_64 \
#           -DQT_HOST_PATH=/usr
#     cmake --build build-win
#
# Deux Qt sont en jeu, et c'est la seule chose à comprendre ici: celui de
# Windows, qui est ce contre quoi on lie (CMAKE_PREFIX_PATH), et celui de la
# machine, dont on ne prend que les outils -- moc, rcc, uic -- qui doivent
# tourner ici et non là-bas (QT_HOST_PATH). Qt exige que les deux portent la
# même version.
#
# Le compilateur est celui de Debian (paquet g++-mingw-w64-x86-64-posix), et
# c'est volontairement la variante -posix: Qt officiel est bâti sur les fils
# POSIX, et mêler les deux modèles de fils dans un même binaire ne se voit
# qu'à l'exécution.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(MINGW_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${MINGW_PREFIX}-gcc-posix)
set(CMAKE_CXX_COMPILER ${MINGW_PREFIX}-g++-posix)
set(CMAKE_RC_COMPILER ${MINGW_PREFIX}-windres)

# Les bibliothèques et les en-têtes se cherchent du côté de Windows; les
# programmes à lancer, ici. Les paquets des deux côtés, parce que Qt-Windows
# est hors de ce préfixe-là et arrive par CMAKE_PREFIX_PATH.
set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# Où Debian range les trois DLL du compilateur. Le paquet Windows les emporte
# avec lui: Qt officiel est bâti avec un autre mingw que celui-ci, et c'est la
# libstdc++ la plus récente des deux qui sert les deux.
set(MINGW_RUNTIME_DIR /usr/lib/gcc/${MINGW_PREFIX}/14-posix)
set(MINGW_SYSROOT_LIB /usr/${MINGW_PREFIX}/lib)
