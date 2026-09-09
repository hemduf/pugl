// Copyright 2026 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

// Regression test for paste requests when no X11 CLIPBOARD owner exists.

#undef NDEBUG

#include <puglutil/test_utils.h>

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <assert.h>
#include <stdbool.h>

typedef struct {
  bool     exposed;
  unsigned offers;
  unsigned data;
} EventCounts;

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
  EventCounts* const counts = (EventCounts*)puglGetHandle(view);
  if (event->type == PUGL_EXPOSE) {
    counts->exposed = true;
  } else if (event->type == PUGL_DATA_OFFER) {
    ++counts->offers;
  } else if (event->type == PUGL_DATA) {
    ++counts->data;
  }

  return PUGL_SUCCESS;
}

int
main(int argc, char** argv)
{
  puglParseTestOptions(&argc, &argv);

  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0);
  assert(world);
  puglSetWorldString(world, PUGL_CLASS_NAME, "PuglTest");

  EventCounts     counts = {false, 0U, 0U};
  PuglView* const view   = puglNewView(world);
  assert(view);
  puglSetViewString(view, PUGL_WINDOW_TITLE, "Pugl Empty Clipboard Test");
  puglSetBackend(view, puglStubBackend());
  puglSetHandle(view, &counts);
  puglSetEventFunc(view, onEvent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 256, 256);
  assert(puglShow(view, PUGL_SHOW_RAISE) <= PUGL_FAILURE);

  while (!counts.exposed) {
    assert(!puglUpdate(world, 0.0));
  }

  // On a fresh X server there is no CLIPBOARD owner. XConvertSelection then
  // returns SelectionNotify with property == None. Processing that response
  // must not pass atom 0 to XGetWindowProperty, emit an X11 BadAtom error, or
  // manufacture a clipboard offer/data event for a failed conversion.
  assert(!puglPaste(view));
  for (unsigned i = 0U; i < 8U; ++i) {
    assert(!puglUpdate(world, 1 / 120.0));
  }
  assert(counts.offers == 0U);
  assert(counts.data == 0U);

  puglFreeView(view);
  puglFreeWorld(world);
  return 0;
}
