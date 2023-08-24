#!/bin/sh

clang-tidy "${MESON_SOURCE_ROOT}"/src/*/*.c -checks=bugprone-*,cert-*,misc-*,modernize-*,performance-*,readability-*,portability-*,clang-analyzer-* --format-style='file' --
