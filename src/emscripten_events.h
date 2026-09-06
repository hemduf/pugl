// Copyright 2026 Fabrizio Duhem
// SPDX-License-Identifier: ISC

#ifndef PUGL_SRC_EMSCRIPTEN_EVENTS_H
#define PUGL_SRC_EMSCRIPTEN_EVENTS_H

#include <pugl/pugl.h>

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
PuglStatus puglEmscriptenRejectOffer(PuglView* view,
                                     const PuglDataOfferEvent* offer,
                                     int regionX,
                                     int regionY,
                                     unsigned regionWidth,
                                     unsigned regionHeight);
uint32_t puglEmscriptenGetNumClipboardTypes(const PuglView* view,
                                            PuglClipboard clipboard);
const char* puglEmscriptenGetClipboardType(const PuglView* view,
                                           PuglClipboard clipboard,
                                           uint32_t typeIndex);
PuglStatus puglEmscriptenSetClipboard(PuglView* view,
                                      PuglClipboard clipboard,
                                      const char* type,
                                      const void* data,
                                      size_t len);
const void* puglEmscriptenGetClipboard(PuglView* view,
                                       PuglClipboard clipboard,
                                       uint32_t typeIndex,
                                       size_t* len);
void puglEmscriptenClearClipboard(PuglView* view);

#endif // PUGL_SRC_EMSCRIPTEN_EVENTS_H
