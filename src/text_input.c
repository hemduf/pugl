// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include "types.h"

#include <pugl/pugl.h>

PuglStatus
puglSetTextInputFlags(PuglView* const view, const PuglTextInputFlags flags)
{
  if (!view) {
    return PUGL_BAD_PARAMETER;
  }

  if (flags & ~(PuglTextInputFlags)PUGL_TEXT_INPUT_HAS_TEXT) {
    return PUGL_BAD_PARAMETER;
  }

  view->textInputFlags = flags;
  return PUGL_SUCCESS;
}
