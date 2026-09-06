# Copyright 2026 Fabrizio Duhem
# SPDX-License-Identifier: 0BSD OR ISC

cmake_minimum_required(VERSION 3.20)
project(pugl_wasm_tests LANGUAGES C)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(PUGL_BUILD_CAIRO OFF CACHE BOOL "" FORCE)
set(PUGL_BUILD_OPENGL OFF CACHE BOOL "" FORCE)
set(PUGL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PUGL_BUILD_VULKAN OFF CACHE BOOL "" FORCE)
set(PUGL_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." pugl)

function(pugl_add_wasm_test target source)
  add_executable(${target} ${source})
  target_compile_features(${target} PRIVATE c_std_99)
  target_link_libraries(${target} PRIVATE Pugl::stub)
  target_link_options(${target} PRIVATE -sASSERTIONS=1 -sEXIT_RUNTIME=1)
  set_target_properties(${target} PROPERTIES SUFFIX ".html")
endfunction()

pugl_add_wasm_test(pugl_wasm_lifecycle test_lifecycle.c)
pugl_add_wasm_test(pugl_wasm_input test_input.c)
