// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#ifndef PUGL_SRC_EMSCRIPTEN_PLATFORM_H
#define PUGL_SRC_EMSCRIPTEN_PLATFORM_H

#include "types.h"

#include <pugl/pugl.h>

#include <stdbool.h>
#include <stdint.h>

struct PuglWorldInternalsImpl {
  uintptr_t nextViewId;
  double    scaleFactor;
};

struct PuglInternalsImpl {
  PuglSurface* surface;
  PuglEvent    pendingExpose;
  uintptr_t    id;
  char         canvasSelector[48];
  bool         mapped;
};

#endif // PUGL_SRC_EMSCRIPTEN_PLATFORM_H
