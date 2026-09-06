// Copyright 2026 Fabrizio Duhem
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <emscripten.h>

#include <stdbool.h>
#include <stdio.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "Check failed at %s:%d: %s\n",                         \
              __FILE__,                                                        \
              __LINE__,                                                        \
              #condition);                                                     \
      EM_ASM({ document.body.dataset.puglTest = 'fail'; });                    \
      return 1;                                                                \
    }                                                                          \
  } while (0)

typedef struct {
  unsigned realizes;
  unsigned configures;
  unsigned exposes;
  unsigned unrealizes;
  bool     mapped;
} TestState;

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
  TestState* const state = (TestState*)puglGetHandle(view);

  switch (event->type) {
  case PUGL_REALIZE:
    ++state->realizes;
    break;
  case PUGL_CONFIGURE:
    ++state->configures;
    state->mapped = (event->configure.style & PUGL_VIEW_STYLE_MAPPED) != 0U;
    break;
  case PUGL_EXPOSE:
    ++state->exposes;
    break;
  case PUGL_UNREALIZE:
    ++state->unrealizes;
    break;
  default:
    break;
  }

  return PUGL_SUCCESS;
}

int
main(void)
{
  TestState state = {0U, 0U, 0U, 0U, false};
  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  CHECK(world);

  PuglView* const view = puglNewView(world);
  CHECK(view);

  CHECK(!puglSetBackend(view, puglStubBackend()));
  CHECK(!puglSetHandle(view, &state));
  CHECK(!puglSetEventFunc(view, onEvent));
  CHECK(!puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 320U, 180U));
  CHECK(!puglSetPositionHint(view, PUGL_DEFAULT_POSITION, 10, 20));
  CHECK(!puglSetViewString(view, PUGL_WINDOW_TITLE, "Pugl WASM lifecycle"));

  CHECK(!puglRealize(view));
  CHECK(state.realizes == 1U);
  CHECK(puglGetNativeView(view) != 0U);
  CHECK(!puglGetVisible(view));
  CHECK(puglRealize(view) == PUGL_FAILURE);

  CHECK(!puglShow(view, PUGL_SHOW_PASSIVE));
  CHECK(state.mapped);
  CHECK(puglGetVisible(view));
  CHECK(!puglUpdate(world, 0.0));
  CHECK(state.exposes >= 1U);

  CHECK(!puglSetSizeHint(view, PUGL_CURRENT_SIZE, 400U, 240U));
  CHECK(puglGetSizeHint(view, PUGL_CURRENT_SIZE).width == 400U);
  CHECK(puglGetSizeHint(view, PUGL_CURRENT_SIZE).height == 240U);
  CHECK(!puglUpdate(world, 0.0));

  CHECK(!puglGrabFocus(view));
  CHECK(puglHasFocus(view));

  CHECK(!puglHide(view));
  CHECK(!state.mapped);
  CHECK(!puglGetVisible(view));

  CHECK(!puglUnrealize(view));
  CHECK(state.unrealizes == 1U);
  CHECK(puglGetNativeView(view) == 0U);

  puglFreeView(view);
  puglFreeWorld(world);

  EM_ASM({ document.body.dataset.puglTest = 'pass'; });
  return 0;
}
