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

  PuglEvent event = {{PUGL_NOTHING, 0U}};
  event.textEdit = (PuglTextEditEvent){
    PUGL_TEXT_EDIT,
    0U,
    1.0,
    PUGL_TEXT_DELETE_BACKWARD,
  };

  assert(event.type == PUGL_TEXT_EDIT);
  assert(event.textEdit.edit == PUGL_TEXT_DELETE_BACKWARD);
  assert(sizeof(PuglTextEditEvent) <= sizeof(PuglScrollEvent));
  assert(sizeof(PuglEvent) >= sizeof(PuglTextEditEvent));

  assert(!puglIsTextInputActive(view));
  assert(puglStartTextInput(view) == PUGL_UNSUPPORTED);
  assert(puglStopTextInput(view) == PUGL_UNSUPPORTED);

  puglFreeView(view);
  puglFreeWorld(world);
  return 0;
}
