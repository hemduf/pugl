// Copyright 2026 Fabrizio Duhem
// SPDX-License-Identifier: ISC

#ifndef PUGL_SRC_EMSCRIPTEN_EVENTS_H
#define PUGL_SRC_EMSCRIPTEN_EVENTS_H

#include "emscripten_platform.h"

#include <pugl/pugl.h>

#include <stddef.h>
#include <stdint.h>

PuglStatus puglEmscriptenRegisterCallbacks(PuglView* view);
void       puglEmscriptenUnregisterCallbacks(PuglView* view);

PuglStatus puglEmscriptenStartTimer(PuglView* view,
                                    uintptr_t id,
                                    double timeout);
PuglStatus puglEmscriptenStopTimer(PuglView* view, uintptr_t id);
void       puglEmscriptenClearTimers(PuglView* view);

PuglStatus puglEmscriptenPollClipboard(PuglView* view);
PuglStatus puglEmscriptenPaste(PuglView* view);
PuglStatus puglEmscriptenAcceptOffer(PuglView* view,
                                     const PuglDataOfferEvent* offer,
                                     uint32_t typeIndex,
                                     PuglDataAction action,
                                     int regionX,
                                     int regionY,
                                     unsigned regionWidth,
                                     unsigned regionHeight);
PuglStatus puglEmscriptenSetClipboard(PuglView* view,
                                      PuglClipboard clipboard,
                                      const char* type,
                                      const void* data,
                                      size_t len);
void puglEmscriptenClearClipboard(PuglView* view);

static inline PuglStatus
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

static inline uint32_t
puglEmscriptenGetNumClipboardTypes(const PuglView* const view,
                                   const PuglClipboard   clipboard)
{
  return view && view->impl && clipboard == PUGL_CLIPBOARD_GENERAL &&
             view->impl->clipboardOffer
           ? 1U
           : 0U;
}

static inline const char*
puglEmscriptenGetClipboardType(const PuglView* const view,
                               const PuglClipboard   clipboard,
                               const uint32_t        typeIndex)
{
  return puglEmscriptenGetNumClipboardTypes(view, clipboard) && typeIndex == 0U
           ? "text/plain;charset=utf-8"
           : NULL;
}

static inline const void*
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

#endif // PUGL_SRC_EMSCRIPTEN_EVENTS_H
