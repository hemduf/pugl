// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include <pugl/gl.h>
#include <pugl/pugl.h>

#include <emscripten.h>

#include <stdbool.h>
#include <stdio.h>

typedef struct {
  unsigned exposes;
  unsigned width;
  unsigned height;
  bool     pixelOk;
} TestState;

static void
setResult(const bool pass)
{
  emscripten_run_script(
    pass ? "document.body.dataset.puglTest='pass';"
           "document.body.dataset.puglReady='true';"
         : "document.body.dataset.puglTest='fail';"
           "document.body.dataset.puglReady='true';");
}

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
  TestState* const state = (TestState*)puglGetHandle(view);
  if (event->type != PUGL_EXPOSE) {
    return PUGL_SUCCESS;
  }

  ++state->exposes;
  state->width  = event->expose.width;
  state->height = event->expose.height;

  glViewport(0, 0, event->expose.width, event->expose.height);
  glClearColor(0.125f, 0.5f, 0.875f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  unsigned char pixel[4] = {0U, 0U, 0U, 0U};
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  state->pixelOk = pixel[0] >= 30U && pixel[0] <= 34U &&
                   pixel[1] >= 126U && pixel[1] <= 130U &&
                   pixel[2] >= 221U && pixel[2] <= 225U && pixel[3] == 255U;
  return state->pixelOk ? PUGL_SUCCESS : PUGL_FAILURE;
}

static PuglView*
newGlView(PuglWorld* const world,
          TestState* const state,
          const int        major,
          const unsigned   width,
          const unsigned   height)
{
  PuglView* const view = puglNewView(world);
  if (!view) {
    return NULL;
  }

  puglSetHandle(view, state);
  puglSetBackend(view, puglGlBackend());
  puglSetEventFunc(view, onEvent);
  puglSetViewHint(view, PUGL_CONTEXT_API, PUGL_OPENGL_ES_API);
  puglSetViewHint(view, PUGL_CONTEXT_VERSION_MAJOR, major);
  puglSetViewHint(view, PUGL_CONTEXT_VERSION_MINOR, 0);
  puglSetViewHint(view, PUGL_ALPHA_BITS, 8);
  puglSetViewHint(view, PUGL_DEPTH_BITS, 24);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, width, height);
  return view;
}

static bool
testWebGlVersion(PuglWorld* const world, const int major)
{
  TestState state = {0U, 0U, 0U, false};
  PuglView* const view = newGlView(world, &state, major, 64U, 64U);
  if (!view) {
    return false;
  }

  const PuglStatus showStatus = puglShow(view, PUGL_SHOW_PASSIVE);
  const bool contextOk =
    !showStatus &&
    puglGetViewHint(view, PUGL_CONTEXT_API) == PUGL_OPENGL_ES_API &&
    puglGetViewHint(view, PUGL_CONTEXT_VERSION_MAJOR) == major &&
    !puglEnterContext(view) && glGetString(GL_VERSION) && !puglLeaveContext(view);
  const bool renderOk =
    contextOk && !puglUpdate(world, 0.0) && state.exposes == 1U && state.pixelOk &&
    state.width == 64U && state.height == 64U;

  puglFreeView(view);
  return renderOk;
}

static bool
testResize(PuglWorld* const world)
{
  TestState state = {0U, 0U, 0U, false};
  PuglView* const view = newGlView(world, &state, 3, 64U, 64U);
  if (!view || puglShow(view, PUGL_SHOW_PASSIVE) || puglUpdate(world, 0.0) ||
      state.exposes != 1U || !state.pixelOk) {
    puglFreeView(view);
    return false;
  }

  state.pixelOk = false;
  const bool resized =
    !puglSetSizeHint(view, PUGL_CURRENT_SIZE, 96U, 48U) &&
    !puglUpdate(world, 0.0) && state.exposes == 2U && state.pixelOk &&
    state.width == 96U && state.height == 48U;

  puglFreeView(view);
  return resized;
}

static bool
testUnsupportedContext(PuglWorld* const world)
{
  TestState state = {0U, 0U, 0U, false};
  PuglView* const view = newGlView(world, &state, 4, 32U, 32U);
  if (!view) {
    return false;
  }

  const PuglStatus status = puglShow(view, PUGL_SHOW_PASSIVE);
  puglFreeView(view);
  return status == PUGL_BAD_CONFIGURATION;
}

int
main(void)
{
  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  if (!world) {
    setResult(false);
    return 1;
  }

  const bool webgl1 = testWebGlVersion(world, 2);
  const bool webgl2 = testWebGlVersion(world, 3);
  const bool resize = testResize(world);
  const bool unsupported = testUnsupportedContext(world);

  puglFreeWorld(world);

  const bool pass = webgl1 && webgl2 && resize && unsupported;
  if (!pass) {
    fprintf(stderr,
            "WebGL checks failed: webgl1=%d webgl2=%d resize=%d unsupported=%d\n",
            webgl1,
            webgl2,
            resize,
            unsupported);
  }

  setResult(pass);
  return pass ? 0 : 2;
}
