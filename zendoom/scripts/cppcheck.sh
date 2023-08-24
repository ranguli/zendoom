#!/bin/sh

cppcheck --enable=all --suppress=missingInclude "${MESON_SOURCE_ROOT}"
