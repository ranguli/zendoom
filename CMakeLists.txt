set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

cmake_minimum_required(VERSION 3.7.2)
project("Smol Doom" VERSION 3.0.0 LANGUAGES C)

# Autotools variables
#set(top_srcdir ${CMAKE_CURRENT_SOURCE_DIR})
#set(top_builddir ${CMAKE_CURRENT_BINARY_DIR})

# AC_INIT variables
set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_TARNAME "smol-doom")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")

string(REGEX REPLACE " Doom$" "" PACKAGE_SHORTNAME "${PACKAGE_NAME}")
set(PACKAGE_COPYRIGHT "Copyright (C) 1993-2017")
set(PACKAGE_LICENSE "GNU General Public License, version 2")

add_compile_options("-Wall" "-Wextra" "-Wunused-function" "-Wdeclaration-after-statement"
    "-Wno-sign-compare"
    "-Wno-missing-field-initializers"
    "-Wno-incompatible-pointer-types"
                    "-Wredundant-decls")

find_package(SDL2 2.0.7)
find_package(SDL2_mixer 2.0.2)
find_package(SDL2_net 2.0.0)

find_package(m)

include(CheckSymbolExists)
include(CheckIncludeFile)
check_symbol_exists(strcasecmp "strings.h" HAVE_DECL_STRCASECMP)
check_symbol_exists(strncasecmp "strings.h" HAVE_DECL_STRNCASECMP)
check_include_file("dirent.h" HAVE_DIRENT_H)

# Without a hyphen. This is used for the bash-completion scripts.
string(TOLOWER "${PACKAGE_SHORTNAME}" PROGRAM_SPREFIX)

# With a hyphen, used almost everywhere else.
set(PROGRAM_PREFIX "${PROGRAM_SPREFIX}-")

configure_file(cmake/config.h.cin ../config.h)
# Common source files used by absolutely everything:

set(COMMON_SOURCE_FILES
    src/impl/system.c    src/impl/system.h
    src/lib/argv.c       src/lib/argv.h
    src/misc/misc.c      src/misc/misc.h
                               src/lib/type.h
    src/lib/fixed.c            src/lib/fixed.h
    src/lib/sha1.c             src/lib/sha1.h
    )


# Dedicated server (chocolate-server):

set(DEDSERV_FILES
    src/wad/iwad.c          src/wad/iwad.h
    src/game/gamemode.c     src/game/gamemode.h
    src/impl/timer.c        src/impl/timer.h
    src/misc/config.c       src/misc/config.h
    src/net/common.c        src/net/common.h
    src/net/io.c            src/net/io.h
    src/net/packet.c        src/net/packet.h
    src/net/sdl.c           src/net/sdl.h
    src/net/query.c         src/net/query.h
    src/net/server.c        src/net/server.h
    src/net/structrw.c      src/net/structrw.h
    src/mem/native.c        src/mem/zone.h)

#add_executable("${PROGRAM_PREFIX}server" WIN32 ${COMMON_SOURCE_FILES} ${DEDSERV_FILES})
#target_include_directories("${PROGRAM_PREFIX}server"
#                           PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")
#target_link_libraries("${PROGRAM_PREFIX}server" SDL2::SDL2main SDL2::net)

# Source files used by the game binaries (chocolate-doom, etc.)

set(GAME_SOURCE_FILES
    src/game/main.c
    src/player/mobj.c         src/player/mobj.h
    src/player/enemy.c
    src/game/finale.c
    src/game/stat.c
    src/game/strings.c         src/game/strings.h
    src/lib/random.c           src/lib/random.h
    src/game/game.c            src/game/game.h
    src/game/info.c            src/game/info.h
    src/game/items.c           src/game/items.h
    src/game/wipe.c            src/game/wipe.h
    src/game/event.c           src/game/event.h
    src/game/loop.c            src/game/loop.h
    src/game/gamemode.c        src/game/gamemode.h
    src/game/cheat.c           src/game/cheat.h
    src/game/controls.c        src/game/controls.h
                               src/game/ticcmd.h
                               src/game/keys.h
    src/automap/automap.c      src/automap/automap.h

    src/hud/lib.c              src/hud/lib.h
    src/hud/stuff.c            src/hud/stuff.h

    src/status/lib.c           src/status/lib.h
    src/status/stuff.c         src/status/stuff.h

    src/window/stuff.c         src/window/stuff.h

    src/player/ceiling.c
    src/player/door.c
    src/player/inter.c         src/player/inter.h
    src/player/fx.c            src/player/fx.h
    src/player/floor.c
    src/player/lights.c
    src/player/local.h
    src/player/map.c
    src/player/maputl.c
    src/player/platform.c
    src/player/savegame.h
    src/player/savegame.c
    src/player/setup.c         src/player/setup.h
    src/player/sight.c
    src/player/sprite.c        src/player/sprite.h
    src/player/switch.c
    src/player/teleport.c
    src/player/tick.c          src/player/tick.h
    src/player/user.c

    src/menu/menu.c            src/menu/menu.h

    src/sound/sound.c          src/sound/sound.h
    src/sound/sounds.c         src/sound/sounds.h

    src/impl/endoom.c          src/impl/endoom.h
    src/impl/glob.c            src/impl/glob.h
    src/impl/input.c           src/impl/input.h
    src/impl/joystick.c        src/impl/joystick.h
    src/impl/musicpack.c
    src/impl/sdlmusic.c
    src/impl/sdlsound.c
    src/impl/sound.c           src/impl/sound.h
    src/impl/timer.c           src/impl/timer.h
    src/impl/video.c           src/impl/video.h
                               src/impl/swap.h

    src/misc/bbox.c            src/misc/bbox.h
    src/misc/config.c          src/misc/config.h

    src/net/client.c           src/net/client.h
    src/net/common.c           src/net/common.h
    src/net/defs.h
    src/net/net.c
    src/net/gui.c              src/net/gui.h
    src/net/io.c               src/net/io.h
    src/net/loop.c             src/net/loop.h
    src/net/packet.c           src/net/packet.h
    src/net/petname.c          src/net/petname.h
    src/net/query.c            src/net/query.h
    src/net/sdl.c              src/net/sdl.h
    src/net/server.c           src/net/server.h
    src/net/structrw.c         src/net/structrw.h
    src/game/tables.c          src/game/tables.h
    src/video/diskicon.c       src/video/diskicon.h
    src/video/video.c          src/video/video.h
                               src/video/patch.h
    src/mem/zone.c             src/mem/zone.h

    src/wad/iwad.c             src/wad/iwad.h
    src/wad/merge.c            src/wad/merge.h
    src/wad/checksum.c         src/wad/checksum.h
    src/wad/main.c             src/wad/main.h
    src/wad/wad.c              src/wad/wad.h
    src/wad/file.c             src/wad/file.h
    src/wad/file_stdc.c
    src/wad/file_posix.c

    src/renderer/bsp.c         src/renderer/bsp.h
    src/renderer/data.c        src/renderer/data.h
    src/renderer/draw.c        src/renderer/draw.h
    src/renderer/main.c        src/renderer/main.h
    src/renderer/plane.c       src/renderer/plane.h
    src/renderer/segs.c        src/renderer/segs.h
    src/renderer/sky.c         src/renderer/sky.h
    src/renderer/things.c      src/renderer/things.h
                               src/renderer/state.h
                               src/renderer/local.h
                               src/renderer/defs.h
    )

set(GAME_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/../")

set(SOURCE_FILES ${COMMON_SOURCE_FILES} ${GAME_SOURCE_FILES} ${DED_SOURCE_FILES})

set(EXTRA_LIBS SDL2::SDL2main SDL2::SDL2 SDL2::mixer SDL2::net)

if(PNG_FOUND)
    list(APPEND EXTRA_LIBS PNG::PNG)
endif()

add_executable("${PROGRAM_PREFIX}doom" ${SOURCE_FILES})

target_include_directories("${PROGRAM_PREFIX}doom" PRIVATE ${GAME_INCLUDE_DIRS})
target_link_libraries("${PROGRAM_PREFIX}doom" ${EXTRA_LIBS})
