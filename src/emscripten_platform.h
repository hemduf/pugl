// Copyright 2026 Fabrizio Duhem
// SPDX-License-Identifier: ISC

#ifndef PUGL_SRC_EMSCRIPTEN_PLATFORM_H
#define PUGL_SRC_EMSCRIPTEN_PLATFORM_H

#include "types.h"

#include <pugl/pugl.h>

#include <stdbool.h>
#include <stdint.h>

#define PUGL_EMSCRIPTEN_ID_SIZE 48U
#define PUGL_EMSCRIPTEN_SELECTOR_SIZE 52U

struct PuglWorldInternalsImpl {
  uintptr_t nextViewId;
  double    scaleFactor;
};

struct PuglInternalsImpl {
  PuglSurface* surface;
  PuglEvent    pendingExpose;
  uintptr_t    id;
  char         hostId[PUGL_EMSCRIPTEN_ID_SIZE];
  char         canvasId[PUGL_EMSCRIPTEN_ID_SIZE];
  char         canvasSelector[PUGL_EMSCRIPTEN_SELECTOR_SIZE];
  bool         mapped;
};

#endif // PUGL_SRC_EMSCRIPTEN_PLATFORM_H
