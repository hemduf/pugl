// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include <pugl/pugl.h>

#include <assert.h>
#include <stddef.h>

int
main(void)
{
  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  assert(world);

  PuglView* const view = puglNewView(world);
  assert(view);

  assert(puglSetTextInputFlags(NULL, 0U) == PUGL_BAD_PARAMETER);
  assert(puglSetTextInputFlags(view, 0U) == PUGL_SUCCESS);
  assert(puglSetTextInputFlags(view, PUGL_TEXT_INPUT_HAS_TEXT) == PUGL_SUCCESS);
  assert(puglSetTextInputFlags(view, 1U << 31U) == PUGL_BAD_PARAMETER);

  // PuglEventType is public ABI.  The text-edit event must remain append-only
  // without renumbering the event values that existed before this extension.
  assert(PUGL_NOTHING == 0);
  assert(PUGL_DATA == 23);
  assert(PUGL_POINTER_DOWN == 24);
  assert(PUGL_POINTER_MOVE == 25);
  assert(PUGL_POINTER_UP == 26);
  assert(PUGL_POINTER_CANCEL == 27);
  assert(PUGL_TEXT_EDIT == 28);

  PuglEvent event = {{PUGL_NOTHING, 0U}};
  event.textEdit = (PuglTextEditEvent){
    PUGL_TEXT_EDIT,
    0U,
    1.0,
    PUGL_TEXT_DELETE_BACKWARD,
  };

  assert(event.type == PUGL_TEXT_EDIT);
  assert(event.textEdit.edit == PUGL_TEXT_DELETE_BACKWARD);

  // PuglScrollEvent was already one of the largest event members before
  // PUGL_TEXT_EDIT.  Keeping the union exactly this size proves that adding the
  // semantic edit event did not grow the public PuglEvent ABI on this target.
  assert(sizeof(PuglTextEditEvent) <= sizeof(PuglScrollEvent));
  assert(sizeof(PuglEvent) == sizeof(PuglScrollEvent));

  assert(!puglIsTextInputActive(view));
  assert(puglStartTextInput(view) == PUGL_UNSUPPORTED);
  assert(puglStopTextInput(view) == PUGL_UNSUPPORTED);

  puglFreeView(view);
  puglFreeWorld(world);
  return 0;
}
