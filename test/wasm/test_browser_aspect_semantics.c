// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

EM_JS(int,
      puglTestCanvasHasFixedAspect,
      (uintptr_t nativeView, unsigned width, unsigned height), {
  const element = document.getElementById(`pugl-view-${nativeView}`);
  if (!element) {
    return 0;
  }

  const actual = getComputedStyle(element).aspectRatio;
  return actual === `${width} / ${height}` || actual === `${width}/${height}`;
});

static PuglStatus
puglTestIgnoreAspectEvent(PuglView* const view, const PuglEvent* const event)
{
  (void)view;
  (void)event;
  return PUGL_SUCCESS;
}

static bool
puglTestFixedAspectContract(void)
{
  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglView* const view = world ? puglNewView(world) : NULL;
  if (!world || !view) {
    if (view) {
      puglFreeView(view);
    }
    if (world) {
      puglFreeWorld(world);
    }
    return false;
  }

  puglSetBackend(view, puglStubBackend());
  puglSetEventFunc(view, puglTestIgnoreAspectEvent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 64U, 36U);
  const PuglStatus aspectStatus =
    puglSetSizeHint(view, PUGL_FIXED_ASPECT, 16U, 9U);
  const bool realized = puglShow(view, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS;
  const PuglNativeView nativeView = puglGetNativeView(view);
  const bool fixedAspect = aspectStatus == PUGL_SUCCESS && realized && nativeView &&
                           puglTestCanvasHasFixedAspect(nativeView, 16U, 9U);

  if (realized) {
    (void)puglUnrealize(view);
  }
  puglFreeView(view);
  puglFreeWorld(world);

  if (!fixedAspect) {
    fprintf(stderr,
            "Browser fixed-aspect hint must map to the native CSS aspect ratio\n");
  }

  return fixedAspect;
}

static void
puglRunBrowserAspectSemantics(void* const data)
{
  (void)data;

  if (!puglTestFixedAspectContract()) {
    emscripten_run_script(
      "throw new Error('Browser aspect-ratio semantics contract failed')");
  }
}

__attribute__((constructor)) static void
puglScheduleBrowserAspectSemantics(void)
{
  emscripten_set_timeout(puglRunBrowserAspectSemantics, 0.0, NULL);
}
