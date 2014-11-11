cmake_minimum_required(VERSION 2.6)

PROJECT(pdip C)

INCLUDE (FindGZIP.cmake)

INCLUDE(version.cmake)

MESSAGE(STATUS "Building PDIP version ${PDIP_VERSION}")

CONFIGURE_FILE(config.h.cmake config.h)


SET(pdip_src pdip.c)
SET(pdip_exe pdip)
SET(pdip_man_src pdip_en.1 pdip_fr.1)
STRING(REGEX REPLACE ".1" ".1.gz" pdip_man_gz "${pdip_man_src}")

ADD_DEFINITIONS(-g -O2 -fsigned-char -Wall -W -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-qual -Winline -Werror)

# Compression of the manuals
FOREACH(man ${pdip_man_src})
  ADD_CUSTOM_COMMAND(OUTPUT ${man}.gz
                     COMMAND ${GZIP_TOOL} -c ${man} > ${man}.gz
                     DEPENDS ${man}
                     COMMENT "Building ${man}.gz"
                    )
ENDFOREACH(man)

# Build the manuals
ADD_CUSTOM_TARGET(man ALL DEPENDS ${pdip_man_gz})

# Build of the program
ADD_EXECUTABLE(${pdip_exe} ${pdip_src})


# Installation of the program
INSTALL(TARGETS pdip
        DESTINATION "bin"
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

# Installation of the manuals
INSTALL(FILES pdip_fr.1.gz
        DESTINATION "share/man/fr/man1"
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ RENAME pdip.1.gz)
INSTALL(FILES pdip_en.1.gz
        DESTINATION "share/man/man1"
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ RENAME pdip.1.gz)

# Get the user identifier
EXECUTE_PROCESS(COMMAND id -u
                OUTPUT_VARIABLE USER_ID)

MESSAGE("The user id is ${USER_ID}")

# We change the rights accesses only if we are running as super user
IF(${USER_ID} EQUAL 0)
  INSTALL(SCRIPT pdip_chown.cmake)
ENDIF(${USER_ID} EQUAL 0)
