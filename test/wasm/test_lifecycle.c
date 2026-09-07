// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <emscripten.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK(test)                   \
  do {                                \
    if (!(test)) {                    \
      return fail(#test, __LINE__);   \
    }                                 \
  } while (0)

typedef struct {
  unsigned        realize;
  unsigned        unrealize;
  unsigned        configure;
  unsigned        update;
  unsigned        expose;
  PuglExposeEvent lastExpose;
} TestState;

static void
setBrowserResult(const bool pass)
{
  emscripten_run_script(
    pass ? "document.body.dataset.puglTest='pass';"
           "document.body.dataset.puglReady='true';"
         : "document.body.dataset.puglTest='fail';"
           "document.body.dataset.puglReady='true';");
}

static int
fail(const char* const expression, const int line)
{
  fprintf(stderr, "Lifecycle check failed at line %d: %s\n", line, expression);
  setBrowserResult(false);
  return 1;
}

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
  TestState* const state = (TestState*)puglGetHandle(view);
  if (!state) {
    return PUGL_FAILURE;
  }

  switch (event->type) {
  case PUGL_REALIZE:
    ++state->realize;
    break;
  case PUGL_UNREALIZE:
    ++state->unrealize;
    break;
  case PUGL_CONFIGURE:
    ++state->configure;
    break;
  case PUGL_UPDATE:
    ++state->update;
    break;
  case PUGL_EXPOSE:
    ++state->expose;
    state->lastExpose = event->expose;
    break;
  default:
    break;
  }

  return PUGL_SUCCESS;
}

static int
evalForView(const PuglNativeView nativeView, const char* const expression)
{
  char script[512] = {0};
  snprintf(script,
           sizeof(script),
           "(()=>{const e=document.getElementById('pugl-view-%" PRIuPTR
           "');return e?(%s):0;})()",
           (uintptr_t)nativeView,
           expression);
  return emscripten_run_script_int(script);
}

static bool
hasCanvas(const PuglNativeView nativeView)
{
  return evalForView(nativeView, "1") != 0;
}

static int
cssWidth(const PuglNativeView nativeView)
{
  return evalForView(nativeView, "parseInt(e.style.width,10)");
}

static int
cssHeight(const PuglNativeView nativeView)
{
  return evalForView(nativeView, "parseInt(e.style.height,10)");
}

static bool
isDisplayed(const PuglNativeView nativeView)
{
  return evalForView(nativeView, "getComputedStyle(e).display!=='none'") != 0;
}

static PuglView*
newView(PuglWorld* const world, TestState* const state)
{
  PuglView* const view = puglNewView(world);
  if (!view) {
    return NULL;
  }

  puglSetHandle(view, state);
  puglSetBackend(view, puglStubBackend());
  puglSetEventFunc(view, onEvent);
  if (puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 320U, 180U)) {
    puglFreeView(view);
    return NULL;
  }

  return view;
}

int
main(void)
{
  TestState state0 = {0};
  TestState state1 = {0};
  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  CHECK(world);

  PuglView* const view0 = newView(world, &state0);
  CHECK(view0);
  CHECK(puglRealize(view0) == PUGL_SUCCESS);
  CHECK(state0.realize == 1U);

  const PuglNativeView native0 = puglGetNativeView(view0);
  CHECK(native0 != 0U);
  CHECK(hasCanvas(native0));
  CHECK(cssWidth(native0) == 320);
  CHECK(cssHeight(native0) == 180);

  CHECK(puglShow(view0, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS);
  CHECK(state0.configure >= 1U);
  CHECK(puglGetVisible(view0));
  CHECK(isDisplayed(native0));
  CHECK(puglGetScaleFactor(view0) > 0.0);

  CHECK(puglSetSizeHint(view0, PUGL_CURRENT_SIZE, 400U, 220U) == PUGL_SUCCESS);
  CHECK(puglGetSizeHint(view0, PUGL_CURRENT_SIZE).width == 400U);
  CHECK(puglGetSizeHint(view0, PUGL_CURRENT_SIZE).height == 220U);
  CHECK(cssWidth(native0) == 400);
  CHECK(cssHeight(native0) == 220);

  CHECK(puglObscureView(view0) == PUGL_SUCCESS);
  const double updateStart = emscripten_get_now();
  CHECK(puglUpdate(world, 1.0) == PUGL_SUCCESS);
  CHECK((emscripten_get_now() - updateStart) < 100.0);
  CHECK(state0.update >= 1U);
  CHECK(state0.expose >= 1U);

  const unsigned exposeCount = state0.expose;
  state0.lastExpose = (PuglExposeEvent){0};
  CHECK(puglObscureRegion(view0, -10, -20, 1000U, 1000U) == PUGL_SUCCESS);
  CHECK(puglUpdate(world, 0.0) == PUGL_SUCCESS);
  CHECK(state0.expose == exposeCount + 1U);
  CHECK(state0.lastExpose.x == 0);
  CHECK(state0.lastExpose.y == 0);
  CHECK(state0.lastExpose.width == 400U);
  CHECK(state0.lastExpose.height == 220U);
  CHECK(puglObscureRegion(view0, INT16_MIN, 0, 1U, 1U) == PUGL_BAD_PARAMETER);

  CHECK(puglHide(view0) == PUGL_SUCCESS);
  CHECK(!puglGetVisible(view0));
  CHECK(!isDisplayed(native0));

  CHECK(puglShow(view0, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS);
  CHECK(puglGetVisible(view0));
  CHECK(isDisplayed(native0));

  PuglView* const view1 = newView(world, &state1);
  CHECK(view1);
  CHECK(puglShow(view1, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS);

  const PuglNativeView native1 = puglGetNativeView(view1);
  CHECK(native1 != 0U);
  CHECK(native1 != native0);
  CHECK(hasCanvas(native1));

  puglFreeView(view1);
  CHECK(!hasCanvas(native1));

  CHECK(puglUnrealize(view0) == PUGL_SUCCESS);
  CHECK(state0.unrealize == 1U);
  CHECK(!hasCanvas(native0));

  puglFreeView(view0);
  puglFreeWorld(world);
  setBrowserResult(true);
  return 0;
}
