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

typedef struct PuglEmscriptenTimerImpl PuglEmscriptenTimer;

struct PuglEmscriptenTimerImpl {
  PuglEmscriptenTimer* next;
  PuglView*            view;
  uintptr_t            id;
  int                  intervalHandle;
};

struct PuglWorldInternalsImpl {
  uintptr_t nextViewId;
  double    scaleFactor;
};

struct PuglInternalsImpl {
  PuglSurface*         surface;
  PuglEvent            pendingExpose;
  PuglBlob             clipboardData;
  PuglEmscriptenTimer* timers;
  uintptr_t            id;
  char                 hostId[PUGL_EMSCRIPTEN_ID_SIZE];
  char                 canvasId[PUGL_EMSCRIPTEN_ID_SIZE];
  char                 canvasSelector[PUGL_EMSCRIPTEN_SELECTOR_SIZE];
  double               pointerX;
  double               pointerY;
  double               pointerRootX;
  double               pointerRootY;
  bool                 mapped;
  bool                 clipboardOffer;
  bool                 clipboardRequest;
};

#endif // PUGL_SRC_EMSCRIPTEN_PLATFORM_H
