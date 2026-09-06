// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include "emscripten_events.h"

#include "emscripten_platform.h"
#include "internal.h"
#include "types.h"

#include <pugl/pugl.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct PuglBrowserBindingImpl PuglBrowserBinding;

struct PuglBrowserBindingImpl {
  PuglBrowserBinding* next;
  PuglView*           view;
  uintptr_t           token;
};

static PuglBrowserBinding* puglBrowserBindings = NULL;
static uintptr_t           puglNextBrowserToken = 1U;

#define PUGL_BROWSER_INPUT_SELECTOR_SIZE 64U

#if defined(__GNUC__)
#  define PUGL_BROWSER_EXPORT __attribute__((visibility("default")))
#else
#  define PUGL_BROWSER_EXPORT
#endif

EM_JS(int, puglBrowserInstallInput, (const char* selector, uintptr_t token), {
  const element = document.querySelector(UTF8ToString(selector));
  if (!element || element.__puglInput) {
    return 0;
  }

  const inputId = `${element.id}-input`;
  if (document.getElementById(inputId)) {
    return 0;
  }

  const input = document.createElement('textarea');
  input.id = inputId;
  input.dataset.puglTextInput = String(token);
  input.tabIndex = -1;
  input.autocomplete = 'off';
  input.autocapitalize = 'off';
  input.spellcheck = false;
  input.wrap = 'off';
  input.rows = 1;
  input.style.position = 'fixed';
  input.style.left = '0';
  input.style.top = '0';
  input.style.width = '1px';
  input.style.height = '1px';
  input.style.margin = '0';
  input.style.padding = '0';
  input.style.border = '0';
  input.style.opacity = '0';
  input.style.pointerEvents = 'none';
  input.style.transform = 'translate(-10000px, -10000px)';
  input.style.zIndex = '-1';

  const parent = element.parentNode || document.body;
  if (!parent) {
    return 0;
  }
  parent.insertBefore(input, element.nextSibling);

  const exported = (name) => Module[name];
  const modifiers = (event) => {
    let result = 0;
    result |= event.shiftKey ? 1 : 0;
    result |= event.ctrlKey ? 2 : 0;
    result |= event.altKey ? 4 : 0;
    result |= event.metaKey ? 8 : 0;
    if (typeof event.getModifierState === 'function') {
      result |= event.getModifierState('NumLock') ? 16 : 0;
      result |= event.getModifierState('ScrollLock') ? 32 : 0;
      result |= event.getModifierState('CapsLock') ? 64 : 0;
    }
    return result;
  };

  const coordinates = (event) => {
    const rect = element.getBoundingClientRect();
    return [event.clientX - rect.left,
            event.clientY - rect.top,
            event.clientX,
            event.clientY];
  };

  const focusInput = () => {
    try {
      input.focus({preventScroll: true});
    } catch (_) {
      input.focus();
    }
    return document.activeElement === input;
  };

  const handlers = {
    textInput: input,
    mods: 0,
    lockMods: 0,
  };

  handlers.keyMods = (event) => {
    const mask = modifiers(event);
    handlers.mods = mask & (1 | 2 | 4 | 8);
    handlers.lockMods = mask & (16 | 32 | 64);
  };

  const emitText = (data) => {
    const callback = exported('_puglEmscriptenTextEvent');
    if (typeof callback !== 'function' || !data) {
      return;
    }

    const state = handlers.mods | handlers.lockMods;
    for (const character of String(data)) {
      callback(token, character.codePointAt(0), state);
    }
  };

  handlers.text = (event) => {
    if (event.cancelable) {
      event.preventDefault();
    }

    const compositionInput =
      event.isComposing ||
      event.inputType === 'insertCompositionText' ||
      event.inputType === 'insertFromComposition';
    if (!compositionInput && event.data) {
      emitText(event.data);
    }
    input.value = '';
  };

  handlers.inputEvent = () => {
    input.value = '';
  };

  handlers.compositionEnd = (event) => {
    if (event.data) {
      emitText(event.data);
    }
    input.value = '';
  };

  const pointer = (kind) => (event) => {
    const callback = exported('_puglEmscriptenPointerEvent');
    if (typeof callback !== 'function') {
      return;
    }

    const position = coordinates(event);
    callback(token,
             kind,
             position[0],
             position[1],
             position[2],
             position[3],
             event.button < 0 ? 0 : event.button,
             modifiers(event));
  };

  handlers.pointerEnter = pointer(1);
  handlers.pointerLeave = pointer(2);
  handlers.pointerMove = pointer(3);
  handlers.pointerDown = (event) => {
    focusInput();

    if (typeof element.setPointerCapture === 'function') {
      try {
        element.setPointerCapture(event.pointerId);
      } catch (_) {}
    }

    pointer(4)(event);
  };
  handlers.pointerUp = (event) => {
    pointer(5)(event);
    if (typeof element.releasePointerCapture === 'function') {
      try {
        if (!element.hasPointerCapture || element.hasPointerCapture(event.pointerId)) {
          element.releasePointerCapture(event.pointerId);
        }
      } catch (_) {}
    }
  };
  handlers.pointerCancel = pointer(2);

  handlers.wheel = (event) => {
    const callback = exported('_puglEmscriptenScrollEvent');
    if (typeof callback !== 'function') {
      return;
    }

    const position = coordinates(event);
    const factor = event.deltaMode === 0 ? 1.0 / 40.0
                 : event.deltaMode === 2 ? 24.0
                 : 1.0;
    callback(token,
             position[0],
             position[1],
             position[2],
             position[3],
             -event.deltaX * factor,
             -event.deltaY * factor,
             modifiers(event));
    if (event.cancelable) {
      event.preventDefault();
    }
  };

  handlers.resize = () => {
    const callback = exported('_puglEmscriptenResizeEvent');
    if (typeof callback !== 'function' || !element.isConnected) {
      return;
    }

    const rect = element.getBoundingClientRect();
    if (!(rect.width > 0) || !(rect.height > 0)) {
      return;
    }

    const ratio = window.devicePixelRatio > 0 ? window.devicePixelRatio : 1;
    const width = Math.max(1, Math.round(rect.width));
    const height = Math.max(1, Math.round(rect.height));
    element.width = Math.max(1, Math.round(rect.width * ratio));
    element.height = Math.max(1, Math.round(rect.height * ratio));
    callback(token, width, height, ratio);
  };

  handlers.canvasFocus = () => {
    focusInput();
  };

  input.addEventListener('keydown', handlers.keyMods, true);
  input.addEventListener('keyup', handlers.keyMods, true);
  input.addEventListener('beforeinput', handlers.text, false);
  input.addEventListener('input', handlers.inputEvent, false);
  input.addEventListener('compositionend', handlers.compositionEnd, false);
  element.addEventListener('focus', handlers.canvasFocus, false);
  element.addEventListener('pointerenter', handlers.pointerEnter, false);
  element.addEventListener('pointerleave', handlers.pointerLeave, false);
  element.addEventListener('pointermove', handlers.pointerMove, false);
  element.addEventListener('pointerdown', handlers.pointerDown, false);
  element.addEventListener('pointerup', handlers.pointerUp, false);
  element.addEventListener('pointercancel', handlers.pointerCancel, false);
  element.addEventListener('wheel', handlers.wheel, {passive: false});
  window.addEventListener('resize', handlers.resize, false);

  if (typeof ResizeObserver === 'function') {
    handlers.observer = new ResizeObserver(handlers.resize);
    handlers.observer.observe(element);
  }

  element.__puglInput = handlers;
  return 1;
});

EM_JS(void, puglBrowserUninstallInput, (const char* selector), {
  const element = document.querySelector(UTF8ToString(selector));
  const handlers = element ? element.__puglInput : null;
  if (!handlers) {
    return;
  }

  const input = handlers.textInput;
  if (input) {
    input.removeEventListener('keydown', handlers.keyMods, true);
    input.removeEventListener('keyup', handlers.keyMods, true);
    input.removeEventListener('beforeinput', handlers.text, false);
    input.removeEventListener('input', handlers.inputEvent, false);
    input.removeEventListener('compositionend', handlers.compositionEnd, false);
  }

  element.removeEventListener('focus', handlers.canvasFocus, false);
  element.removeEventListener('pointerenter', handlers.pointerEnter, false);
  element.removeEventListener('pointerleave', handlers.pointerLeave, false);
  element.removeEventListener('pointermove', handlers.pointerMove, false);
  element.removeEventListener('pointerdown', handlers.pointerDown, false);
  element.removeEventListener('pointerup', handlers.pointerUp, false);
  element.removeEventListener('pointercancel', handlers.pointerCancel, false);
  element.removeEventListener('wheel', handlers.wheel, false);
  window.removeEventListener('resize', handlers.resize, false);
  if (handlers.observer) {
    handlers.observer.disconnect();
  }

  if (input) {
    input.remove();
  }
  delete element.__puglInput;
});

EM_JS(unsigned, puglBrowserLockModifiers, (const char* selector), {
  const element = document.querySelector(UTF8ToString(selector));
  return element && element.__puglInput ? element.__puglInput.lockMods : 0;
});

EM_JS(int, puglBrowserFocus, (const char* selector), {
  const element = document.querySelector(UTF8ToString(selector));
  const input = element && element.__puglInput
                  ? element.__puglInput.textInput
                  : null;
  if (!input) {
    return 0;
  }

  try {
    input.focus({preventScroll: true});
  } catch (_) {
    input.focus();
  }
  return document.activeElement === input;
});

EM_JS(int, puglBrowserHasFocus, (const char* selector), {
  const element = document.querySelector(UTF8ToString(selector));
  const input = element && element.__puglInput
                  ? element.__puglInput.textInput
                  : null;
  return !!input && document.activeElement === input;
});

static PuglBrowserBinding*
puglFindBinding(const uintptr_t token)
{
  for (PuglBrowserBinding* binding = puglBrowserBindings; binding;
       binding = binding->next) {
    if (binding->token == token) {
      return binding;
    }
  }

  return NULL;
}

static PuglView*
puglBindingView(PuglBrowserBinding* const binding)
{
  PuglView* const view = binding ? binding->view : NULL;
  return view && view->impl && view->impl->id ? view : NULL;
}

static PuglView*
puglTokenView(const uintptr_t token)
{
  return puglBindingView(puglFindBinding(token));
}

static PuglMods
puglBrowserMods(const unsigned mask)
{
  return ((mask & 1U) ? PUGL_MOD_SHIFT : 0U) |
         ((mask & 2U) ? PUGL_MOD_CTRL : 0U) |
         ((mask & 4U) ? PUGL_MOD_ALT : 0U) |
         ((mask & 8U) ? PUGL_MOD_SUPER : 0U) |
         ((mask & 16U) ? PUGL_MOD_NUM_LOCK : 0U) |
         ((mask & 32U) ? PUGL_MOD_SCROLL_LOCK : 0U) |
         ((mask & 64U) ? PUGL_MOD_CAPS_LOCK : 0U);
}

static bool
puglInputSelector(const PuglView* const view,
                  char selector[PUGL_BROWSER_INPUT_SELECTOR_SIZE])
{
  if (!view || !view->impl || !view->impl->canvasSelector[0]) {
    return false;
  }

  const int length = snprintf(selector,
                              PUGL_BROWSER_INPUT_SELECTOR_SIZE,
                              "%s-input",
                              view->impl->canvasSelector);
  return length > 0 && (size_t)length < PUGL_BROWSER_INPUT_SELECTOR_SIZE;
}

static uint32_t
puglSpecialKey(const EmscriptenKeyboardEvent* const event)
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
puglSingleUtf8CodePoint(const char* const string)
{
  const size_t length = strlen(string);
  if (!length || length > 4U) {
    return PUGL_KEY_NONE;
  }

  const unsigned char* const s = (const unsigned char*)string;
  if (length == 1U) {
    return s[0] < 0x80U ? (uint32_t)s[0] : PUGL_KEY_NONE;
  }

  uint32_t character = 0U;
  if (length == 2U && (s[0] & 0xE0U) == 0xC0U) {
    character = (uint32_t)(s[0] & 0x1FU);
  } else if (length == 3U && (s[0] & 0xF0U) == 0xE0U) {
    character = (uint32_t)(s[0] & 0x0FU);
  } else if (length == 4U && (s[0] & 0xF8U) == 0xF0U) {
    character = (uint32_t)(s[0] & 0x07U);
  } else {
    return PUGL_KEY_NONE;
  }

  for (size_t i = 1U; i < length; ++i) {
    if ((s[i] & 0xC0U) != 0x80U) {
      return PUGL_KEY_NONE;
    }
    character = (character << 6U) | (uint32_t)(s[i] & 0x3FU);
  }

  if ((length == 2U && character < 0x80U) ||
      (length == 3U && character < 0x800U) ||
      (length == 4U && character < 0x10000U) ||
      character > 0x10FFFFU ||
      (character >= 0xD800U && character <= 0xDFFFU)) {
    return PUGL_KEY_NONE;
  }

  return character;
}

static uint32_t
puglPrintableKey(const EmscriptenKeyboardEvent* const event)
{
  const uint32_t natural = puglSingleUtf8CodePoint(event->key);
  if (natural && !event->shiftKey && !event->ctrlKey && !event->altKey &&
      !event->metaKey) {
    if (natural < 0x80U) {
      const unsigned char c = (unsigned char)natural;
      return (uint32_t)(isupper(c) ? tolower(c) : c);
    }
    return natural;
  }

  const char* const code = event->code;
  if (!strncmp(code, "Key", 3U) && code[3] >= 'A' && code[3] <= 'Z' &&
      code[4] == '\0') {
    return (uint32_t)tolower((unsigned char)code[3]);
  }

  if (!strncmp(code, "Digit", 5U) && code[5] >= '0' && code[5] <= '9' &&
      code[6] == '\0') {
    return (uint32_t)code[5];
  }

  static const struct {
    const char* code;
    uint32_t    key;
  } punctuation[] = {
    {"Backquote", '`'}, {"Minus", '-'},       {"Equal", '='},
    {"BracketLeft", '['}, {"BracketRight", ']'}, {"Backslash", '\\'},
    {"Semicolon", ';'}, {"Quote", '\''},       {"Comma", ','},
    {"Period", '.'},    {"Slash", '/'},
  };

  for (size_t i = 0U; i < sizeof(punctuation) / sizeof(punctuation[0]); ++i) {
    if (!strcmp(code, punctuation[i].code)) {
      return punctuation[i].key;
    }
  }

  if (natural < 0x80U) {
    const unsigned char c = (unsigned char)natural;
    return (uint32_t)(isupper(c) ? tolower(c) : c);
  }

  return natural;
}

static bool
puglKeyCallback(const int                            eventType,
                const EmscriptenKeyboardEvent* const browserEvent,
                void* const                          userData)
{
  PuglBrowserBinding* const binding = (PuglBrowserBinding*)userData;
  PuglView* const           view    = puglBindingView(binding);
  if (!view) {
    return false;
  }

  if (eventType == EMSCRIPTEN_EVENT_KEYDOWN && browserEvent->repeat &&
      view->hints[PUGL_IGNORE_KEY_REPEAT] == PUGL_TRUE) {
    return true;
  }

  const uint32_t special = puglSpecialKey(browserEvent);
  const PuglKey  key =
    (PuglKey)(special ? special : puglPrintableKey(browserEvent));
  const unsigned browserMods =
    (browserEvent->shiftKey ? 1U : 0U) |
    (browserEvent->ctrlKey ? 2U : 0U) |
    (browserEvent->altKey ? 4U : 0U) |
    (browserEvent->metaKey ? 8U : 0U) |
    puglBrowserLockModifiers(view->impl->canvasSelector);

  PuglEvent event   = {0};
  event.key.type    = eventType == EMSCRIPTEN_EVENT_KEYDOWN ? PUGL_KEY_PRESS
                                                             : PUGL_KEY_RELEASE;
  event.key.time    = puglGetTime(view->world);
  event.key.x       = view->impl->pointerX;
  event.key.y       = view->impl->pointerY;
  event.key.xRoot   = view->impl->pointerRootX;
  event.key.yRoot   = view->impl->pointerRootY;
  event.key.state   = puglFilterMods(puglBrowserMods(browserMods), key);
  event.key.keycode = browserEvent->which ? browserEvent->which
                                           : browserEvent->keyCode;
  event.key.key     = (uint32_t)key;
  return puglDispatchEvent(view, &event) == PUGL_SUCCESS;
}

static bool
puglFocusCallback(const int                         eventType,
                  const EmscriptenFocusEvent* const browserEvent,
                  void* const                       userData)
{
  (void)browserEvent;

  PuglView* const view = puglBindingView((PuglBrowserBinding*)userData);
  if (!view) {
    return false;
  }

  PuglEvent event  = {0};
  event.focus.type = eventType == EMSCRIPTEN_EVENT_FOCUS ? PUGL_FOCUS_IN
                                                          : PUGL_FOCUS_OUT;
  event.focus.mode = PUGL_CROSSING_NORMAL;
  return puglDispatchEvent(view, &event) == PUGL_SUCCESS;
}

static uint32_t
puglButton(const unsigned browserButton)
{
  switch (browserButton) {
  case 0U:
    return 0U;
  case 1U:
    return 2U;
  case 2U:
    return 1U;
  default:
    return browserButton;
  }
}

static void
puglRememberPointer(PuglView* const view,
                    const double    x,
                    const double    y,
                    const double    xRoot,
                    const double    yRoot)
{
  view->impl->pointerX     = x;
  view->impl->pointerY     = y;
  view->impl->pointerRootX = xRoot;
  view->impl->pointerRootY = yRoot;
}

EMSCRIPTEN_KEEPALIVE PUGL_BROWSER_EXPORT void
puglEmscriptenPointerEvent(const uintptr_t token,
                           const int       kind,
                           const double    x,
                           const double    y,
                           const double    xRoot,
                           const double    yRoot,
                           const unsigned  button,
                           const unsigned  browserMods)
{
  PuglView* const view = puglTokenView(token);
  if (!view) {
    return;
  }

  puglRememberPointer(view, x, y, xRoot, yRoot);
  const PuglMods mods = puglBrowserMods(browserMods);
  PuglEvent      event = {0};

  switch (kind) {
  case 1:
  case 2:
    event.crossing.type = kind == 1 ? PUGL_POINTER_IN : PUGL_POINTER_OUT;
    event.crossing.time = puglGetTime(view->world);
    event.crossing.x = x;
    event.crossing.y = y;
    event.crossing.xRoot = xRoot;
    event.crossing.yRoot = yRoot;
    event.crossing.state = mods;
    event.crossing.mode = PUGL_CROSSING_NORMAL;
    break;
  case 3:
    event.motion.type = PUGL_MOTION;
    event.motion.time = puglGetTime(view->world);
    event.motion.x = x;
    event.motion.y = y;
    event.motion.xRoot = xRoot;
    event.motion.yRoot = yRoot;
    event.motion.state = mods;
    break;
  case 4:
  case 5:
    event.button.type = kind == 4 ? PUGL_BUTTON_PRESS : PUGL_BUTTON_RELEASE;
    event.button.time = puglGetTime(view->world);
    event.button.x = x;
    event.button.y = y;
    event.button.xRoot = xRoot;
    event.button.yRoot = yRoot;
    event.button.state = mods;
    event.button.button = puglButton(button);
    break;
  default:
    return;
  }

  (void)puglDispatchEvent(view, &event);
}

EMSCRIPTEN_KEEPALIVE PUGL_BROWSER_EXPORT void
puglEmscriptenScrollEvent(const uintptr_t token,
                          const double    x,
                          const double    y,
                          const double    xRoot,
                          const double    yRoot,
                          const double    dx,
                          const double    dy,
                          const unsigned  browserMods)
{
  PuglView* const view = puglTokenView(token);
  if (!view) {
    return;
  }

  puglRememberPointer(view, x, y, xRoot, yRoot);
  PuglEvent event        = {0};
  event.scroll.type      = PUGL_SCROLL;
  event.scroll.time      = puglGetTime(view->world);
  event.scroll.x         = x;
  event.scroll.y         = y;
  event.scroll.xRoot     = xRoot;
  event.scroll.yRoot     = yRoot;
  event.scroll.state     = puglBrowserMods(browserMods);
  event.scroll.direction = PUGL_SCROLL_SMOOTH;
  event.scroll.dx        = dx;
  event.scroll.dy        = dy;
  (void)puglDispatchEvent(view, &event);
}

static size_t
puglEncodeUtf8(uint32_t character, char string[8])
{
  if (character > 0x10FFFFU ||
      (character >= 0xD800U && character <= 0xDFFFU)) {
    character = 0xFFFDU;
  }

  if (character <= 0x7FU) {
    string[0] = (char)character;
    return 1U;
  }
  if (character <= 0x7FFU) {
    string[0] = (char)(0xC0U | (character >> 6U));
    string[1] = (char)(0x80U | (character & 0x3FU));
    return 2U;
  }
  if (character <= 0xFFFFU) {
    string[0] = (char)(0xE0U | (character >> 12U));
    string[1] = (char)(0x80U | ((character >> 6U) & 0x3FU));
    string[2] = (char)(0x80U | (character & 0x3FU));
    return 3U;
  }

  string[0] = (char)(0xF0U | (character >> 18U));
  string[1] = (char)(0x80U | ((character >> 12U) & 0x3FU));
  string[2] = (char)(0x80U | ((character >> 6U) & 0x3FU));
  string[3] = (char)(0x80U | (character & 0x3FU));
  return 4U;
}

EMSCRIPTEN_KEEPALIVE PUGL_BROWSER_EXPORT void
puglEmscriptenTextEvent(const uintptr_t token,
                        const uint32_t  character,
                        const unsigned  browserMods)
{
  PuglView* const view = puglTokenView(token);
  if (!view) {
    return;
  }

  PuglEvent event      = {0};
  event.text.type      = PUGL_TEXT;
  event.text.time      = puglGetTime(view->world);
  event.text.x         = view->impl->pointerX;
  event.text.y         = view->impl->pointerY;
  event.text.xRoot     = view->impl->pointerRootX;
  event.text.yRoot     = view->impl->pointerRootY;
  event.text.state     = puglBrowserMods(browserMods);
  event.text.keycode   = 0U;
  event.text.character = character;
  const size_t length  = puglEncodeUtf8(character, event.text.string);
  event.text.string[length] = '\0';
  (void)puglDispatchEvent(view, &event);
}

EMSCRIPTEN_KEEPALIVE PUGL_BROWSER_EXPORT void
puglEmscriptenResizeEvent(const uintptr_t token,
                          const unsigned  width,
                          const unsigned  height,
                          const double    scaleFactor)
{
  PuglView* const view = puglTokenView(token);
  if (!view || !puglIsValidSize(width, height)) {
    return;
  }

  view->world->impl->scaleFactor = scaleFactor > 0.0 ? scaleFactor : 1.0;
  if (!view->impl->mapped || view->lastConfigure.type != PUGL_CONFIGURE) {
    return;
  }

  if (view->lastConfigure.width == width &&
      view->lastConfigure.height == height) {
    (void)puglObscureView(view);
    return;
  }

  PuglEvent event         = {0};
  event.configure         = view->lastConfigure;
  event.configure.width   = (PuglSpan)width;
  event.configure.height  = (PuglSpan)height;
  const PuglStatus status = puglDispatchEvent(view, &event);

  PuglView* const liveView = puglTokenView(token);
  if (!status && liveView == view) {
    (void)puglObscureView(view);
  }
}

static PuglStatus
puglRegistrationStatus(const EMSCRIPTEN_RESULT result)
{
  return result == EMSCRIPTEN_RESULT_SUCCESS ? PUGL_SUCCESS
                                             : PUGL_REGISTRATION_FAILED;
}

static void
puglRemoveBinding(PuglBrowserBinding* const binding)
{
  PuglBrowserBinding** link = &puglBrowserBindings;
  while (*link && *link != binding) {
    link = &(*link)->next;
  }

  if (*link) {
    *link = binding->next;
  }

  binding->next  = NULL;
  binding->view  = NULL;
  binding->token = 0U;
}

PuglStatus
puglEmscriptenRegisterInput(PuglView* const view)
{
  if (!view || !view->impl || !view->impl->id) {
    return PUGL_BAD_PARAMETER;
  }

  PuglBrowserBinding* binding =
    (PuglBrowserBinding*)view->impl->eventBinding;
  if (!binding) {
    binding = (PuglBrowserBinding*)calloc(1U, sizeof(PuglBrowserBinding));
    if (!binding) {
      return PUGL_NO_MEMORY;
    }
    view->impl->eventBinding = binding;
  }

  if (binding->view) {
    return PUGL_FAILURE;
  }

  char inputSelector[PUGL_BROWSER_INPUT_SELECTOR_SIZE] = {0};
  if (!puglInputSelector(view, inputSelector)) {
    return PUGL_REGISTRATION_FAILED;
  }

  uintptr_t token = puglNextBrowserToken++;
  if (!token) {
    token = puglNextBrowserToken++;
  }

  binding->view  = view;
  binding->token = token;
  binding->next  = puglBrowserBindings;
  puglBrowserBindings = binding;

  if (!puglBrowserInstallInput(view->impl->canvasSelector, token)) {
    puglRemoveBinding(binding);
    return PUGL_REGISTRATION_FAILED;
  }

  PuglStatus status = PUGL_SUCCESS;
#define REGISTER(call)                                                        \
  do {                                                                        \
    const PuglStatus current = puglRegistrationStatus(call);                  \
    if (!status && current) {                                                 \
      status = current;                                                       \
    }                                                                         \
  } while (0)

  REGISTER(emscripten_set_keydown_callback(
    inputSelector, binding, false, puglKeyCallback));
  REGISTER(emscripten_set_keyup_callback(
    inputSelector, binding, false, puglKeyCallback));
  REGISTER(emscripten_set_focus_callback(
    inputSelector, binding, false, puglFocusCallback));
  REGISTER(emscripten_set_blur_callback(
    inputSelector, binding, false, puglFocusCallback));

#undef REGISTER

  if (status) {
    puglEmscriptenUnregisterInput(view);
  }
  return status;
}

void
puglEmscriptenUnregisterInput(PuglView* const view)
{
  if (!view || !view->impl) {
    return;
  }

  PuglBrowserBinding* const binding =
    (PuglBrowserBinding*)view->impl->eventBinding;
  if (!binding || !binding->view) {
    return;
  }

  binding->view = NULL;

  char inputSelector[PUGL_BROWSER_INPUT_SELECTOR_SIZE] = {0};
  if (puglInputSelector(view, inputSelector)) {
    (void)emscripten_set_keydown_callback(inputSelector, NULL, false, NULL);
    (void)emscripten_set_keyup_callback(inputSelector, NULL, false, NULL);
    (void)emscripten_set_focus_callback(inputSelector, NULL, false, NULL);
    (void)emscripten_set_blur_callback(inputSelector, NULL, false, NULL);
  }

  if (view->impl->canvasSelector[0]) {
    puglBrowserUninstallInput(view->impl->canvasSelector);
  }

  puglRemoveBinding(binding);
}

void
puglEmscriptenFreeInput(PuglView* const view)
{
  if (!view || !view->impl) {
    return;
  }

  puglEmscriptenUnregisterInput(view);
  free(view->impl->eventBinding);
  view->impl->eventBinding = NULL;
}

PuglStatus
puglEmscriptenGrabFocus(PuglView* const view)
{
  if (!view || !view->impl || !view->impl->id) {
    return PUGL_BAD_PARAMETER;
  }

  return puglBrowserFocus(view->impl->canvasSelector) ? PUGL_SUCCESS
                                                       : PUGL_FAILURE;
}

bool
puglEmscriptenHasFocus(const PuglView* const view)
{
  return view && view->impl && view->impl->id &&
         puglBrowserHasFocus(view->impl->canvasSelector);
}
