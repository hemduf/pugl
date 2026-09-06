// Copyright 2026 Fabrizio Duhem
// SPDX-License-Identifier: ISC

#include "emscripten_events.h"

#include "emscripten_platform.h"
#include "internal.h"
#include "types.h"

#include <pugl/pugl.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PUGL_EMSCRIPTEN_CLIPBOARD_PENDING (-1)
#define PUGL_EMSCRIPTEN_CLIPBOARD_FAILED (-2)

EM_JS(int, puglBrowserClipboardWrite, (const char* data, size_t len), {
  if (typeof navigator === 'undefined' || !navigator.clipboard ||
      typeof navigator.clipboard.writeText !== 'function') {
    return 0;
  }

  const text = UTF8ToString(data, len);
  navigator.clipboard.writeText(text).catch(() => {});
  return 1;
});

EM_JS(int, puglBrowserClipboardRequest, (unsigned id), {
  if (typeof navigator === 'undefined' || !navigator.clipboard ||
      typeof navigator.clipboard.readText !== 'function') {
    return 0;
  }

  const requests = globalThis.__puglClipboardRequests ||
                   (globalThis.__puglClipboardRequests = new Map());
  requests.set(id, {state: 'pending', text: ''});

  navigator.clipboard.readText().then((text) => {
    const current = requests.get(id);
    if (current) {
      current.state = 'ready';
      current.text = String(text);
    }
  }).catch(() => {
    const current = requests.get(id);
    if (current) {
      current.state = 'failed';
      current.text = '';
    }
  });

  return 1;
});

EM_JS(int, puglBrowserClipboardLength, (unsigned id), {
  const requests = globalThis.__puglClipboardRequests;
  const request = requests ? requests.get(id) : null;
  if (!request || request.state === 'pending') {
    return -1;
  }
  if (request.state === 'failed') {
    return -2;
  }
  return lengthBytesUTF8(request.text);
});

EM_JS(int,
      puglBrowserClipboardRead,
      (unsigned id, char* data, size_t capacity),
      {
        const requests = globalThis.__puglClipboardRequests;
        const request = requests ? requests.get(id) : null;
        if (!request || request.state !== 'ready' || !capacity) {
          return 0;
        }

        stringToUTF8(request.text, data, capacity);
        requests.delete(id);
        return 1;
      });

EM_JS(void, puglBrowserClipboardCancel, (unsigned id), {
  const requests = globalThis.__puglClipboardRequests;
  if (requests) {
    requests.delete(id);
  }
});

static PuglMods
puglEmscriptenMods(const bool ctrl,
                   const bool shift,
                   const bool alt,
                   const bool meta)
{
  return (ctrl ? PUGL_MOD_CTRL : 0U) | (shift ? PUGL_MOD_SHIFT : 0U) |
         (alt ? PUGL_MOD_ALT : 0U) | (meta ? PUGL_MOD_SUPER : 0U);
}

static uint32_t
puglEmscriptenSpecialKey(const EmscriptenKeyboardEvent* const event)
{
  const char* const key  = event->key;
  const char* const code = event->code;

  if (!strncmp(code, "Numpad", 6U)) {
    if (code[6] >= '0' && code[6] <= '9' && code[7] == '\0') {
      return PUGL_KEY_PAD_0 + (uint32_t)(code[6] - '0');
    }
    if (!strcmp(code, "NumpadEnter")) {
      return PUGL_KEY_PAD_ENTER;
    }
    if (!strcmp(code, "NumpadEqual")) {
      return PUGL_KEY_PAD_EQUAL;
    }
    if (!strcmp(code, "NumpadMultiply")) {
      return PUGL_KEY_PAD_MULTIPLY;
    }
    if (!strcmp(code, "NumpadAdd")) {
      return PUGL_KEY_PAD_ADD;
    }
    if (!strcmp(code, "NumpadSubtract")) {
      return PUGL_KEY_PAD_SUBTRACT;
    }
    if (!strcmp(code, "NumpadDecimal")) {
      return PUGL_KEY_PAD_DECIMAL;
    }
    if (!strcmp(code, "NumpadDivide")) {
      return PUGL_KEY_PAD_DIVIDE;
    }
  }

  if (!strcmp(key, "Backspace")) {
    return PUGL_KEY_BACKSPACE;
  }
  if (!strcmp(key, "Tab")) {
    return PUGL_KEY_TAB;
  }
  if (!strcmp(key, "Enter")) {
    return PUGL_KEY_ENTER;
  }
  if (!strcmp(key, "Escape")) {
    return PUGL_KEY_ESCAPE;
  }
  if (!strcmp(key, "Delete")) {
    return PUGL_KEY_DELETE;
  }
  if (!strcmp(key, " ") || !strcmp(key, "Spacebar")) {
    return PUGL_KEY_SPACE;
  }
  if (!strcmp(key, "PageUp")) {
    return PUGL_KEY_PAGE_UP;
  }
  if (!strcmp(key, "PageDown")) {
    return PUGL_KEY_PAGE_DOWN;
  }
  if (!strcmp(key, "End")) {
    return PUGL_KEY_END;
  }
  if (!strcmp(key, "Home")) {
    return PUGL_KEY_HOME;
  }
  if (!strcmp(key, "ArrowLeft")) {
    return PUGL_KEY_LEFT;
  }
  if (!strcmp(key, "ArrowUp")) {
    return PUGL_KEY_UP;
  }
  if (!strcmp(key, "ArrowRight")) {
    return PUGL_KEY_RIGHT;
  }
  if (!strcmp(key, "ArrowDown")) {
    return PUGL_KEY_DOWN;
  }
  if (!strcmp(key, "PrintScreen")) {
    return PUGL_KEY_PRINT_SCREEN;
  }
  if (!strcmp(key, "Insert")) {
    return PUGL_KEY_INSERT;
  }
  if (!strcmp(key, "Pause")) {
    return PUGL_KEY_PAUSE;
  }
  if (!strcmp(key, "ContextMenu")) {
    return PUGL_KEY_MENU;
  }
  if (!strcmp(key, "NumLock")) {
    return PUGL_KEY_NUM_LOCK;
  }
  if (!strcmp(key, "ScrollLock")) {
    return PUGL_KEY_SCROLL_LOCK;
  }
  if (!strcmp(key, "CapsLock")) {
    return PUGL_KEY_CAPS_LOCK;
  }

  if (!strcmp(key, "Shift")) {
    return event->location == DOM_KEY_LOCATION_RIGHT ? PUGL_KEY_SHIFT_R
                                                     : PUGL_KEY_SHIFT_L;
  }
  if (!strcmp(key, "Control")) {
    return event->location == DOM_KEY_LOCATION_RIGHT ? PUGL_KEY_CTRL_R
                                                     : PUGL_KEY_CTRL_L;
  }
  if (!strcmp(key, "Alt") || !strcmp(key, "AltGraph")) {
    return event->location == DOM_KEY_LOCATION_RIGHT ? PUGL_KEY_ALT_R
                                                     : PUGL_KEY_ALT_L;
  }
  if (!strcmp(key, "Meta") || !strcmp(key, "OS")) {
    return event->location == DOM_KEY_LOCATION_RIGHT ? PUGL_KEY_SUPER_R
                                                     : PUGL_KEY_SUPER_L;
  }

  if (key[0] == 'F' && key[1] >= '1' && key[1] <= '9') {
    const int value = atoi(key + 1);
    if (value >= 1 && value <= 12) {
      return PUGL_KEY_F1 + (uint32_t)(value - 1);
    }
  }

  return PUGL_KEY_NONE;
}

static uint32_t
puglEmscriptenKey(const EmscriptenKeyboardEvent* const event)
{
  const uint32_t special = puglEmscriptenSpecialKey(event);
  if (special) {
    return special;
  }

  if (!strncmp(event->code, "Key", 3U) && event->code[3] >= 'A' &&
      event->code[3] <= 'Z' && event->code[4] == '\0') {
    return (uint32_t)tolower((unsigned char)event->code[3]);
  }

  if (!strncmp(event->code, "Digit", 5U) && event->code[5] >= '0' &&
      event->code[5] <= '9' && event->code[6] == '\0') {
    return (uint32_t)event->code[5];
  }

  const size_t length = strlen(event->key);
  if (length == 1U) {
    const unsigned char c = (unsigned char)event->key[0];
    return (uint32_t)(isupper(c) ? tolower(c) : c);
  }

  if (length > 1U && length <= 4U) {
    return puglDecodeUTF8((const uint8_t*)event->key);
  }

  return PUGL_KEY_NONE;
}

static size_t
puglEmscriptenCharacterLength(const char* const text)
{
  const unsigned char first = (unsigned char)text[0];
  if (!first) {
    return 0U;
  }
  if (first < 0x80U) {
    return text[1] == '\0' ? 1U : 0U;
  }

  const size_t expected = (first & 0xE0U) == 0xC0U   ? 2U
                          : (first & 0xF0U) == 0xE0U ? 3U
                          : (first & 0xF8U) == 0xF0U ? 4U
                                                    : 0U;
  if (!expected || strlen(text) != expected) {
    return 0U;
  }

  for (size_t i = 1U; i < expected; ++i) {
    if (((unsigned char)text[i] & 0xC0U) != 0x80U) {
      return 0U;
    }
  }

  return expected;
}

static PuglStatus
puglEmscriptenDispatchText(PuglView* const                       view,
                           const EmscriptenKeyboardEvent* const browserEvent,
                           const PuglMods                       mods)
{
  if (browserEvent->ctrlKey || browserEvent->altKey || browserEvent->metaKey) {
    return PUGL_SUCCESS;
  }

  const size_t length = puglEmscriptenCharacterLength(browserEvent->key);
  if (!length || length >= sizeof(((PuglTextEvent*)0)->string)) {
    return PUGL_SUCCESS;
  }

  PuglEvent event      = {0};
  event.text.type      = PUGL_TEXT;
  event.text.time      = puglGetTime(view->world);
  event.text.x         = view->impl->pointerX;
  event.text.y         = view->impl->pointerY;
  event.text.xRoot     = view->impl->pointerRootX;
  event.text.yRoot     = view->impl->pointerRootY;
  event.text.state     = mods;
  event.text.keycode   = browserEvent->which ? browserEvent->which
                                              : browserEvent->keyCode;
  event.text.character = puglDecodeUTF8((const uint8_t*)browserEvent->key);
  memcpy(event.text.string, browserEvent->key, length);
  event.text.string[length] = '\0';
  return puglDispatchEvent(view, &event);
}

static void
puglEmscriptenRememberPointer(PuglView* const                   view,
                              const EmscriptenMouseEvent* const event)
{
  view->impl->pointerX     = event->targetX;
  view->impl->pointerY     = event->targetY;
  view->impl->pointerRootX = event->screenX;
  view->impl->pointerRootY = event->screenY;
}

static bool
puglEmscriptenKeyCallback(const int                            eventType,
                          const EmscriptenKeyboardEvent* const browserEvent,
                          void* const                          userData)
{
  PuglView* const view = (PuglView*)userData;
  if (!view || !view->impl || !view->impl->id) {
    return false;
  }

  const PuglMods mods = puglEmscriptenMods(browserEvent->ctrlKey,
                                           browserEvent->shiftKey,
                                           browserEvent->altKey,
                                           browserEvent->metaKey);
  const PuglKey key = (PuglKey)puglEmscriptenKey(browserEvent);

  if (eventType == EMSCRIPTEN_EVENT_KEYDOWN && browserEvent->repeat &&
      view->hints[PUGL_IGNORE_KEY_REPEAT] == PUGL_TRUE) {
    return true;
  }

  PuglEvent event   = {0};
  event.key.type    = eventType == EMSCRIPTEN_EVENT_KEYDOWN ? PUGL_KEY_PRESS
                                                             : PUGL_KEY_RELEASE;
  event.key.time    = puglGetTime(view->world);
  event.key.x       = view->impl->pointerX;
  event.key.y       = view->impl->pointerY;
  event.key.xRoot   = view->impl->pointerRootX;
  event.key.yRoot   = view->impl->pointerRootY;
  event.key.state   = puglFilterMods(mods, key);
  event.key.keycode = browserEvent->which ? browserEvent->which
                                           : browserEvent->keyCode;
  event.key.key     = (uint32_t)key;

  const PuglStatus keyStatus = puglDispatchEvent(view, &event);
  if (keyStatus || eventType != EMSCRIPTEN_EVENT_KEYDOWN) {
    return keyStatus == PUGL_SUCCESS;
  }

  return puglEmscriptenDispatchText(view, browserEvent, mods) == PUGL_SUCCESS;
}

static uint32_t
puglEmscriptenButton(const unsigned short button)
{
  switch (button) {
  case 0U:
    return 0U;
  case 1U:
    return 2U;
  case 2U:
    return 1U;
  default:
    return (uint32_t)button;
  }
}

static bool
puglEmscriptenMouseCallback(const int                         eventType,
                            const EmscriptenMouseEvent* const browserEvent,
                            void* const                       userData)
{
  PuglView* const view = (PuglView*)userData;
  if (!view || !view->impl || !view->impl->id) {
    return false;
  }

  puglEmscriptenRememberPointer(view, browserEvent);
  const PuglMods mods = puglEmscriptenMods(browserEvent->ctrlKey,
                                           browserEvent->shiftKey,
                                           browserEvent->altKey,
                                           browserEvent->metaKey);
  PuglEvent event = {0};

  switch (eventType) {
  case EMSCRIPTEN_EVENT_MOUSEENTER:
  case EMSCRIPTEN_EVENT_MOUSELEAVE:
    event.crossing.type = eventType == EMSCRIPTEN_EVENT_MOUSEENTER
                            ? PUGL_POINTER_IN
                            : PUGL_POINTER_OUT;
    event.crossing.time  = puglGetTime(view->world);
    event.crossing.x     = browserEvent->targetX;
    event.crossing.y     = browserEvent->targetY;
    event.crossing.xRoot = browserEvent->screenX;
    event.crossing.yRoot = browserEvent->screenY;
    event.crossing.state = mods;
    event.crossing.mode  = PUGL_CROSSING_NORMAL;
    break;

  case EMSCRIPTEN_EVENT_MOUSEDOWN:
  case EMSCRIPTEN_EVENT_MOUSEUP:
    event.button.type = eventType == EMSCRIPTEN_EVENT_MOUSEDOWN
                          ? PUGL_BUTTON_PRESS
                          : PUGL_BUTTON_RELEASE;
    event.button.time   = puglGetTime(view->world);
    event.button.x      = browserEvent->targetX;
    event.button.y      = browserEvent->targetY;
    event.button.xRoot  = browserEvent->screenX;
    event.button.yRoot  = browserEvent->screenY;
    event.button.state  = mods;
    event.button.button = puglEmscriptenButton(browserEvent->button);
    break;

  case EMSCRIPTEN_EVENT_MOUSEMOVE:
    event.motion.type  = PUGL_MOTION;
    event.motion.time  = puglGetTime(view->world);
    event.motion.x     = browserEvent->targetX;
    event.motion.y     = browserEvent->targetY;
    event.motion.xRoot = browserEvent->screenX;
    event.motion.yRoot = browserEvent->screenY;
    event.motion.state = mods;
    break;

  default:
    return false;
  }

  return puglDispatchEvent(view, &event) == PUGL_SUCCESS;
}

static bool
puglEmscriptenWheelCallback(const int                         eventType,
                            const EmscriptenWheelEvent* const browserEvent,
                            void* const                       userData)
{
  (void)eventType;

  PuglView* const view = (PuglView*)userData;
  if (!view || !view->impl || !view->impl->id) {
    return false;
  }

  puglEmscriptenRememberPointer(view, &browserEvent->mouse);

  double factor = 1.0;
  if (browserEvent->deltaMode == DOM_DELTA_PIXEL) {
    factor = 1.0 / 40.0;
  } else if (browserEvent->deltaMode == DOM_DELTA_PAGE) {
    factor = 24.0;
  }

  PuglEvent event         = {0};
  event.scroll.type       = PUGL_SCROLL;
  event.scroll.time       = puglGetTime(view->world);
  event.scroll.x          = browserEvent->mouse.targetX;
  event.scroll.y          = browserEvent->mouse.targetY;
  event.scroll.xRoot      = browserEvent->mouse.screenX;
  event.scroll.yRoot      = browserEvent->mouse.screenY;
  event.scroll.state      = puglEmscriptenMods(browserEvent->mouse.ctrlKey,
                                               browserEvent->mouse.shiftKey,
                                               browserEvent->mouse.altKey,
                                               browserEvent->mouse.metaKey);
  event.scroll.direction  = PUGL_SCROLL_SMOOTH;
  event.scroll.dx         = browserEvent->deltaX * factor;
  event.scroll.dy         = -browserEvent->deltaY * factor;

  return puglDispatchEvent(view, &event) == PUGL_SUCCESS;
}

static bool
puglEmscriptenFocusCallback(const int                         eventType,
                            const EmscriptenFocusEvent* const browserEvent,
                            void* const                       userData)
{
  (void)browserEvent;

  PuglView* const view = (PuglView*)userData;
  if (!view || !view->impl || !view->impl->id) {
    return false;
  }

  PuglEvent event  = {0};
  event.focus.type = eventType == EMSCRIPTEN_EVENT_FOCUS ? PUGL_FOCUS_IN
                                                          : PUGL_FOCUS_OUT;
  event.focus.mode = PUGL_CROSSING_NORMAL;
  return puglDispatchEvent(view, &event) == PUGL_SUCCESS;
}

static PuglStatus
puglEmscriptenRegistrationStatus(const EMSCRIPTEN_RESULT result)
{
  return result == EMSCRIPTEN_RESULT_SUCCESS ? PUGL_SUCCESS
                                             : PUGL_REGISTRATION_FAILED;
}

PuglStatus
puglEmscriptenRegisterCallbacks(PuglView* const view)
{
  const char* const target = view->impl->canvasSelector;
  PuglStatus status = PUGL_SUCCESS;

#define REGISTER(call)                                                         \
  do {                                                                         \
    const PuglStatus s = puglEmscriptenRegistrationStatus(call);              \
    if (!status && s) {                                                        \
      status = s;                                                              \
    }                                                                          \
  } while (0)

  REGISTER(emscripten_set_keydown_callback(
    target, view, false, puglEmscriptenKeyCallback));
  REGISTER(emscripten_set_keyup_callback(
    target, view, false, puglEmscriptenKeyCallback));
  REGISTER(emscripten_set_mouseenter_callback(
    target, view, false, puglEmscriptenMouseCallback));
  REGISTER(emscripten_set_mouseleave_callback(
    target, view, false, puglEmscriptenMouseCallback));
  REGISTER(emscripten_set_mousedown_callback(
    target, view, false, puglEmscriptenMouseCallback));
  REGISTER(emscripten_set_mouseup_callback(
    target, view, false, puglEmscriptenMouseCallback));
  REGISTER(emscripten_set_mousemove_callback(
    target, view, false, puglEmscriptenMouseCallback));
  REGISTER(emscripten_set_wheel_callback(
    target, view, false, puglEmscriptenWheelCallback));
  REGISTER(emscripten_set_focus_callback(
    target, view, false, puglEmscriptenFocusCallback));
  REGISTER(emscripten_set_blur_callback(
    target, view, false, puglEmscriptenFocusCallback));

#undef REGISTER

  if (status) {
    puglEmscriptenUnregisterCallbacks(view);
  }

  return status;
}

void
puglEmscriptenUnregisterCallbacks(PuglView* const view)
{
  if (!view || !view->impl || !view->impl->id) {
    return;
  }

  const char* const target = view->impl->canvasSelector;
  (void)emscripten_set_keydown_callback(target, view, false, NULL);
  (void)emscripten_set_keyup_callback(target, view, false, NULL);
  (void)emscripten_set_mouseenter_callback(target, view, false, NULL);
  (void)emscripten_set_mouseleave_callback(target, view, false, NULL);
  (void)emscripten_set_mousedown_callback(target, view, false, NULL);
  (void)emscripten_set_mouseup_callback(target, view, false, NULL);
  (void)emscripten_set_mousemove_callback(target, view, false, NULL);
  (void)emscripten_set_wheel_callback(target, view, false, NULL);
  (void)emscripten_set_focus_callback(target, view, false, NULL);
  (void)emscripten_set_blur_callback(target, view, false, NULL);
}

static void
puglEmscriptenTimerCallback(void* const data)
{
  PuglEmscriptenTimer* const timer = (PuglEmscriptenTimer*)data;
  if (!timer || !timer->view || !timer->view->impl || !timer->view->impl->id) {
    return;
  }

  PuglEvent event = {0};
  event.timer.type = PUGL_TIMER;
  event.timer.id   = timer->id;
  (void)puglDispatchEvent(timer->view, &event);
}

PuglStatus
puglEmscriptenStartTimer(PuglView* const view,
                         const uintptr_t id,
                         const double timeout)
{
  if (!view || !view->impl || !view->impl->id || !isfinite(timeout) ||
      timeout <= 0.0) {
    return PUGL_BAD_PARAMETER;
  }

  (void)puglEmscriptenStopTimer(view, id);

  PuglEmscriptenTimer* const timer =
    (PuglEmscriptenTimer*)calloc(1U, sizeof(PuglEmscriptenTimer));
  if (!timer) {
    return PUGL_NO_MEMORY;
  }

  timer->view = view;
  timer->id   = id;
  timer->intervalHandle =
    emscripten_set_interval(puglEmscriptenTimerCallback, timeout * 1000.0, timer);

  if (timer->intervalHandle < 0) {
    free(timer);
    return PUGL_FAILURE;
  }

  timer->next        = view->impl->timers;
  view->impl->timers = timer;
  return PUGL_SUCCESS;
}

PuglStatus
puglEmscriptenStopTimer(PuglView* const view, const uintptr_t id)
{
  if (!view || !view->impl) {
    return PUGL_BAD_PARAMETER;
  }

  PuglEmscriptenTimer** link = &view->impl->timers;
  while (*link) {
    PuglEmscriptenTimer* const timer = *link;
    if (timer->id == id) {
      *link = timer->next;
      emscripten_clear_interval(timer->intervalHandle);
      free(timer);
      return PUGL_SUCCESS;
    }
    link = &timer->next;
  }

  return PUGL_FAILURE;
}

void
puglEmscriptenClearTimers(PuglView* const view)
{
  if (!view || !view->impl) {
    return;
  }

  PuglEmscriptenTimer* timer = view->impl->timers;
  while (timer) {
    PuglEmscriptenTimer* const next = timer->next;
    emscripten_clear_interval(timer->intervalHandle);
    free(timer);
    timer = next;
  }
  view->impl->timers = NULL;
}

PuglStatus
puglEmscriptenPollClipboard(PuglView* const view)
{
  PuglInternals* const impl = view ? view->impl : NULL;
  if (!impl || !impl->clipboardRequest || !impl->id) {
    return PUGL_SUCCESS;
  }

  const int length = puglBrowserClipboardLength((unsigned)impl->id);
  if (length == PUGL_EMSCRIPTEN_CLIPBOARD_PENDING) {
    return PUGL_SUCCESS;
  }

  impl->clipboardRequest = false;
  if (length == PUGL_EMSCRIPTEN_CLIPBOARD_FAILED) {
    puglBrowserClipboardCancel((unsigned)impl->id);
    return PUGL_SUCCESS;
  }

  char* const data = (char*)calloc((size_t)length + 1U, 1U);
  if (!data) {
    return PUGL_NO_MEMORY;
  }

  if (!puglBrowserClipboardRead(
        (unsigned)impl->id, data, (size_t)length + 1U)) {
    free(data);
    return PUGL_FAILURE;
  }

  const PuglStatus copyStatus =
    puglSetBlob(&impl->clipboardData, data, (size_t)length);
  free(data);
  if (copyStatus) {
    return copyStatus;
  }

  impl->clipboardOffer = true;
  PuglEvent event       = {0};
  event.offer.type      = PUGL_DATA_OFFER;
  event.offer.time      = puglGetTime(view->world);
  event.offer.x         = impl->pointerX;
  event.offer.y         = impl->pointerY;
  event.offer.clipboard = PUGL_CLIPBOARD_GENERAL;
  return puglDispatchEvent(view, &event);
}

PuglStatus
puglEmscriptenPaste(PuglView* const view)
{
  if (!view || !view->impl || !view->impl->id) {
    return PUGL_BAD_PARAMETER;
  }

  puglEmscriptenClearClipboard(view);
  if (!puglBrowserClipboardRequest((unsigned)view->impl->id)) {
    return PUGL_UNSUPPORTED;
  }

  view->impl->clipboardRequest = true;
  return PUGL_SUCCESS;
}

PuglStatus
puglEmscriptenAcceptOffer(PuglView* const                 view,
                          const PuglDataOfferEvent* const offer,
                          const uint32_t                  typeIndex,
                          const PuglDataAction            action,
                          const int                       regionX,
                          const int                       regionY,
                          const unsigned                  regionWidth,
                          const unsigned                  regionHeight)
{
  (void)action;
  (void)regionX;
  (void)regionY;
  (void)regionWidth;
  (void)regionHeight;

  if (!view || !view->impl || !view->impl->clipboardOffer || !offer ||
      offer->clipboard != PUGL_CLIPBOARD_GENERAL || typeIndex != 0U) {
    return PUGL_BAD_PARAMETER;
  }

  view->impl->clipboardOffer = false;
  PuglEvent event       = {0};
  event.data.type       = PUGL_DATA;
  event.data.time       = offer->time;
  event.data.x          = offer->x;
  event.data.y          = offer->y;
  event.data.clipboard  = PUGL_CLIPBOARD_GENERAL;
  event.data.typeIndex  = typeIndex;
  return puglDispatchEvent(view, &event);
}

void
puglEmscriptenClearClipboard(PuglView* const view)
{
  if (!view || !view->impl) {
    return;
  }

  if (view->impl->clipboardRequest && view->impl->id) {
    puglBrowserClipboardCancel((unsigned)view->impl->id);
  }
  view->impl->clipboardRequest = false;
  view->impl->clipboardOffer   = false;
  free(view->impl->clipboardData.data);
  view->impl->clipboardData.data = NULL;
  view->impl->clipboardData.len  = 0U;
}

PuglStatus
puglEmscriptenSetClipboard(PuglView* const     view,
                           const PuglClipboard clipboard,
                           const char* const   type,
                           const void* const   data,
                           const size_t        len)
{
  if (!view || !view->impl || clipboard != PUGL_CLIPBOARD_GENERAL || !type ||
      !data || strcmp(type, "text/plain") &&
                 strcmp(type, "text/plain;charset=utf-8")) {
    return PUGL_BAD_PARAMETER;
  }

  return puglBrowserClipboardWrite((const char*)data, len) ? PUGL_SUCCESS
                                                            : PUGL_UNSUPPORTED;
}
