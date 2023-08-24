#!/bin/sh

find "${MESON_SOURCE_ROOT}/src/" -iname *.h -o -iname *.cpp | xargs clang-format -i --verbose
