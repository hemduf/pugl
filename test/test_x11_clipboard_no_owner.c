// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

/*
  Regression test for an X11 clipboard conversion with no selection owner.

  X11 reports failed conversions with SelectionNotify.property == None.  Pugl
  must not pass that sentinel to XGetWindowProperty(), which would trigger a
  process-wide BadAtom error.
*/

#undef NDEBUG

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <X11/X.h>
#include <X11/Xlib.h>

#include <assert.h>
#include <stddef.h>

typedef struct {
  unsigned dataOffers;
} TestState;

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
  TestState* const state = (TestState*)puglGetHandle(view);
  if (event->type == PUGL_DATA_OFFER) {
    ++state->dataOffers;
  }

  return PUGL_SUCCESS;
}

int
main(void)
{
  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  assert(world);

  TestState state = {0U};
  PuglView* const view = puglNewView(world);
  assert(view);

  assert(!puglSetBackend(view, puglStubBackend()));
  puglSetHandle(view, &state);
  puglSetEventFunc(view, onEvent);
  puglSetDefaultSize(view, 64U, 64U);
  assert(!puglRealize(view));

  Display* const display = (Display*)puglGetNativeWorld(world);
  assert(display);

  const Atom clipboard = XInternAtom(display, "CLIPBOARD", False);
  assert(clipboard != None);

  // Make the no-owner condition deterministic even on an interactive desktop.
  XSetSelectionOwner(display, clipboard, None, CurrentTime);
  XSync(display, False);
  assert(XGetSelectionOwner(display, clipboard) == None);

  // This produces SelectionNotify.property == None.  Before the regression
  // fix, dispatching it terminated the process with X11 BadAtom.
  assert(!puglPaste(view));
  for (unsigned i = 0U; i < 4U; ++i) {
    assert(!puglUpdate(world, 0.0));
  }

  assert(state.dataOffers == 0U);

  puglFreeView(view);
  puglFreeWorld(world);
  return 0;
}
