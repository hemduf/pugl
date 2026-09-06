// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <emscripten.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

EM_JS(int,
      puglTestCursorEquals,
      (uintptr_t nativeView, const char* expected), {
  const element = document.getElementById(`pugl-view-${nativeView}`);
  return !!element &&
         getComputedStyle(element).cursor === UTF8ToString(expected);
});

__attribute__((constructor)) static void
puglTestBrowserCursorContract(void)
{
  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglView* const  view  = world ? puglNewView(world) : NULL;
  if (!world || !view) {
    fprintf(stderr, "Browser cursor test setup failed\n");
    return;
  }

  puglSetBackend(view, puglStubBackend());
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 32U, 32U);

  const bool realized = puglShow(view, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS;
  const PuglStatus cursorStatus =
    realized ? puglSetCursor(view, PUGL_CURSOR_CROSSHAIR) : PUGL_FAILURE;
  const bool cursorMatches =
    realized && cursorStatus == PUGL_SUCCESS &&
    puglTestCursorEquals(puglGetNativeView(view), "crosshair");

  if (realized) {
    (void)puglUnrealize(view);
  }
  puglFreeView(view);
  puglFreeWorld(world);

  if (!cursorMatches) {
    fprintf(stderr,
            "Browser cursor mapping contract is not implemented: status=%d\n",
            (int)cursorStatus);
    emscripten_run_script(
      "throw new Error('Browser cursor mapping contract is not implemented')");
  }
}

__attribute__((constructor)) static void
puglTestUnsupportedDesktopWindowContract(void)
{
  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglView* const  view  = world ? puglNewView(world) : NULL;
  if (!world || !view) {
    fprintf(stderr, "Browser desktop-window semantics test setup failed\n");
    return;
  }

  const PuglStatus positionStatus = puglSetWindowPosition(view, 12, 34);
  const PuglStatus transientStatus =
    puglSetTransientParent(view, (PuglNativeView)1234U);

  puglFreeView(view);
  puglFreeWorld(world);

  if (positionStatus != PUGL_UNSUPPORTED ||
      transientStatus != PUGL_UNSUPPORTED) {
    fprintf(stderr,
            "Browser desktop-only APIs must be explicit: position=%d transient=%d\n",
            (int)positionStatus,
            (int)transientStatus);
    emscripten_run_script(
      "throw new Error('Browser desktop-only API contract is not explicit')");
  }
}
