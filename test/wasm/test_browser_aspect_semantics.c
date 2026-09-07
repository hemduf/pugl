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

static bool
puglTestUnsupportedAspectBounds(void)
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

  const PuglStatus minStatus =
    puglSetSizeHint(view, PUGL_MIN_ASPECT, 4U, 3U);
  const PuglStatus maxStatus =
    puglSetSizeHint(view, PUGL_MAX_ASPECT, 16U, 9U);
  const bool unsupported = minStatus == PUGL_UNSUPPORTED &&
                           maxStatus == PUGL_UNSUPPORTED;

  puglFreeView(view);
  puglFreeWorld(world);

  if (!unsupported) {
    fprintf(stderr,
            "Browser min/max aspect hints must report PUGL_UNSUPPORTED\n");
  }

  return unsupported;
}

static bool
puglTestRaiseContract(void)
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
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 48U, 32U);

  const PuglStatus raiseStatus = puglShow(view, PUGL_SHOW_RAISE);
  const bool shownButNotRaised = raiseStatus == PUGL_FAILURE &&
                                 puglGetNativeView(view) != 0U &&
                                 puglGetVisible(view);

  if (puglGetNativeView(view)) {
    (void)puglUnrealize(view);
  }
  puglFreeView(view);
  puglFreeWorld(world);

  if (!shownButNotRaised) {
    fprintf(stderr,
            "Browser raise requests must show the view but report PUGL_FAILURE\n");
  }

  return shownButNotRaised;
}

static void
puglRunBrowserAspectSemantics(void* const data)
{
  (void)data;

  if (!puglTestFixedAspectContract() || !puglTestUnsupportedAspectBounds() ||
      !puglTestRaiseContract()) {
    emscripten_run_script(
      "throw new Error('Browser aspect/style semantics contract failed')");
  }
}

__attribute__((constructor)) static void
puglScheduleBrowserAspectSemantics(void)
{
  emscripten_set_timeout(puglRunBrowserAspectSemantics, 0.0, NULL);
}
