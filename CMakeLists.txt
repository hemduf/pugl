# Copyright 2026 Fabrizio Duhem
# SPDX-License-Identifier: 0BSD OR ISC

cmake_minimum_required(VERSION 3.20)

project(
  pugl
  VERSION 0.5.7
  DESCRIPTION "A minimal portable API for embeddable GUIs"
  LANGUAGES C
)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(EMSCRIPTEN)
  set(PUGL_DEFAULT_SHARED OFF)
else()
  set(PUGL_DEFAULT_SHARED ON)
endif()

option(BUILD_SHARED_LIBS "Build shared libraries" ${PUGL_DEFAULT_SHARED})
option(PUGL_BUILD_STUB "Build the stub graphics backend" ON)
option(PUGL_BUILD_OPENGL "Build the OpenGL graphics backend when available" ON)
option(PUGL_BUILD_CAIRO "Build the Cairo graphics backend when available" ON)
option(PUGL_BUILD_VULKAN "Build the Vulkan graphics backend when available" ON)
option(PUGL_BUILD_TESTS "Build CMake smoke tests" ${PROJECT_IS_TOP_LEVEL})
option(PUGL_INSTALL "Install Pugl and its CMake package" ${PROJECT_IS_TOP_LEVEL})
option(PUGL_XCURSOR "Use Xcursor when available" ON)
option(PUGL_XRANDR "Use Xrandr when available" ON)
option(PUGL_XSYNC "Use XSync when available" ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(PUGL_VERSION_SUFFIX "-${PROJECT_VERSION_MAJOR}")
set(PUGL_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/pugl${PUGL_VERSION_SUFFIX}")
set(PUGL_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/Pugl")

set(pugl_common_sources
  src/common.c
  src/internal.c
)

set(pugl_platform_compile_definitions)
set(pugl_platform_compile_options)
set(pugl_platform_link_libraries)
set(pugl_backend_extension ".c")
set(pugl_soversion "${PROJECT_VERSION_MAJOR}")

if(EMSCRIPTEN)
  set(pugl_platform "emscripten")
  set(pugl_platform_sources
    src/emscripten.c
    src/emscripten_events.c
  )
  set(pugl_soversion "")
elseif(APPLE)
  enable_language(OBJC)

  set(pugl_platform "mac")
  set(pugl_platform_sources src/mac.m)
  set(pugl_backend_extension ".m")

  find_library(PUGL_COCOA_FRAMEWORK Cocoa REQUIRED)
  find_library(PUGL_FOUNDATION_FRAMEWORK Foundation REQUIRED)
  find_library(PUGL_COREVIDEO_FRAMEWORK CoreVideo REQUIRED)

  list(APPEND pugl_platform_compile_definitions GL_SILENCE_DEPRECATION)
  list(APPEND pugl_platform_link_libraries
    "${PUGL_COCOA_FRAMEWORK}"
    "${PUGL_FOUNDATION_FRAMEWORK}"
    "${PUGL_COREVIDEO_FRAMEWORK}"
  )
elseif(WIN32)
  set(pugl_platform "win")
  set(pugl_platform_sources src/win.c)
  set(pugl_soversion "")

  list(APPEND pugl_platform_compile_definitions
    WINVER=0x0500
    _WIN32_WINNT=0x0500
    WIN32_LEAN_AND_MEAN
    NOGDICAPMASKS
    NOSYSMETRICS
    NOKEYSTATES
    OEMRESOURCE
    NOATOM
    NOCOLOR
    NODRAWTEXT
    NOKERNEL
    NOMB
    NOMEMMGR
    NOMETAFILE
    NOMINMAX
    NOOPENFILE
    NOSCROLL
    NOSERVICE
    NOSOUND
    NOWH
    NOCOMM
    NOKANJI
    NOHELP
    NOPROFILER
    NODEFERWINDOWPOS
    NOMCX
  )

  list(APPEND pugl_platform_link_libraries
    dwmapi
    gdi32
    shell32
    shlwapi
    user32
  )
else()
  set(pugl_platform "x11")
  set(pugl_platform_sources src/x11.c)

  find_package(X11 REQUIRED)
  list(APPEND pugl_platform_compile_definitions _POSIX_C_SOURCE=200809L)
  list(APPEND pugl_platform_link_libraries X11::X11)

  set(PUGL_USE_XCURSOR 0)
  if(PUGL_XCURSOR AND X11_Xcursor_FOUND)
    set(PUGL_USE_XCURSOR 1)
    if(TARGET X11::Xcursor)
      list(APPEND pugl_platform_link_libraries X11::Xcursor)
    else()
      list(APPEND pugl_platform_link_libraries ${X11_Xcursor_LIB})
    endif()
  endif()

  set(PUGL_USE_XRANDR 0)
  if(PUGL_XRANDR AND X11_Xrandr_FOUND)
    set(PUGL_USE_XRANDR 1)
    if(TARGET X11::Xrandr)
      list(APPEND pugl_platform_link_libraries X11::Xrandr)
    else()
      list(APPEND pugl_platform_link_libraries ${X11_Xrandr_LIB})
    endif()
  endif()

  set(PUGL_USE_XSYNC 0)
  if(PUGL_XSYNC AND X11_Xext_FOUND AND X11_XSync_FOUND)
    set(PUGL_USE_XSYNC 1)
    if(TARGET X11::Xext)
      list(APPEND pugl_platform_link_libraries X11::Xext)
    else()
      list(APPEND pugl_platform_link_libraries ${X11_Xext_LIB})
    endif()
  endif()

  list(APPEND pugl_platform_compile_definitions
    USE_XCURSOR=${PUGL_USE_XCURSOR}
    USE_XRANDR=${PUGL_USE_XRANDR}
    USE_XSYNC=${PUGL_USE_XSYNC}
  )
endif()

function(pugl_configure_library target)
  target_compile_features(${target} PUBLIC c_std_99)
  target_compile_definitions(${target} PRIVATE PUGL_INTERNAL)
  target_compile_definitions(${target} PRIVATE ${pugl_platform_compile_definitions})
  target_compile_options(${target} PRIVATE ${pugl_platform_compile_options})
  target_include_directories(
    ${target}
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:${PUGL_INSTALL_INCLUDEDIR}>
  )
  set_target_properties(${target} PROPERTIES
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
  )
endfunction()

add_library(pugl ${pugl_common_sources} ${pugl_platform_sources})
add_library(Pugl::pugl ALIAS pugl)
pugl_configure_library(pugl)
target_link_libraries(pugl PUBLIC ${pugl_platform_link_libraries})
set_target_properties(pugl PROPERTIES
  OUTPUT_NAME "pugl_${pugl_platform}${PUGL_VERSION_SUFFIX}"
  VERSION "${PROJECT_VERSION}"
)
if(pugl_soversion)
  set_target_properties(pugl PROPERTIES SOVERSION "${pugl_soversion}")
endif()
if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(pugl PUBLIC PUGL_STATIC)
endif()

set(pugl_install_targets pugl)
set(PUGL_CONFIG_HAS_OPENGL OFF)
set(PUGL_CONFIG_HAS_CAIRO OFF)
set(PUGL_CONFIG_HAS_VULKAN OFF)

if(PUGL_BUILD_STUB)
  add_library(pugl_stub "src/${pugl_platform}_stub${pugl_backend_extension}")
  add_library(Pugl::stub ALIAS pugl_stub)
  pugl_configure_library(pugl_stub)
  target_link_libraries(pugl_stub PUBLIC pugl)
  set_target_properties(pugl_stub PROPERTIES
    EXPORT_NAME stub
    OUTPUT_NAME "pugl_${pugl_platform}_stub${PUGL_VERSION_SUFFIX}"
    VERSION "${PROJECT_VERSION}"
  )
  if(pugl_soversion)
    set_target_properties(pugl_stub PROPERTIES SOVERSION "${pugl_soversion}")
  endif()
  list(APPEND pugl_install_targets pugl_stub)
endif()

if(PUGL_BUILD_OPENGL AND NOT EMSCRIPTEN)
  find_package(OpenGL QUIET)
  if(OpenGL_FOUND)
    add_library(pugl_gl "src/${pugl_platform}_gl${pugl_backend_extension}")
    add_library(Pugl::gl ALIAS pugl_gl)
    pugl_configure_library(pugl_gl)
    target_link_libraries(pugl_gl PUBLIC pugl OpenGL::GL)
    set_target_properties(pugl_gl PROPERTIES
      EXPORT_NAME gl
      OUTPUT_NAME "pugl_${pugl_platform}_gl${PUGL_VERSION_SUFFIX}"
      VERSION "${PROJECT_VERSION}"
    )
    if(pugl_soversion)
      set_target_properties(pugl_gl PROPERTIES SOVERSION "${pugl_soversion}")
    endif()
    list(APPEND pugl_install_targets pugl_gl)
    set(PUGL_CONFIG_HAS_OPENGL ON)
  else()
    message(STATUS "OpenGL not found, skipping the OpenGL backend")
  endif()
elseif(PUGL_BUILD_OPENGL AND EMSCRIPTEN)
  message(STATUS "WebGL backend is not enabled in the platform-lifecycle milestone")
endif()

if(PUGL_BUILD_CAIRO AND NOT EMSCRIPTEN)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(CAIRO QUIET IMPORTED_TARGET cairo)
  endif()

  if(TARGET PkgConfig::CAIRO)
    add_library(pugl_cairo "src/${pugl_platform}_cairo${pugl_backend_extension}")
    add_library(Pugl::cairo ALIAS pugl_cairo)
    pugl_configure_library(pugl_cairo)
    target_link_libraries(pugl_cairo PUBLIC pugl PkgConfig::CAIRO)
    if(APPLE)
      find_library(PUGL_COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
      target_link_libraries(pugl_cairo PUBLIC "${PUGL_COREGRAPHICS_FRAMEWORK}")
    endif()
    set_target_properties(pugl_cairo PROPERTIES
      EXPORT_NAME cairo
      OUTPUT_NAME "pugl_${pugl_platform}_cairo${PUGL_VERSION_SUFFIX}"
      VERSION "${PROJECT_VERSION}"
    )
    if(pugl_soversion)
      set_target_properties(pugl_cairo PROPERTIES SOVERSION "${pugl_soversion}")
    endif()
    list(APPEND pugl_install_targets pugl_cairo)
    set(PUGL_CONFIG_HAS_CAIRO ON)
  else()
    message(STATUS "Cairo not found, skipping the Cairo backend")
  endif()
endif()

if(PUGL_BUILD_VULKAN AND NOT EMSCRIPTEN)
  find_package(Vulkan QUIET)
  if(Vulkan_FOUND)
    add_library(pugl_vulkan "src/${pugl_platform}_vulkan${pugl_backend_extension}")
    add_library(Pugl::vulkan ALIAS pugl_vulkan)
    pugl_configure_library(pugl_vulkan)
    target_link_libraries(pugl_vulkan PUBLIC pugl Vulkan::Vulkan ${CMAKE_DL_LIBS})
    if(APPLE)
      find_library(PUGL_METAL_FRAMEWORK Metal REQUIRED)
      find_library(PUGL_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
      target_link_libraries(pugl_vulkan PUBLIC
        "${PUGL_METAL_FRAMEWORK}"
        "${PUGL_QUARTZCORE_FRAMEWORK}"
        "${PUGL_FOUNDATION_FRAMEWORK}"
      )
    endif()
    set_target_properties(pugl_vulkan PROPERTIES
      EXPORT_NAME vulkan
      OUTPUT_NAME "pugl_${pugl_platform}_vulkan${PUGL_VERSION_SUFFIX}"
      VERSION "${PROJECT_VERSION}"
    )
    if(pugl_soversion)
      set_target_properties(pugl_vulkan PROPERTIES SOVERSION "${pugl_soversion}")
    endif()
    list(APPEND pugl_install_targets pugl_vulkan)
    set(PUGL_CONFIG_HAS_VULKAN ON)
  else()
    message(STATUS "Vulkan not found, skipping the Vulkan backend")
  endif()
endif()

if(PUGL_BUILD_TESTS AND BUILD_TESTING)
  add_executable(pugl_test_strerror test/test_strerror.c)
  target_link_libraries(pugl_test_strerror PRIVATE pugl)
  add_test(NAME pugl.cmake.strerror COMMAND pugl_test_strerror)

  if(TARGET pugl_stub)
    add_executable(pugl_test_headers test/cmake/test_headers.c)
    target_link_libraries(pugl_test_headers PRIVATE pugl_stub)
    add_test(NAME pugl.cmake.headers COMMAND pugl_test_headers)
  endif()
endif()

if(PUGL_INSTALL)
  install(
    DIRECTORY include/pugl
    DESTINATION "${PUGL_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
  )

  install(
    TARGETS ${pugl_install_targets}
    EXPORT PuglTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION "${PUGL_INSTALL_INCLUDEDIR}"
  )

  configure_package_config_file(
    cmake/PuglConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/PuglConfig.cmake"
    INSTALL_DESTINATION "${PUGL_INSTALL_CMAKEDIR}"
  )
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/PuglConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
  )

  install(
    EXPORT PuglTargets
    FILE PuglTargets.cmake
    NAMESPACE Pugl::
    DESTINATION "${PUGL_INSTALL_CMAKEDIR}"
  )
  install(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/PuglConfig.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/PuglConfigVersion.cmake"
    DESTINATION "${PUGL_INSTALL_CMAKEDIR}"
  )
endif()
