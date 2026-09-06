# Copyright 2026 Fabrizio Duhem
# SPDX-License-Identifier: 0BSD OR ISC

cmake_minimum_required(VERSION 3.20)
project(pugl_wasm_lifecycle_test LANGUAGES C)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(PUGL_BUILD_CAIRO OFF CACHE BOOL "" FORCE)
set(PUGL_BUILD_OPENGL OFF CACHE BOOL "" FORCE)
set(PUGL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PUGL_BUILD_VULKAN OFF CACHE BOOL "" FORCE)
set(PUGL_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." pugl)

add_executable(pugl_wasm_lifecycle test_lifecycle.c)
target_compile_features(pugl_wasm_lifecycle PRIVATE c_std_99)
target_link_libraries(pugl_wasm_lifecycle PRIVATE Pugl::stub)
target_link_options(pugl_wasm_lifecycle PRIVATE -sASSERTIONS=1 -sEXIT_RUNTIME=1)
set_target_properties(pugl_wasm_lifecycle PROPERTIES SUFFIX ".html")
