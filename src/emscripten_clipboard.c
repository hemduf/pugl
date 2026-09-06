// Copyright 2026 Fabrizio Duhem
// SPDX-License-Identifier: ISC

#include "emscripten_events.h"
#include "emscripten_platform.h"

#include <pugl/pugl.h>

#include <stddef.h>
#include <stdint.h>

PuglStatus
puglEmscriptenRejectOffer(PuglView* const                 view,
                          const PuglDataOfferEvent* const offer,
                          const int                       regionX,
                          const int                       regionY,
                          const unsigned                  regionWidth,
                          const unsigned                  regionHeight)
{
  (void)regionX;
  (void)regionY;
  (void)regionWidth;
  (void)regionHeight;

  if (!view || !view->impl || !offer ||
      offer->clipboard != PUGL_CLIPBOARD_GENERAL) {
    return PUGL_BAD_PARAMETER;
  }

  puglEmscriptenClearClipboard(view);
  return PUGL_SUCCESS;
}

uint32_t
puglEmscriptenGetNumClipboardTypes(const PuglView* const view,
                                   const PuglClipboard   clipboard)
{
  return view && view->impl && clipboard == PUGL_CLIPBOARD_GENERAL &&
             view->impl->clipboardOffer
           ? 1U
           : 0U;
}

const char*
puglEmscriptenGetClipboardType(const PuglView* const view,
                                const PuglClipboard   clipboard,
                                const uint32_t        typeIndex)
{
  return puglEmscriptenGetNumClipboardTypes(view, clipboard) && typeIndex == 0U
           ? "text/plain;charset=utf-8"
           : NULL;
}

const void*
puglEmscriptenGetClipboard(PuglView* const     view,
                           const PuglClipboard clipboard,
                           const uint32_t      typeIndex,
                           size_t* const       len)
{
  if (len) {
    *len = 0U;
  }

  if (!view || !view->impl || clipboard != PUGL_CLIPBOARD_GENERAL ||
      typeIndex != 0U || !view->impl->clipboardData.data) {
    return NULL;
  }

  if (len) {
    *len = view->impl->clipboardData.len;
  }

  return view->impl->clipboardData.data;
}
