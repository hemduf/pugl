// Copyright 2021-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

// Tests graceful handling of bad calls

#undef NDEBUG

#include <puglutil/test_utils.h>

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <assert.h>
#include <stdbool.h>

typedef struct {
  PuglTestOptions opts;
  PuglWorld*      world;
  PuglView*       view;
  bool            exposed;
  unsigned        dataEvents;
} PuglTest;

static PuglStatus
onEvent(PuglView* view, const PuglEvent* event)
{
  static const PuglEvent close_event  = {{PUGL_CLOSE, 0U}};
  const PuglEvent        expose_event = {{PUGL_EXPOSE, 0U}};

  PuglTest* test = (PuglTest*)puglGetHandle(view);

  if (test->opts.verbose) {
    printEvent(event, "Event: ", true);
  }

  if (event->type == PUGL_DATA) {
    ++test->dataEvents;
  }

  if (event->type == PUGL_UPDATE) {
    assert(puglUpdate(test->world, -1.0) == PUGL_BAD_CALL);
  }

  if (event->type == PUGL_EXPOSE) {
    /* The commented checks are for functions that don't bother to check
       because they obviously shouldn't be called when exposing */

    assert(puglSetPositionHint(view, PUGL_CURRENT_POSITION, 1, 1) ==
           PUGL_BAD_CALL);
    assert(puglSetSizeHint(view, PUGL_CURRENT_SIZE, 1U, 1U) == PUGL_BAD_CALL);
    // assert(puglSetParent(view, 0U) == PUGL_BAD_CALL);
    // assert(puglSetTransientParent(view, 0U) == PUGL_BAD_CALL);
    // assert(puglRealize(view) == PUGL_BAD_CALL);
    // assert(puglUnrealize(view) == PUGL_BAD_CALL);
    // assert(puglShow(view, PUGL_SHOW_RAISE) == PUGL_BAD_CALL);
    // assert(puglHide(view) == PUGL_BAD_CALL);
    // assert(puglSetViewStyle(view, 0U) == PUGL_BAD_CALL);
    assert(puglObscureView(view) == PUGL_BAD_CALL);
    assert(puglObscureRegion(view, 0, 0, 32U, 32U) == PUGL_BAD_CALL);
    // assert(puglGrabFocus(view) == PUGL_BAD_CALL);
    // assert(puglPaste(view) == PUGL_BAD_CALL);
    // assert(puglAcceptOffer(view, NULL, 0U) == PUGL_BAD_CALL);
    // assert(puglSetClipboard(view, NULL, NULL, 0U) == PUGL_BAD_CALL);
    // assert(puglSetCursor(view, PUGL_CURSOR_ARROW) == PUGL_BAD_CALL);
    assert(puglSendEvent(view, &expose_event) == PUGL_FAILURE);
    assert(puglSendEvent(view, &close_event) == PUGL_FAILURE);
    test->exposed = true;
  }

  return PUGL_SUCCESS;
}

int
main(int argc, char** argv)
{
  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  assert(world);

  PuglTest test = {
    puglParseTestOptions(&argc, &argv), world, puglNewView(world), false, 0U};
  assert(test.view);

  // Set up view
  puglSetWorldString(test.world, PUGL_CLASS_NAME, "PuglTest");
  puglSetViewString(test.view, PUGL_WINDOW_TITLE, "Pugl Bad Call Test");
  puglSetBackend(test.view, puglStubBackend());
  puglSetHandle(test.view, &test);
  puglSetEventFunc(test.view, onEvent);
  puglSetSizeHint(test.view, PUGL_DEFAULT_SIZE, 256, 256);
  puglSetPositionHint(test.view, PUGL_DEFAULT_POSITION, 0, 0);

  // Realize, show, then update until the view is exposed
  assert(!puglRealize(test.view));

#if defined(__APPLE__)
  // The macOS backend used to declare puglRejectOffer() without defining it.
  // A synthetic drag offer with no active NSDraggingInfo must fail cleanly,
  // and this assertion also makes the test link the public symbol on macOS.
  const PuglDataOfferEvent inactive_drag_offer = {
    PUGL_DATA_OFFER, 0U, 0.0, 0.0, 0.0, PUGL_CLIPBOARD_DRAG};
  assert(puglRejectOffer(
           test.view, &inactive_drag_offer, 0, 0, 1U, 1U) ==
         PUGL_BAD_PARAMETER);
#endif

#if defined(_WIN32)
  // Win32 uses WM_DROPFILES, so native rejection happens after the OS drop but
  // still before Pugl exposes data.  Bad or inactive accept calls must fail,
  // and rejection must never dispatch data.
  const unsigned dataEventsBeforeReject = test.dataEvents;
  const PuglDataOfferEvent general_offer = {
    PUGL_DATA_OFFER, 0U, 0.0, 0.0, 0.0, PUGL_CLIPBOARD_GENERAL};
  const PuglDataOfferEvent drag_offer = {
    PUGL_DATA_OFFER, 0U, 0.0, 0.0, 0.0, PUGL_CLIPBOARD_DRAG};
  const PuglDataOfferEvent invalid_offer = {
    PUGL_DATA_OFFER, 0U, 0.0, 0.0, 0.0, (PuglClipboard)-1};

  assert(puglAcceptOffer(test.view,
                         NULL,
                         0U,
                         PUGL_DATA_ACTION_COPY,
                         0,
                         0,
                         1U,
                         1U) == PUGL_BAD_PARAMETER);
  assert(puglAcceptOffer(test.view,
                         &drag_offer,
                         0U,
                         PUGL_DATA_ACTION_COPY,
                         0,
                         0,
                         1U,
                         1U) == PUGL_BAD_PARAMETER);
  assert(test.dataEvents == dataEventsBeforeReject);

  assert(puglRejectOffer(test.view, NULL, 0, 0, 1U, 1U) ==
         PUGL_BAD_PARAMETER);
  assert(puglRejectOffer(test.view, &general_offer, 0, 0, 1U, 1U) ==
         PUGL_SUCCESS);
  assert(test.dataEvents == dataEventsBeforeReject);
  assert(puglRejectOffer(test.view, &drag_offer, 0, 0, 1U, 1U) ==
         PUGL_SUCCESS);
  assert(test.dataEvents == dataEventsBeforeReject);
  assert(puglRejectOffer(test.view, &invalid_offer, 0, 0, 1U, 1U) ==
         PUGL_BAD_PARAMETER);
  assert(test.dataEvents == dataEventsBeforeReject);
#endif

  assert(puglShow(test.view, PUGL_SHOW_RAISE) <= PUGL_FAILURE);
  while (!test.exposed) {
    assert(!puglUpdate(test.world, -1.0));
  }

  // Tear down
  puglFreeView(test.view);
  puglFreeWorld(world);

  return 0;
}
