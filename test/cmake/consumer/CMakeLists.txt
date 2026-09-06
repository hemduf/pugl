# Copyright 2026 Fabrizio Duhem
# SPDX-License-Identifier: 0BSD OR ISC

cmake_minimum_required(VERSION 3.20)
project(pugl_cmake_consumer LANGUAGES C)

find_package(Pugl 0.5 CONFIG REQUIRED)

add_executable(pugl_cmake_consumer main.c)
target_link_libraries(pugl_cmake_consumer PRIVATE Pugl::pugl Pugl::stub)
target_compile_features(pugl_cmake_consumer PRIVATE c_std_99)
