// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

// Tests basic view setup

#undef NDEBUG

#include <puglutil/test_utils.h>

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#if defined(__APPLE__)
#  include <TargetConditionals.h>
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(PUGL_POINTER_CANCEL == 27, "Existing event ABI changed");
_Static_assert(PUGL_TEXT_EDIT == 28, "Text edit event must be append-only");
_Static_assert(sizeof(PuglTextEditEvent) <= sizeof(PuglEvent),
               "Text edit event must not grow PuglEvent");
#endif

typedef enum {
  START,
  REALIZED,
  CONFIGURED,
  UNREALIZED,
} State;

typedef struct {
  PuglWorld*      world;
  PuglView*       view;
  PuglTestOptions opts;
  State           state;
} PuglTest;

static PuglStatus
onEvent(PuglView* view, const PuglEvent* event)
{
  PuglTest* test = (PuglTest*)puglGetHandle(view);

  if (test->opts.verbose) {
    printEvent(event, "Event: ", true);
  }

  switch (event->type) {
  case PUGL_REALIZE:
    assert(test->state == START);
    test->state = REALIZED;
    break;
  case PUGL_CONFIGURE:
    assert(test->state == REALIZED || test->state == CONFIGURED);
    test->state = CONFIGURED;
    break;
  case PUGL_UNREALIZE:
    test->state = UNREALIZED;
    break;
  default:
    break;
  }

  return PUGL_SUCCESS;
}

int
main(int argc, char** argv)
{
  PuglTest test = {puglNewWorld(PUGL_PROGRAM, 0),
                   NULL,
                   puglParseTestOptions(&argc, &argv),
                   START};

  // Set up view
  test.view = puglNewView(test.world);
  puglSetWorldString(test.world, PUGL_CLASS_NAME, "PuglTest");
  puglSetViewString(test.view, PUGL_WINDOW_TITLE, "Pugl View Test");
  puglSetBackend(test.view, puglStubBackend());
  puglSetHandle(test.view, &test);
  puglSetEventFunc(test.view, onEvent);
  puglSetSizeHint(test.view, PUGL_DEFAULT_SIZE, 256, 256);
  puglSetPositionHint(test.view, PUGL_DEFAULT_POSITION, 384, 640);

  // Text input flags are portable common state throughout the view lifetime.
  assert(!puglIsTextInputActive(test.view));
  assert(!puglSetTextInputFlags(test.view, 0U));
  assert(!puglSetTextInputFlags(test.view, PUGL_TEXT_INPUT_HAS_TEXT));
  assert(puglSetTextInputFlags(test.view, PUGL_TEXT_INPUT_HAS_TEXT | (1U << 1U)) ==
         PUGL_BAD_PARAMETER);

  // Native text-input sessions are unsupported on all current platforms except
  // iOS, where the pre-realization call must still fail because no responder
  // hierarchy exists yet.
#if defined(__APPLE__) && TARGET_OS_IPHONE
  assert(puglStartTextInput(test.view) != PUGL_SUCCESS);
#else
  assert(puglStartTextInput(test.view) == PUGL_UNSUPPORTED);
  assert(puglStopTextInput(test.view) == PUGL_UNSUPPORTED);
#endif

  // Check basic accessors
  assert(puglGetBackend(test.view) == puglStubBackend());
  assert(!strcmp(puglGetWorldString(test.world, PUGL_CLASS_NAME), "PuglTest"));
  assert(
    !strcmp(puglGetViewString(test.view, PUGL_WINDOW_TITLE), "Pugl View Test"));

  // Create and show window
  assert(!puglRealize(test.view));
  assert(puglShow(test.view, PUGL_SHOW_RAISE) <= PUGL_FAILURE);
  while (test.state < CONFIGURED) {
    assert(!puglUpdate(test.world, -1.0));
  }

  // Check that puglGetNativeView() returns something
  assert(puglGetNativeView(test.view));

  // Runtime title updates must allow clearing a realized native window title.
  assert(!puglSetViewString(test.view, PUGL_WINDOW_TITLE, "Updated title"));
  assert(!strcmp(puglGetViewString(test.view, PUGL_WINDOW_TITLE), "Updated title"));
  assert(!puglSetViewString(test.view, PUGL_WINDOW_TITLE, ""));
  assert(!puglGetViewString(test.view, PUGL_WINDOW_TITLE));
  assert(!puglSetViewString(test.view, PUGL_WINDOW_TITLE, "Restored title"));
  assert(!strcmp(puglGetViewString(test.view, PUGL_WINDOW_TITLE), "Restored title"));

  // Tear down
  puglFreeView(test.view);
  puglFreeWorld(test.world);

  return 0;
}
