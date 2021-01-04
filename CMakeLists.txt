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

configure_file(cmake/config.h.cin config.h)

add_subdirectory("src")

