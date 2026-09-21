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
  // Effective view focus, decoupled from the hidden text input's own
  // focus/blur transitions (the input briefly loses focus to its canvas on
  // every click, which is not a view focus change).
  bool         focused;
};

#endif // PUGL_SRC_EMSCRIPTEN_PLATFORM_H
