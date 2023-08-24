#!/bin/sh

find "${MESON_SOURCE_ROOT}/src/" -iname *.h -o -iname *.c | xargs clang-format -i --verbose
