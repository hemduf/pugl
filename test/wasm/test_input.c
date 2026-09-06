// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <emscripten.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(test)                   \
  do {                                \
    if (!(test)) {                    \
      return fail(#test, __LINE__);   \
    }                                 \
  } while (0)

typedef struct {
  unsigned          focusIn;
  unsigned          focusOut;
  unsigned          keyPress;
  unsigned          keyRelease;
  unsigned          text;
  unsigned          pointerIn;
  unsigned          pointerOut;
  unsigned          motion;
  unsigned          buttonPress;
  unsigned          buttonRelease;
  unsigned          scroll;
  unsigned          configure;
  PuglKeyEvent       lastKey;
  PuglTextEvent      lastText;
  PuglMotionEvent    lastMotion;
  PuglButtonEvent    lastButton;
  PuglScrollEvent    lastScroll;
  PuglConfigureEvent lastConfigure;
} TestState;

static void
setBrowserResult(const bool pass)
{
  emscripten_run_script(
    pass ? "document.body.dataset.puglTest='pass';"
           "document.body.dataset.puglReady='true';"
         : "document.body.dataset.puglTest='fail';"
           "document.body.dataset.puglReady='true';");
}

static int
fail(const char* const expression, const int line)
{
  fprintf(stderr, "Input check failed at line %d: %s\n", line, expression);
  setBrowserResult(false);
  return 1;
}

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
  TestState* const state = (TestState*)puglGetHandle(view);
  if (!state) {
    return PUGL_FAILURE;
  }

  switch (event->type) {
  case PUGL_FOCUS_IN:
    ++state->focusIn;
    break;
  case PUGL_FOCUS_OUT:
    ++state->focusOut;
    break;
  case PUGL_KEY_PRESS:
    ++state->keyPress;
    state->lastKey = event->key;
    break;
  case PUGL_KEY_RELEASE:
    ++state->keyRelease;
    state->lastKey = event->key;
    break;
  case PUGL_TEXT:
    ++state->text;
    state->lastText = event->text;
    break;
  case PUGL_POINTER_IN:
    ++state->pointerIn;
    break;
  case PUGL_POINTER_OUT:
    ++state->pointerOut;
    break;
  case PUGL_MOTION:
    ++state->motion;
    state->lastMotion = event->motion;
    break;
  case PUGL_BUTTON_PRESS:
    ++state->buttonPress;
    state->lastButton = event->button;
    break;
  case PUGL_BUTTON_RELEASE:
    ++state->buttonRelease;
    state->lastButton = event->button;
    break;
  case PUGL_SCROLL:
    ++state->scroll;
    state->lastScroll = event->scroll;
    break;
  case PUGL_CONFIGURE:
    ++state->configure;
    state->lastConfigure = event->configure;
    break;
  default:
    break;
  }

  return PUGL_SUCCESS;
}

static void
dispatchInput(const PuglNativeView nativeView)
{
  char script[4096] = {0};
  snprintf(
    script,
    sizeof(script),
    "(()=>{const e=document.getElementById('pugl-view-%" PRIuPTR "');"
    "if(!e)throw new Error('missing Pugl canvas');"
    "e.focus();"
    "e.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowLeft',code:'ArrowLeft',"
    "keyCode:37,which:37,shiftKey:true,ctrlKey:true,bubbles:true}));"
    "e.dispatchEvent(new KeyboardEvent('keyup',{key:'ArrowLeft',code:'ArrowLeft',"
    "keyCode:37,which:37,shiftKey:true,ctrlKey:true,bubbles:true}));"
    "e.dispatchEvent(new InputEvent('beforeinput',{data:'é',inputType:'insertText',"
    "bubbles:true,cancelable:true}));"
    "const r=e.getBoundingClientRect();"
    "const p=(type,button=0)=>new PointerEvent(type,{pointerId:7,pointerType:'pen',"
    "clientX:r.left+25,clientY:r.top+30,screenX:125,screenY:130,button,buttons:1,"
    "shiftKey:true,bubbles:true});"
    "e.dispatchEvent(p('pointerenter'));"
    "e.dispatchEvent(p('pointermove'));"
    "e.dispatchEvent(p('pointerdown',0));"
    "e.dispatchEvent(p('pointerup',0));"
    "e.dispatchEvent(p('pointerleave'));"
    "e.dispatchEvent(new WheelEvent('wheel',{clientX:r.left+25,clientY:r.top+30,"
    "screenX:125,screenY:130,deltaX:1,deltaY:2,deltaMode:1,altKey:true,bubbles:true}));"
    "e.style.width='360px';e.style.height='210px';"
    "window.dispatchEvent(new Event('resize'));"
    "e.blur();})()",
    (uintptr_t)nativeView);
  emscripten_run_script(script);
}

static void
saveDetachedCanvas(const PuglNativeView nativeView)
{
  char script[256] = {0};
  snprintf(script,
           sizeof(script),
           "window.puglDetached=document.getElementById('pugl-view-%" PRIuPTR "');",
           (uintptr_t)nativeView);
  emscripten_run_script(script);
}

static void
dispatchDetachedInput(void)
{
  emscripten_run_script(
    "(()=>{const e=window.puglDetached;if(!e)return;"
    "e.dispatchEvent(new KeyboardEvent('keydown',{key:'a',code:'KeyA',bubbles:true}));"
    "e.dispatchEvent(new PointerEvent('pointermove',{clientX:1,clientY:1,bubbles:true}));"
    "})()");
}

int
main(void)
{
  TestState state = {0};
  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  CHECK(world);

  PuglView* const view = puglNewView(world);
  CHECK(view);
  puglSetHandle(view, &state);
  puglSetBackend(view, puglStubBackend());
  puglSetEventFunc(view, onEvent);
  CHECK(puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 320U, 180U) == PUGL_SUCCESS);
  CHECK(puglShow(view, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS);

  const PuglNativeView nativeView = puglGetNativeView(view);
  CHECK(nativeView != 0U);

  const unsigned initialConfigure = state.configure;
  dispatchInput(nativeView);

  CHECK(state.focusIn >= 1U);
  CHECK(state.focusOut >= 1U);
  CHECK(state.keyPress == 1U);
  CHECK(state.keyRelease == 1U);
  CHECK(state.lastKey.key == PUGL_KEY_LEFT);
  CHECK((state.lastKey.state & PUGL_MOD_SHIFT) != 0U);
  CHECK((state.lastKey.state & PUGL_MOD_CTRL) != 0U);
  CHECK(state.text == 1U);
  CHECK(state.lastText.character == 0x00E9U);
  CHECK(strcmp(state.lastText.string, "é") == 0);
  CHECK(state.pointerIn >= 1U);
  CHECK(state.pointerOut >= 1U);
  CHECK(state.motion >= 1U);
  CHECK(state.buttonPress == 1U);
  CHECK(state.buttonRelease == 1U);
  CHECK(state.lastMotion.x >= 24.0 && state.lastMotion.x <= 26.0);
  CHECK(state.lastMotion.y >= 29.0 && state.lastMotion.y <= 31.0);
  CHECK((state.lastMotion.state & PUGL_MOD_SHIFT) != 0U);
  CHECK(state.lastButton.button == 0U);
  CHECK(state.scroll == 1U);
  CHECK(state.lastScroll.direction == PUGL_SCROLL_SMOOTH);
  CHECK(state.lastScroll.dx == -1.0);
  CHECK(state.lastScroll.dy == -2.0);
  CHECK((state.lastScroll.state & PUGL_MOD_ALT) != 0U);
  CHECK(state.configure > initialConfigure);
  CHECK(state.lastConfigure.width == 360U);
  CHECK(state.lastConfigure.height == 210U);

  CHECK(puglGrabFocus(view) == PUGL_SUCCESS);
  CHECK(puglHasFocus(view));
  emscripten_run_script("document.activeElement.blur()");
  CHECK(!puglHasFocus(view));

  saveDetachedCanvas(nativeView);
  const unsigned keysBeforeTeardown = state.keyPress;
  const unsigned motionBeforeTeardown = state.motion;
  CHECK(puglUnrealize(view) == PUGL_SUCCESS);
  dispatchDetachedInput();
  CHECK(state.keyPress == keysBeforeTeardown);
  CHECK(state.motion == motionBeforeTeardown);

  puglFreeView(view);
  puglFreeWorld(world);
  setBrowserResult(true);
  return 0;
}
