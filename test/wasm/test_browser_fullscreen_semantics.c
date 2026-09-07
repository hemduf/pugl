// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
  PuglViewStyleFlags style;
  unsigned           configureEvents;
} PuglBrowserFullscreenState;

static PuglBrowserFullscreenState puglBrowserFullscreenState = {0U, 0U};

EM_JS(int, puglTestInstallFullscreenMock, (uintptr_t nativeView), {
  const element = document.getElementById(`pugl-view-${nativeView}`);
  if (!element || typeof document === 'undefined') {
    return 0;
  }

  try {
    Object.defineProperty(document, 'fullscreenEnabled', {
      configurable: true,
      value: true,
    });
    Object.defineProperty(document, 'fullscreenElement', {
      configurable: true,
      writable: true,
      value: null,
    });
  } catch (_) {
    return 0;
  }

  element.requestFullscreen = () => {
    document.fullscreenElement = element;
    document.dispatchEvent(new Event('fullscreenchange'));
    return Promise.resolve();
  };

  document.exitFullscreen = () => {
    document.fullscreenElement = null;
    document.dispatchEvent(new Event('fullscreenchange'));
    return Promise.resolve();
  };

  return 1;
});

EM_JS(int, puglTestIsFullscreen, (uintptr_t nativeView), {
  const element = document.getElementById(`pugl-view-${nativeView}`);
  return !!element && document.fullscreenElement === element;
});

static PuglStatus
puglTestFullscreenEvent(PuglView* const view, const PuglEvent* const event)
{
  (void)view;

  if (event->type == PUGL_CONFIGURE) {
    puglBrowserFullscreenState.style = event->configure.style;
    ++puglBrowserFullscreenState.configureEvents;
  }

  return PUGL_SUCCESS;
}

static void
puglRunBrowserFullscreenSemantics(void* const data)
{
  (void)data;

  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglView* const  view  = world ? puglNewView(world) : NULL;
  if (!world || !view) {
    if (view) {
      puglFreeView(view);
    }
    if (world) {
      puglFreeWorld(world);
    }
    emscripten_run_script(
      "throw new Error('Browser fullscreen test setup failed')");
    return;
  }

  puglSetBackend(view, puglStubBackend());
  puglSetEventFunc(view, puglTestFullscreenEvent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 48U, 32U);

  const bool shown = puglShow(view, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS;
  const PuglNativeView nativeView = puglGetNativeView(view);
  const bool mockInstalled =
    shown && nativeView && puglTestInstallFullscreenMock(nativeView);
  const unsigned beforeEnter = puglBrowserFullscreenState.configureEvents;

  const PuglStatus enterStatus =
    mockInstalled
      ? puglSetViewStyle(
          view, PUGL_VIEW_STYLE_MAPPED | PUGL_VIEW_STYLE_FULLSCREEN)
      : PUGL_FAILURE;

  const bool entered =
    enterStatus == PUGL_SUCCESS && puglTestIsFullscreen(nativeView) &&
    (puglGetViewStyle(view) & PUGL_VIEW_STYLE_FULLSCREEN) &&
    puglBrowserFullscreenState.configureEvents > beforeEnter &&
    (puglBrowserFullscreenState.style & PUGL_VIEW_STYLE_FULLSCREEN);

  const PuglStatus hideStatus = entered ? puglHide(view) : PUGL_FAILURE;
  const bool hidden =
    hideStatus == PUGL_SUCCESS && !puglTestIsFullscreen(nativeView) &&
    !puglGetVisible(view) &&
    !(puglGetViewStyle(view) & PUGL_VIEW_STYLE_FULLSCREEN);

  const PuglStatus reshowStatus =
    hidden ? puglShow(view, PUGL_SHOW_PASSIVE) : PUGL_FAILURE;
  const PuglStatus reenterStatus =
    reshowStatus == PUGL_SUCCESS
      ? puglSetViewStyle(
          view, PUGL_VIEW_STYLE_MAPPED | PUGL_VIEW_STYLE_FULLSCREEN)
      : PUGL_FAILURE;
  const bool reentered =
    reenterStatus == PUGL_SUCCESS && puglTestIsFullscreen(nativeView) &&
    (puglGetViewStyle(view) & PUGL_VIEW_STYLE_FULLSCREEN);

  const unsigned beforeLeave = puglBrowserFullscreenState.configureEvents;
  const PuglStatus leaveStatus =
    reentered ? puglSetViewStyle(view, PUGL_VIEW_STYLE_MAPPED) : PUGL_FAILURE;
  const bool left =
    leaveStatus == PUGL_SUCCESS && !puglTestIsFullscreen(nativeView) &&
    !(puglGetViewStyle(view) & PUGL_VIEW_STYLE_FULLSCREEN) &&
    puglBrowserFullscreenState.configureEvents > beforeLeave &&
    !(puglBrowserFullscreenState.style & PUGL_VIEW_STYLE_FULLSCREEN);

  const PuglStatus unsupportedStatus =
    puglSetViewStyle(view, PUGL_VIEW_STYLE_MAPPED | PUGL_VIEW_STYLE_ABOVE);
  const bool explicitUnsupported = unsupportedStatus == PUGL_UNSUPPORTED;

  if (shown) {
    (void)puglUnrealize(view);
  }
  puglFreeView(view);
  puglFreeWorld(world);

  if (!mockInstalled || !entered || !hidden || !reentered || !left ||
      !explicitUnsupported) {
    fprintf(stderr,
            "Browser fullscreen style contract failed: mock=%d enter=%d "
            "entered=%d hide=%d hidden=%d reenter=%d reentered=%d "
            "leave=%d left=%d unsupported=%d\n",
            mockInstalled,
            (int)enterStatus,
            entered,
            (int)hideStatus,
            hidden,
            (int)reenterStatus,
            reentered,
            (int)leaveStatus,
            left,
            (int)unsupportedStatus);
    emscripten_run_script(
      "throw new Error('Browser fullscreen style contract failed')");
  }
}

__attribute__((constructor)) static void
puglScheduleBrowserFullscreenSemantics(void)
{
  emscripten_set_timeout(puglRunBrowserFullscreenSemantics, 0.0, NULL);
}
