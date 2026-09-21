// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <emscripten.h>

#include <stdbool.h>
#include <stdio.h>

#define CHECK(test)                   \
  do {                                \
    if (!(test)) {                    \
      return fail(#test, __LINE__);   \
    }                                 \
  } while (0)

typedef struct {
  unsigned focusIn;
  unsigned focusOut;
  unsigned buttonPress;
  unsigned buttonRelease;
  unsigned motion;
} TestState;

static PuglWorld* world;
static PuglView*  viewA;
static PuglView*  viewB;
static TestState  stateA;
static TestState  stateB;

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
  fprintf(stderr,
          "Multi-view focus check failed at line %d: %s "
          "[A in=%u out=%u press=%u release=%u motion=%u; "
          "B in=%u out=%u]\n",
          line,
          expression,
          stateA.focusIn,
          stateA.focusOut,
          stateA.buttonPress,
          stateA.buttonRelease,
          stateA.motion,
          stateB.focusIn,
          stateB.focusOut);
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
  case PUGL_FOCUS_IN:
    ++state->focusIn;
    break;
  case PUGL_FOCUS_OUT:
    ++state->focusOut;
    break;
  case PUGL_BUTTON_PRESS:
    ++state->buttonPress;
    break;
  case PUGL_BUTTON_RELEASE:
    ++state->buttonRelease;
    break;
  case PUGL_MOTION:
    ++state->motion;
    break;
  default:
    break;
  }

  return PUGL_SUCCESS;
}

EMSCRIPTEN_KEEPALIVE int
puglWasmMultiViewDestroySecond(void)
{
  CHECK(world);
  CHECK(viewA);
  CHECK(viewB);
  CHECK(puglHasFocus(viewB));

  puglFreeView(viewB);
  viewB = NULL;

  CHECK(!puglHasFocus(viewA));
  return 0;
}

EMSCRIPTEN_KEEPALIVE int
puglWasmMultiViewFinish(void)
{
  CHECK(world);
  CHECK(viewA);
  CHECK(!viewB);
  CHECK(puglHasFocus(viewA));
  CHECK(stateA.focusIn >= 2U);
  CHECK(stateA.focusOut >= 1U);
  CHECK(stateA.buttonPress >= 1U);
  CHECK(stateA.buttonRelease >= 1U);
  CHECK(stateA.motion >= 1U);
  CHECK(stateB.focusIn >= 1U);

  puglFreeView(viewA);
  puglFreeWorld(world);
  viewA = NULL;
  world = NULL;

  setBrowserResult(true);
  return 0;
}

static PuglView*
makeView(TestState* const state)
{
  PuglView* const view = puglNewView(world);
  if (!view) {
    return NULL;
  }

  puglSetHandle(view, state);
  puglSetBackend(view, puglStubBackend());
  puglSetEventFunc(view, onEvent);
  if (puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 240U, 140U) ||
      puglShow(view, PUGL_SHOW_PASSIVE)) {
    puglFreeView(view);
    return NULL;
  }

  return view;
}

int
main(void)
{
  stateA = (TestState){0};
  stateB = (TestState){0};
  world = puglNewWorld(PUGL_PROGRAM, 0U);
  CHECK(world);

  viewA = makeView(&stateA);
  CHECK(viewA);
  viewB = makeView(&stateB);
  CHECK(viewB);
  CHECK(puglGetNativeView(viewA) != 0U);
  CHECK(puglGetNativeView(viewB) != 0U);
  CHECK(puglGetNativeView(viewA) != puglGetNativeView(viewB));

  emscripten_run_script(
    "document.body.dataset.puglTest='pending';"
    "document.body.dataset.puglReady='true';");
  return 0;
}
