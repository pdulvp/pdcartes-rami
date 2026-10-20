# Read by cpack as it runs, which is after the build: by then this build has
# stamped itself with the time it was made, and that stamp is what separates
# one package of a version from the next. Setting it in CMakeLists would
# freeze it at the moment the project was configured, which may be weeks of
# builds ago.

if(EXISTS "${CPACK_BUILD_VERSION_FILE}")
    file(READ "${CPACK_BUILD_VERSION_FILE}" _ramisol_build)
    string(STRIP "${_ramisol_build}" _ramisol_build)
    if(_ramisol_build)
        set(CPACK_DEBIAN_PACKAGE_RELEASE "${_ramisol_build}")
        # Le paquet Debian porte l'empreinte dans son numéro de version, que
        # dpkg comprend; une archive n'a pas de numéro de version, elle n'a
        # qu'un nom -- alors l'empreinte y va, à la même place et dans le même
        # ordre: ramisol-1.0.0-261020201232-windows-x86_64.zip.
        if(CPACK_GENERATOR STREQUAL "ZIP")
            set(CPACK_PACKAGE_FILE_NAME
                "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${_ramisol_build}-windows-x86_64")
        endif()
    endif()
endif()
