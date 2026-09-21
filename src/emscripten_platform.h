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
  void*        eventBinding;
  uintptr_t    id;
  double       pointerX;
  double       pointerY;
  double       pointerRootX;
  double       pointerRootY;
  char         canvasSelector[48];
  bool         mapped;
};

#endif // PUGL_SRC_EMSCRIPTEN_PLATFORM_H
