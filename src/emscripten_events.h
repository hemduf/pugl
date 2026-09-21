// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#ifndef PUGL_SRC_EMSCRIPTEN_EVENTS_H
#define PUGL_SRC_EMSCRIPTEN_EVENTS_H

#include <pugl/pugl.h>

#include <stdbool.h>

PuglStatus puglEmscriptenRegisterInput(PuglView* view);
void       puglEmscriptenUnregisterInput(PuglView* view);
void       puglEmscriptenFreeInput(PuglView* view);
PuglStatus puglEmscriptenGrabFocus(PuglView* view);
bool       puglEmscriptenHasFocus(const PuglView* view);

#endif // PUGL_SRC_EMSCRIPTEN_EVENTS_H
