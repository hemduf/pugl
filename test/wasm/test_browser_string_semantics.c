// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include <stdbool.h>
#include <stdio.h>

EM_JS(int, puglTestDocumentTitleEquals, (const char* expected), {
  return typeof document !== 'undefined' &&
         document.title === UTF8ToString(expected);
});

static PuglStatus
puglTestIgnoreStringEvent(PuglView* const view, const PuglEvent* const event)
{
  (void)view;
  (void)event;
  return PUGL_SUCCESS;
}

static bool
puglTestBrowserStringContract(void)
{
  static const char title[] = "Pugl Browser Contract";

  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglView* const  view  = world ? puglNewView(world) : NULL;
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
  puglSetEventFunc(view, puglTestIgnoreStringEvent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 32U, 32U);

  const bool realized = puglShow(view, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS;
  const PuglStatus titleStatus =
    realized ? puglSetViewString(view, PUGL_WINDOW_TITLE, title) : PUGL_FAILURE;
  const PuglStatus applicationStatus =
    realized ? puglSetViewString(view,
                                 PUGL_APPLICATION_NAME,
                                 "pugl-browser-contract")
             : PUGL_FAILURE;
  const PuglStatus classStatus =
    realized ? puglSetViewString(view,
                                 PUGL_CLASS_NAME,
                                 "PuglBrowserContract")
             : PUGL_FAILURE;

  const bool titleMapped = realized && titleStatus == PUGL_SUCCESS &&
                           puglTestDocumentTitleEquals(title);
  const bool identifiersExplicit =
    applicationStatus == PUGL_UNSUPPORTED && classStatus == PUGL_UNSUPPORTED;

  if (realized) {
    (void)puglUnrealize(view);
  }
  puglFreeView(view);
  puglFreeWorld(world);

  if (!titleMapped || !identifiersExplicit) {
    fprintf(stderr,
            "Browser string semantics failed: title=%d app=%d class=%d mapped=%d\n",
            (int)titleStatus,
            (int)applicationStatus,
            (int)classStatus,
            titleMapped);
  }

  return titleMapped && identifiersExplicit;
}

static void
puglRunBrowserStringSemantics(void* const data)
{
  (void)data;

  if (!puglTestBrowserStringContract()) {
    emscripten_run_script(
      "throw new Error('Browser string-hint semantics contract failed')");
  }
}

__attribute__((constructor)) static void
puglScheduleBrowserStringSemantics(void)
{
  emscripten_set_timeout(puglRunBrowserStringSemantics, 0.0, NULL);
}
