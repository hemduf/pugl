// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include <pugl/pugl.h>

#include <assert.h>
#include <math.h>
#include <stddef.h>

int
main(void)
{
  PuglEvent event = {{PUGL_NOTHING, 0U}};

  event.pointer = (PuglPointerEvent){
    PUGL_POINTER_DOWN,
    0U,
    1.0,
    12.0,
    34.0,
    0U,
    42U,
    PUGL_POINTER_TOUCH,
    PUGL_POINTER_IS_PRIMARY,
    0.5,
    8.0,
    10.0,
  };

  assert(event.type == PUGL_POINTER_DOWN);
  assert(event.pointer.id != PUGL_POINTER_ID_NONE);
  assert(event.pointer.id == 42U);
  assert(event.pointer.pointerType == PUGL_POINTER_TOUCH);
  assert(event.pointer.pointerFlags & PUGL_POINTER_IS_PRIMARY);
  assert(fabs(event.pointer.pressure - 0.5) < 1e-12);
  // Adding raw pointer contacts must not enlarge the public event union.
  // PuglScrollEvent was already one of the largest event payloads.
  assert(sizeof(PuglPointerEvent) <= sizeof(PuglScrollEvent));
  assert(sizeof(PuglEvent) >= sizeof(PuglPointerEvent));

  return 0;
}
