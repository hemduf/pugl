// Copyright 2026 Fabrizio Duhem
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <emscripten.h>
#include <emscripten/eventloop.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  PuglWorld* world;
  PuglView*  view;
  unsigned   pointerIn;
  unsigned   pointerOut;
  unsigned   motion;
  unsigned   buttons;
  unsigned   scroll;
  unsigned   focusIn;
  unsigned   focusOut;
  unsigned   keyPress;
  unsigned   keyRelease;
  unsigned   text;
  unsigned   timers;
  unsigned   offers;
  unsigned   dataEvents;
  bool       clipboardMatches;
  bool       failed;
} TestContext;

EM_JS(int, puglInstallTestClipboard, (), {
  if (typeof navigator === 'undefined' || !navigator.clipboard) {
    return 0;
  }

  const clipboard = navigator.clipboard;
  globalThis.__puglTestClipboardValue = String();

  const writeText = (text) => {
    globalThis.__puglTestClipboardValue = String(text);
    return Promise.resolve();
  };
  const readText = () => Promise.resolve(globalThis.__puglTestClipboardValue);

  try {
    clipboard.writeText = writeText;
    clipboard.readText = readText;
  } catch (_) {
    return 0;
  }

  return clipboard.writeText === writeText && clipboard.readText === readText;
});

EM_JS(void, puglMarkTestFailure, (const char* reason), {
  document.body.dataset.puglTest = 'fail';
  document.body.dataset.puglReason = UTF8ToString(reason);
});

EM_JS(void, puglDispatchTestInput, (unsigned id), {
  const canvas = document.getElementById(`pugl-canvas-${id}`);
  if (!canvas) {
    document.body.dataset.puglTest = 'fail';
    document.body.dataset.puglReason = 'missing-canvas';
    return;
  }

  const mouse = {
    bubbles: true,
    clientX: 12,
    clientY: 18,
    screenX: 112,
    screenY: 218
  };

  canvas.dispatchEvent(new MouseEvent('mouseenter', mouse));
  canvas.dispatchEvent(new MouseEvent('mousemove', mouse));
  canvas.dispatchEvent(new MouseEvent('mousedown', {...mouse, button: 2}));
  canvas.dispatchEvent(new MouseEvent('mouseup', {...mouse, button: 2}));
  canvas.dispatchEvent(new WheelEvent('wheel', {
    ...mouse,
    deltaMode: WheelEvent.DOM_DELTA_PIXEL,
    deltaX: 0,
    deltaY: -40
  }));

  canvas.focus({preventScroll: true});
  canvas.dispatchEvent(new KeyboardEvent('keydown', {
    bubbles: true,
    key: 'a',
    code: 'KeyA'
  }));
  canvas.dispatchEvent(new KeyboardEvent('keypress', {
    bubbles: true,
    key: 'a',
    code: 'KeyA',
    charCode: 97
  }));
  canvas.dispatchEvent(new KeyboardEvent('keyup', {
    bubbles: true,
    key: 'a',
    code: 'KeyA'
  }));
  canvas.blur();
  canvas.dispatchEvent(new MouseEvent('mouseleave', mouse));
});

static void
fail(TestContext* const context, const char* const what)
{
  if (!context->failed) {
    fprintf(stderr, "WASM input test failed: %s\n", what);
    puglMarkTestFailure(what);
  }
  context->failed = true;
}

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
  TestContext* const context = (TestContext*)puglGetHandle(view);

  switch (event->type) {
  case PUGL_POINTER_IN:
    ++context->pointerIn;
    break;
  case PUGL_POINTER_OUT:
    ++context->pointerOut;
    break;
  case PUGL_MOTION:
    ++context->motion;
    break;
  case PUGL_BUTTON_PRESS:
  case PUGL_BUTTON_RELEASE:
    ++context->buttons;
    if (event->button.button != 1U) {
      fail(context, "button-map");
    }
    break;
  case PUGL_SCROLL:
    ++context->scroll;
    if (event->scroll.direction != PUGL_SCROLL_SMOOTH ||
        event->scroll.dy <= 0.0) {
      fail(context, "wheel-map");
    }
    break;
  case PUGL_FOCUS_IN:
    ++context->focusIn;
    break;
  case PUGL_FOCUS_OUT:
    ++context->focusOut;
    break;
  case PUGL_KEY_PRESS:
    ++context->keyPress;
    if (event->key.key != (uint32_t)'a') {
      fail(context, "key-map");
    }
    break;
  case PUGL_KEY_RELEASE:
    ++context->keyRelease;
    break;
  case PUGL_TEXT:
    ++context->text;
    if (event->text.character != (uint32_t)'a' ||
        strcmp(event->text.string, "a")) {
      fail(context, "text-map");
    }
    break;
  case PUGL_TIMER:
    if (event->timer.id == 77U) {
      ++context->timers;
    }
    break;
  case PUGL_DATA_OFFER:
    ++context->offers;
    if (puglGetNumClipboardTypes(view, PUGL_CLIPBOARD_GENERAL) != 1U ||
        !puglGetClipboardType(view, PUGL_CLIPBOARD_GENERAL, 0U) ||
        puglAcceptOffer(view,
                        &event->offer,
                        0U,
                        PUGL_DATA_ACTION_COPY,
                        0,
                        0,
                        0U,
                        0U)) {
      fail(context, "clipboard-accept");
    }
    break;
  case PUGL_DATA: {
    ++context->dataEvents;
    size_t len       = 0U;
    const char* data = (const char*)puglGetClipboard(
      view, PUGL_CLIPBOARD_GENERAL, event->data.typeIndex, &len);
    context->clipboardMatches = data && len == 5U && !memcmp(data, "hello", 5U);
    if (!context->clipboardMatches) {
      fail(context, "clipboard-data");
    }
    break;
  }
  default:
    break;
  }

  return PUGL_SUCCESS;
}

static void
finish(void* const data)
{
  TestContext* const context = (TestContext*)data;

  if (puglUpdate(context->world, 0.0)) {
    fail(context, "clipboard-poll");
  }

  if (context->pointerIn < 1U || context->pointerOut < 1U ||
      context->motion < 1U || context->buttons != 2U || context->scroll < 1U) {
    fail(context, "pointer-coverage");
  }
  if (context->focusIn < 1U || context->focusOut < 1U) {
    fail(context, "focus-coverage");
  }
  if (context->keyPress < 1U || context->keyRelease < 1U || context->text < 1U) {
    fail(context, "keyboard-coverage");
  }
  if (context->timers < 1U) {
    fail(context, "timer");
  }
  if (context->offers != 1U || context->dataEvents != 1U ||
      !context->clipboardMatches) {
    fail(context, "clipboard-events");
  }

  if (puglStopTimer(context->view, 77U)) {
    fail(context, "timer-stop");
  }

  if (!context->failed) {
    EM_ASM({ document.body.dataset.puglTest = 'pass'; });
  }

  puglFreeView(context->view);
  puglFreeWorld(context->world);
  free(context);
  emscripten_runtime_keepalive_pop();
}

int
main(void)
{
  TestContext* const context = (TestContext*)calloc(1U, sizeof(TestContext));
  if (!context) {
    puglMarkTestFailure("context-allocation");
    return 1;
  }

  context->world = puglNewWorld(PUGL_PROGRAM, 0U);
  context->view  = context->world ? puglNewView(context->world) : NULL;
  if (!context->world || !context->view) {
    puglMarkTestFailure("pugl-allocation");
    return 1;
  }

  puglSetHandle(context->view, context);
  if (puglSetBackend(context->view, puglStubBackend()) ||
      puglSetEventFunc(context->view, onEvent) ||
      puglSetSizeHint(context->view, PUGL_DEFAULT_SIZE, 320U, 180U) ||
      puglShow(context->view, PUGL_SHOW_PASSIVE)) {
    fail(context, "view-setup");
    return 1;
  }

  if (!puglInstallTestClipboard()) {
    fail(context, "clipboard-shim");
    return 1;
  }

  if (puglSetClipboard(context->view,
                       PUGL_CLIPBOARD_GENERAL,
                       "text/plain;charset=utf-8",
                       "hello",
                       5U) ||
      puglPaste(context->view)) {
    fail(context, "clipboard-request");
    return 1;
  }

  puglDispatchTestInput((unsigned)puglGetNativeView(context->view));

  if (puglStartTimer(context->view, 77U, 0.005)) {
    fail(context, "timer-setup");
    return 1;
  }

  emscripten_runtime_keepalive_push();
  emscripten_set_timeout(finish, 80.0, context);
  return 0;
}
