cmake_minimum_required(VERSION 2.6)

PROJECT(pdemo C)

INCLUDE (FindGZIP.cmake)

INCLUDE(version.cmake)

MESSAGE(STATUS "Building PDEMO version ${PDEMO_VERSION}")

CONFIGURE_FILE(config.h.cmake config.h)


SET(pdemo_src pdemo.c)
SET(pdemo_exe pdemo)
SET(pdemo_man_src pdemo_en.1 pdemo_fr.1)
STRING(REGEX REPLACE ".1" ".1.gz" pdemo_man_gz "${pdemo_man_src}")

ADD_DEFINITIONS(-g -O2 -fsigned-char -Wall -W -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-qual -Winline -Werror)

# Compression of the manuals
FOREACH(man ${pdemo_man_src})
  ADD_CUSTOM_COMMAND(OUTPUT ${man}.gz
                     COMMAND ${GZIP_TOOL} -c ${man} > ${man}.gz
                     DEPENDS ${man}
                     COMMENT "Building ${man}.gz"
                    )
ENDFOREACH(man)

# Build the manuals
ADD_CUSTOM_TARGET(man ALL DEPENDS ${pdemo_man_gz})

# Build of the program
ADD_EXECUTABLE(${pdemo_exe} ${pdemo_src})


# Installation of the program
INSTALL(TARGETS pdemo
        DESTINATION "bin"
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

# Installation of the manuals
INSTALL(FILES pdemo_fr.1.gz
        DESTINATION "share/man/fr/man1"
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ RENAME pdemo.1.gz)
INSTALL(FILES pdemo_en.1.gz
        DESTINATION "share/man/man1"
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ RENAME pdemo.1.gz)

# Get the user identifier
EXECUTE_PROCESS(COMMAND id -u
                OUTPUT_VARIABLE USER_ID)

MESSAGE("The user id is ${USER_ID}")

# We change the rights accesses only if we are running as super user
IF(${USER_ID} EQUAL 0)
  INSTALL(SCRIPT pdemo_chown.cmake)
ENDIF(${USER_ID} EQUAL 0)
