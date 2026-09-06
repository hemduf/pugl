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
  unsigned          compositionText;
  unsigned          pointerIn;
  unsigned          pointerOut;
  unsigned          motion;
  unsigned          buttonPress;
  unsigned          buttonRelease;
  unsigned          scroll;
  unsigned          configure;
  uint32_t          unicodeKey;
  PuglKeyEvent       lastKey;
  PuglTextEvent      lastText;
  PuglMotionEvent    lastMotion;
  PuglButtonEvent    lastButton;
  PuglScrollEvent    lastScroll;
  PuglConfigureEvent lastConfigure;
} TestState;

static TestState      state;
static PuglWorld*     world;
static PuglView*      view;
static PuglNativeView nativeView;

static void
setBrowserResult(const bool pass)
{
  emscripten_run_script(
    pass ? "document.body.dataset.puglTest='pass';"
           "document.body.dataset.puglReady='true';"
         : "document.body.dataset.puglTest='fail';"
           "document.body.dataset.puglReady='true';");
}

static void
setBrowserReady(void)
{
  emscripten_run_script(
    "document.body.dataset.puglTest='pending';"
    "document.body.dataset.puglReady='true';");
}

static int
fail(const char* const expression, const int line)
{
  fprintf(stderr,
          "Input check failed at line %d: %s "
          "[text=%u composition=%u focusIn=%u focusOut=%u "
          "keyPress=%u keyRelease=%u configure=%u]\n",
          line,
          expression,
          state.text,
          state.compositionText,
          state.focusIn,
          state.focusOut,
          state.keyPress,
          state.keyRelease,
          state.configure);
  setBrowserResult(false);
  return 1;
}

static PuglStatus
onEvent(PuglView* const eventView, const PuglEvent* const event)
{
  TestState* const testState = (TestState*)puglGetHandle(eventView);
  if (!testState) {
    return PUGL_FAILURE;
  }

  switch (event->type) {
  case PUGL_FOCUS_IN:
    ++testState->focusIn;
    break;
  case PUGL_FOCUS_OUT:
    ++testState->focusOut;
    break;
  case PUGL_KEY_PRESS:
    ++testState->keyPress;
    testState->lastKey = event->key;
    if (event->key.keycode == 233U) {
      testState->unicodeKey = event->key.key;
    }
    break;
  case PUGL_KEY_RELEASE:
    ++testState->keyRelease;
    testState->lastKey = event->key;
    break;
  case PUGL_TEXT:
    ++testState->text;
    testState->lastText = event->text;
    if (event->text.character == 0x6F22U) {
      ++testState->compositionText;
    }
    break;
  case PUGL_POINTER_IN:
    ++testState->pointerIn;
    break;
  case PUGL_POINTER_OUT:
    ++testState->pointerOut;
    break;
  case PUGL_MOTION:
    ++testState->motion;
    testState->lastMotion = event->motion;
    break;
  case PUGL_BUTTON_PRESS:
    ++testState->buttonPress;
    testState->lastButton = event->button;
    break;
  case PUGL_BUTTON_RELEASE:
    ++testState->buttonRelease;
    testState->lastButton = event->button;
    break;
  case PUGL_SCROLL:
    ++testState->scroll;
    testState->lastScroll = event->scroll;
    break;
  case PUGL_CONFIGURE:
    ++testState->configure;
    testState->lastConfigure = event->configure;
    break;
  default:
    break;
  }

  return PUGL_SUCCESS;
}

static void
dispatchInput(const PuglNativeView currentNativeView)
{
  char script[6144] = {0};
  snprintf(
    script,
    sizeof(script),
    "(()=>{const e=document.getElementById('pugl-view-%" PRIuPTR "');"
    "if(!e)throw new Error('missing Pugl canvas');"
    "const input=document.getElementById(e.id+'-input');"
    "if(!input)throw new Error('missing Pugl text input');"
    "const inputEvent=(type,data,inputType,isComposing)=>{"
    "const ev=new InputEvent(type,{data,bubbles:true,cancelable:type==='beforeinput'});"
    "Object.defineProperty(ev,'inputType',{value:inputType});"
    "Object.defineProperty(ev,'isComposing',{value:isComposing});"
    "return ev;};"
    "e.focus();"
    "if(document.activeElement!==input)throw new Error('canvas focus did not redirect');"
    "input.dispatchEvent(new KeyboardEvent('keydown',{key:'é',code:'KeyE',"
    "keyCode:233,which:233,bubbles:true}));"
    "input.dispatchEvent(new KeyboardEvent('keyup',{key:'é',code:'KeyE',"
    "keyCode:233,which:233,bubbles:true}));"
    "input.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowLeft',code:'ArrowLeft',"
    "keyCode:37,which:37,shiftKey:true,ctrlKey:true,bubbles:true}));"
    "input.dispatchEvent(new KeyboardEvent('keyup',{key:'ArrowLeft',code:'ArrowLeft',"
    "keyCode:37,which:37,shiftKey:true,ctrlKey:true,bubbles:true}));"
    "input.dispatchEvent(new CompositionEvent('compositionstart',{data:'',bubbles:true}));"
    "input.dispatchEvent(inputEvent('beforeinput','漢','insertCompositionText',true));"
    "input.value='漢';"
    "input.dispatchEvent(inputEvent('input','漢','insertCompositionText',true));"
    "input.dispatchEvent(new CompositionEvent('compositionend',{data:'漢',bubbles:true}));"
    "input.dispatchEvent(inputEvent('beforeinput','漢','insertFromComposition',false));"
    "input.value='漢';"
    "input.dispatchEvent(inputEvent('input','漢','insertFromComposition',false));"
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
    "window.dispatchEvent(new Event('resize'));})()",
    (uintptr_t)currentNativeView);
  emscripten_run_script(script);
}

static void
saveDetachedTargets(const PuglNativeView currentNativeView)
{
  char script[384] = {0};
  snprintf(
    script,
    sizeof(script),
    "(()=>{const e=document.getElementById('pugl-view-%" PRIuPTR "');"
    "window.puglDetachedCanvas=e;"
    "window.puglDetachedInput=e?document.getElementById(e.id+'-input'):null;})()",
    (uintptr_t)currentNativeView);
  emscripten_run_script(script);
}

static void
dispatchDetachedInput(void)
{
  emscripten_run_script(
    "(()=>{const e=window.puglDetachedCanvas;"
    "const input=window.puglDetachedInput;"
    "if(input){"
    "input.dispatchEvent(new KeyboardEvent('keydown',{key:'a',code:'KeyA',bubbles:true}));"
    "input.dispatchEvent(new InputEvent('beforeinput',{data:'x',inputType:'insertText',"
    "bubbles:true,cancelable:true}));}"
    "if(e)e.dispatchEvent(new PointerEvent('pointermove',{clientX:1,clientY:1,bubbles:true}));"
    "})()");
}

static bool
browserInputExists(const PuglNativeView currentNativeView)
{
  char script[256] = {0};
  snprintf(script,
           sizeof(script),
           "!!document.getElementById('pugl-view-%" PRIuPTR "-input')",
           (uintptr_t)currentNativeView);
  return emscripten_run_script_int(script) != 0;
}

EMSCRIPTEN_KEEPALIVE int
puglWasmInputFinish(void)
{
  CHECK(world);
  CHECK(view);
  CHECK(nativeView != 0U);
  CHECK(state.text == 2U);
  CHECK(state.compositionText == 1U);
  CHECK(state.lastText.character == 0x00C9U);
  CHECK(strcmp(state.lastText.string, "É") == 0);
  CHECK((state.lastText.state & PUGL_MOD_SHIFT) != 0U);
  CHECK(puglHasFocus(view));

  emscripten_run_script("document.activeElement.blur()");
  CHECK(!puglHasFocus(view));
  CHECK(state.focusOut >= 1U);

  CHECK(puglGrabFocus(view) == PUGL_SUCCESS);
  CHECK(puglHasFocus(view));
  CHECK(state.focusIn >= 2U);

  saveDetachedTargets(nativeView);
  const unsigned keysBeforeTeardown   = state.keyPress;
  const unsigned textBeforeTeardown   = state.text;
  const unsigned motionBeforeTeardown = state.motion;
  CHECK(puglUnrealize(view) == PUGL_SUCCESS);
  CHECK(!browserInputExists(nativeView));
  dispatchDetachedInput();
  CHECK(state.keyPress == keysBeforeTeardown);
  CHECK(state.text == textBeforeTeardown);
  CHECK(state.motion == motionBeforeTeardown);

  puglFreeView(view);
  puglFreeWorld(world);
  view       = NULL;
  world      = NULL;
  nativeView = 0U;
  setBrowserResult(true);
  return 0;
}

int
main(void)
{
  state = (TestState){0};
  world = puglNewWorld(PUGL_PROGRAM, 0U);
  CHECK(world);

  view = puglNewView(world);
  CHECK(view);
  puglSetHandle(view, &state);
  puglSetBackend(view, puglStubBackend());
  puglSetEventFunc(view, onEvent);
  CHECK(puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 320U, 180U) == PUGL_SUCCESS);
  CHECK(puglShow(view, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS);

  nativeView = puglGetNativeView(view);
  CHECK(nativeView != 0U);
  CHECK(browserInputExists(nativeView));

  const unsigned initialConfigure = state.configure;
  dispatchInput(nativeView);

  CHECK(state.focusIn >= 1U);
  CHECK(state.keyPress == 2U);
  CHECK(state.keyRelease == 2U);
  CHECK(state.unicodeKey == 0x00E9U);
  CHECK(state.lastKey.key == PUGL_KEY_LEFT);
  CHECK((state.lastKey.state & PUGL_MOD_SHIFT) != 0U);
  CHECK((state.lastKey.state & PUGL_MOD_CTRL) != 0U);
  CHECK(state.text == 1U);
  CHECK(state.compositionText == 1U);
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
  CHECK(puglHasFocus(view));

  setBrowserReady();
  return 0;
}
