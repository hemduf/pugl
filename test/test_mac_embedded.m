// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

// Tests native MacOS embedded view visibility and focus lifecycle

#undef NDEBUG

#include <pugl/pugl.h>
#include <pugl/stub.h>

#import <Cocoa/Cocoa.h>
#import <objc/message.h>
#import <objc/runtime.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static void
check(const bool condition, const char* const expression, const unsigned line)
{
  if (!condition) {
    fprintf(stderr, "test_mac_embedded.m:%u: %s\n", line, expression);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(condition) check((condition), #condition, __LINE__)

@interface PuglTestWindow : NSWindow {
@public
  BOOL puglTestKeyWindow;
}
@end

@implementation PuglTestWindow
- (BOOL)isKeyWindow
{
  return puglTestKeyWindow;
}
@end

@interface PuglTestResponderView : NSView
@end

@implementation PuglTestResponderView
- (BOOL)acceptsFirstResponder
{
  return YES;
}
@end

typedef struct {
  unsigned configures;
  unsigned focusIn;
  unsigned focusOut;
  unsigned buttonPresses;
} TestState;

typedef enum {
  CALLBACK_NONE,
  CALLBACK_HIDE_ON_MAPPED_CONFIGURE,
  CALLBACK_SHOW_ON_UNMAPPED_CONFIGURE,
  CALLBACK_FREE_ON_CONFIGURE,
  CALLBACK_FREE_ON_FOCUS_IN,
  CALLBACK_FREE_ON_FOCUS_OUT,
  CALLBACK_UNREALIZE_ON_FOCUS_OUT,
  CALLBACK_SHOW_ON_FOCUS_OUT,
  CALLBACK_FREE_ON_NATIVE_SHOW,
  CALLBACK_UNREALIZE_ON_NATIVE_SHOW,
} CallbackAction;

typedef struct {
  PuglView*      view;
  CallbackAction action;
  unsigned       configures;
  unsigned       focusIn;
  unsigned       focusOut;
  unsigned       buttonPresses;
} CallbackState;

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
  TestState* const state = (TestState*)puglGetHandle(view);

  switch (event->type) {
  case PUGL_CONFIGURE:
    ++state->configures;
    break;
  case PUGL_FOCUS_IN:
    ++state->focusIn;
    break;
  case PUGL_FOCUS_OUT:
    ++state->focusOut;
    break;
  case PUGL_BUTTON_PRESS:
    ++state->buttonPresses;
    break;
  default:
    break;
  }

  return PUGL_SUCCESS;
}

static PuglStatus
onCallbackEvent(PuglView* const view, const PuglEvent* const event)
{
  CallbackState* const state = (CallbackState*)puglGetHandle(view);

  switch (event->type) {
  case PUGL_CONFIGURE:
    ++state->configures;
    if (state->action == CALLBACK_HIDE_ON_MAPPED_CONFIGURE &&
        (event->configure.style & PUGL_VIEW_STYLE_MAPPED)) {
      state->action = CALLBACK_NONE;
      assert(!puglHide(view));
    } else if (state->action == CALLBACK_SHOW_ON_UNMAPPED_CONFIGURE &&
               !(event->configure.style & PUGL_VIEW_STYLE_MAPPED)) {
      state->action = CALLBACK_NONE;
      CHECK(!puglShow(view, PUGL_SHOW_PASSIVE));
    } else if (state->action == CALLBACK_FREE_ON_CONFIGURE) {
      state->action = CALLBACK_NONE;
      state->view   = NULL;
      puglFreeView(view);
    }
    break;

  case PUGL_FOCUS_IN:
    ++state->focusIn;
    if (state->action == CALLBACK_FREE_ON_FOCUS_IN) {
      state->action = CALLBACK_NONE;
      state->view   = NULL;
      puglFreeView(view);
    }
    break;

  case PUGL_FOCUS_OUT:
    ++state->focusOut;
    if (state->action == CALLBACK_FREE_ON_FOCUS_OUT) {
      state->action = CALLBACK_NONE;
      state->view   = NULL;
      puglFreeView(view);
    } else if (state->action == CALLBACK_UNREALIZE_ON_FOCUS_OUT) {
      state->action = CALLBACK_NONE;
      CHECK(!puglUnrealize(view));
    } else if (state->action == CALLBACK_SHOW_ON_FOCUS_OUT) {
      state->action = CALLBACK_NONE;
      CHECK(!puglShow(view, PUGL_SHOW_PASSIVE));
    }
    break;

  case PUGL_BUTTON_PRESS:
    ++state->buttonPresses;
    break;

  default:
    break;
  }

  return PUGL_SUCCESS;
}

static PuglView*
newEmbeddedView(PuglWorld* const world,
                NSView* const    parent,
                void* const      handle,
                PuglEventFunc    eventFunc)
{
  PuglView* const view = puglNewView(world);
  assert(view);

  assert(!puglSetBackend(view, puglStubBackend()));
  puglSetHandle(view, handle);
  assert(!puglSetEventFunc(view, eventFunc));
  assert(!puglSetParent(view, (PuglNativeView)parent));
  assert(!puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 160U, 100U));

  return view;
}

static NSEvent*
mouseDownEvent(NSWindow* const window)
{
  return [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown
                            location:NSMakePoint(16.0, 16.0)
                       modifierFlags:0U
                           timestamp:0.0
                        windowNumber:[window windowNumber]
                             context:nil
                         eventNumber:1
                          clickCount:1
                            pressure:1.0];
}

static const char visibilityStateKey = 0;
static Class      visibilityClass;

static void
nativeSetHidden(id const self, SEL const selector, const BOOL hidden)
{
  struct objc_super superclass = {self, class_getSuperclass(visibilityClass)};
  ((void (*)(struct objc_super*, SEL, BOOL))objc_msgSendSuper)(
    &superclass, selector, hidden);

  CallbackState* const state = (CallbackState*)[(
    NSValue*)objc_getAssociatedObject(self, &visibilityStateKey) pointerValue];
  if (!hidden && state && state->view) {
    const CallbackAction action = state->action;
    if (action == CALLBACK_FREE_ON_NATIVE_SHOW) {
      state->action        = CALLBACK_NONE;
      PuglView* const view = state->view;
      state->view          = NULL;
      puglFreeView(view);
    } else if (action == CALLBACK_UNREALIZE_ON_NATIVE_SHOW) {
      state->action = CALLBACK_NONE;
      CHECK(!puglUnrealize(state->view));
    }
  }
}

static void
testNativeShowRetirement(PuglWorld* const     world,
                         NSView* const        parent,
                         const CallbackAction action)
{
  CallbackState state = {NULL, action, 0U, 0U, 0U, 0U};
  state.view          = newEmbeddedView(world, parent, &state, onCallbackEvent);
  CHECK(!puglRealize(state.view));

  NSView* const native        = [(NSView*)puglGetNativeView(state.view) retain];
  Class const   originalClass = object_getClass(native);
  if (!visibilityClass) {
    visibilityClass =
      objc_allocateClassPair(originalClass, "PuglTestVisibilityWrapper", 0U);
    CHECK(visibilityClass);
    CHECK(class_addMethod(visibilityClass,
                          @selector(setHidden:),
                          (IMP)nativeSetHidden,
                          method_getTypeEncoding(class_getInstanceMethod(
                            originalClass, @selector(setHidden:)))));
    objc_registerClassPair(visibilityClass);
  }

  objc_setAssociatedObject(native,
                           &visibilityStateKey,
                           [NSValue valueWithPointer:&state],
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  object_setClass(native, visibilityClass);
  CHECK(!puglShow(state.view, PUGL_SHOW_PASSIVE));
  CHECK(state.configures == 0U);
  CHECK(action == CALLBACK_FREE_ON_NATIVE_SHOW ? state.view == NULL
                                               : state.view != NULL);
  if (state.view) {
    CHECK(!puglGetNativeView(state.view));
    puglFreeView(state.view);
  }

  // Remove the stack-state association before releasing this borrowed target.
  object_setClass(native, originalClass);
  objc_setAssociatedObject(
    native, &visibilityStateKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  [native release];
}

int
main(void)
{
  NSAutoreleasePool* const pool = [NSAutoreleasePool new];

  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  assert(world);

  NSRect const frame = NSMakeRect(0.0, 0.0, 320.0, 200.0);
  PuglTestWindow* const host =
    [[PuglTestWindow alloc] initWithContentRect:frame
                                     styleMask:NSWindowStyleMaskTitled
                                       backing:NSBackingStoreBuffered
                                         defer:NO];
  NSView* const parent = [[NSView alloc] initWithFrame:frame];
  NSView* const sentinel =
    [[PuglTestResponderView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1)];
  [parent addSubview:sentinel];
  [host setContentView:parent];
  assert([host makeFirstResponder:sentinel]);

  PuglView* const view = puglNewView(world);
  assert(view);

  TestState state = {0U, 0U, 0U, 0U};

  puglSetWorldString(world, PUGL_CLASS_NAME, "PuglMacEmbeddedTest");
  puglSetBackend(view, puglStubBackend());
  puglSetHandle(view, &state);
  puglSetEventFunc(view, onEvent);
  puglSetParent(view, (PuglNativeView)parent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 160U, 100U);

  // Hiding an unrealized embedded view is a harmless no-op.
  assert(!puglHide(view));
  assert(!puglGetVisible(view));

  assert(!puglRealize(view));

  NSView* const nativeView = (NSView*)puglGetNativeView(view);
  assert(nativeView);
  assert([nativeView isHidden]);
  assert(!puglGetVisible(view));
  assert(!puglHasFocus(view));
  assert([host firstResponder] == sentinel);
  assert(![host isVisible]);

  // Showing an embedded child maps only the child and never raises its host.
  assert(!puglShow(view, PUGL_SHOW_PASSIVE));
  assert(![nativeView isHidden]);
  assert(puglGetVisible(view));
  assert(state.configures == 1U);
  assert(!puglHasFocus(view));
  assert(puglGrabFocus(view) == PUGL_FAILURE);
  assert(![host isVisible]);
  assert([host firstResponder] == sentinel);

  // Repeating show is idempotent and does not synthesize configuration.
  assert(!puglShow(view, PUGL_SHOW_PASSIVE));
  assert(state.configures == 1U);

  // Model host activation explicitly so this is deterministic on headless CI.
  [host setIsVisible:YES];
  host->puglTestKeyWindow = YES;
  [[NSNotificationCenter defaultCenter]
    postNotificationName:NSWindowDidBecomeKeyNotification
                  object:host];
  assert([host isVisible]);
  assert([host isKeyWindow]);
  assert([host firstResponder] == sentinel);
  assert(!puglHasFocus(view));

  // A user click transfers keyboard focus to the embedded view before input.
  [(id)nativeView mouseDown:mouseDownEvent(host)];
  assert([host firstResponder] == nativeView);
  assert(puglHasFocus(view));
  assert(state.focusIn == 1U);
  assert(state.focusOut == 0U);
  assert(state.buttonPresses == 1U);

  // Hiding only the child releases its focus and leaves the host mapped.
  assert(!puglHide(view));
  assert([nativeView isHidden]);
  assert(!puglGetVisible(view));
  assert(!puglHasFocus(view));
  assert([host isVisible]);
  assert([host firstResponder] != nativeView);
  assert(state.configures == 2U);
  assert(state.focusIn == 1U);
  assert(state.focusOut == 1U);

  // Repeating hide is idempotent and does not synthesize configuration.
  assert(!puglHide(view));
  assert(state.configures == 2U);
  assert(state.focusOut == 1U);

  // Passive show does not steal focus back.
  assert(!puglShow(view, PUGL_SHOW_PASSIVE));
  assert(![nativeView isHidden]);
  assert(puglGetVisible(view));
  assert(state.configures == 3U);
  assert(!puglHasFocus(view));
  assert([host firstResponder] != nativeView);
  assert(state.focusIn == 1U);

  [(id)nativeView mouseDown:mouseDownEvent(host)];
  assert(state.focusIn == 2U);
  assert([host firstResponder] == nativeView);
  assert(puglHasFocus(view));

  // Borrowed-window key transitions are reflected as Pugl focus transitions.
  host->puglTestKeyWindow = NO;
  [[NSNotificationCenter defaultCenter]
    postNotificationName:NSWindowDidResignKeyNotification
                  object:host];
  assert(state.focusOut == 2U);
  assert(!puglHasFocus(view));

  host->puglTestKeyWindow = YES;
  [[NSNotificationCenter defaultCenter]
    postNotificationName:NSWindowDidBecomeKeyNotification
                  object:host];
  assert(state.focusIn == 3U);
  assert(puglHasFocus(view));

  // Destroying a focused child must not call user code or destroy its host.
  puglFreeView(view);
  assert(state.focusOut == 2U);
  assert([host isVisible]);
  assert([host contentView] == parent);

  // Two simultaneous children must keep focus and lifecycle state isolated.
  TestState firstState  = {0U, 0U, 0U, 0U};
  TestState secondState = {0U, 0U, 0U, 0U};
  PuglView* const firstView =
    newEmbeddedView(world, parent, &firstState, onEvent);
  PuglView* const secondView =
    newEmbeddedView(world, parent, &secondState, onEvent);

  assert(!puglRealize(firstView));
  assert(!puglRealize(secondView));
  assert(!puglShow(firstView, PUGL_SHOW_PASSIVE));
  assert(!puglShow(secondView, PUGL_SHOW_PASSIVE));

  NSView* const firstNative  = (NSView*)puglGetNativeView(firstView);
  NSView* const secondNative = (NSView*)puglGetNativeView(secondView);
  assert(firstNative);
  assert(secondNative);

  [(id)firstNative mouseDown:mouseDownEvent(host)];
  assert(puglHasFocus(firstView));
  assert(!puglHasFocus(secondView));
  assert(firstState.focusIn == 1U);
  assert(secondState.focusIn == 0U);

  [(id)secondNative mouseDown:mouseDownEvent(host)];
  assert(!puglHasFocus(firstView));
  assert(puglHasFocus(secondView));
  assert(firstState.focusOut == 1U);
  assert(secondState.focusIn == 1U);

  // Destroying A must not alter B's focus or observer state.
  puglFreeView(firstView);
  assert(puglHasFocus(secondView));
  assert(secondState.focusOut == 0U);
  assert([host contentView] == parent);

  assert(!puglHide(secondView));
  assert(secondState.focusOut == 1U);
  assert(!puglHasFocus(secondView));
  assert(!puglShow(secondView, PUGL_SHOW_PASSIVE));
  [(id)secondNative mouseDown:mouseDownEvent(host)];
  assert(puglHasFocus(secondView));
  assert(secondState.focusIn == 2U);

  puglFreeView(secondView);
  assert(secondState.focusOut == 1U);
  assert([host contentView] == parent);

  // A configure callback may synchronously hide again; the nested transition
  // must win instead of being overwritten by the outer show.
  CallbackState reentrant = {
    NULL, CALLBACK_HIDE_ON_MAPPED_CONFIGURE, 0U, 0U, 0U, 0U};
  PuglView* const reentrantView =
    newEmbeddedView(world, parent, &reentrant, onCallbackEvent);
  reentrant.view = reentrantView;
  assert(!puglRealize(reentrantView));

  NSView* const reentrantNative = (NSView*)puglGetNativeView(reentrantView);
  assert(reentrantNative);
  assert(!puglShow(reentrantView, PUGL_SHOW_PASSIVE));
  assert(reentrant.view == reentrantView);
  assert(reentrant.configures == 2U);
  assert([reentrantNative isHidden]);
  assert(!puglGetVisible(reentrantView));
  puglFreeView(reentrantView);

  // A configure callback may destroy the view.  The child dispatcher commits
  // state before the callback and does not dereference the view afterwards.
  CallbackState freeOnConfigure = {
    NULL, CALLBACK_FREE_ON_CONFIGURE, 0U, 0U, 0U, 0U};
  PuglView* configureView =
    newEmbeddedView(world, parent, &freeOnConfigure, onCallbackEvent);
  freeOnConfigure.view = configureView;
  assert(!puglRealize(configureView));
  assert(!puglShow(configureView, PUGL_SHOW_PASSIVE));
  assert(freeOnConfigure.view == NULL);
  assert(freeOnConfigure.configures == 1U);
  assert([host contentView] == parent);

  // A focus callback may destroy the view before mouseDown can dispatch the
  // button event.  Native wrapper lifetime must remain valid while unwinding.
  CallbackState freeOnFocusIn = {
    NULL, CALLBACK_FREE_ON_FOCUS_IN, 0U, 0U, 0U, 0U};
  PuglView* focusInView =
    newEmbeddedView(world, parent, &freeOnFocusIn, onCallbackEvent);
  freeOnFocusIn.view = focusInView;
  assert(!puglRealize(focusInView));
  assert(!puglShow(focusInView, PUGL_SHOW_PASSIVE));

  NSView* const focusInNative = (NSView*)puglGetNativeView(focusInView);
  assert(focusInNative);
  [(id)focusInNative mouseDown:mouseDownEvent(host)];
  assert(freeOnFocusIn.view == NULL);
  assert(freeOnFocusIn.focusIn == 1U);
  assert(freeOnFocusIn.buttonPresses == 0U);
  assert([host contentView] == parent);

  // Hiding a focused child may destroy it from FOCUS_OUT.  Nothing after that
  // callback may dereference the Pugl view or released native wrapper.
  CallbackState freeOnFocusOut = {
    NULL, CALLBACK_NONE, 0U, 0U, 0U, 0U};
  PuglView* focusOutView =
    newEmbeddedView(world, parent, &freeOnFocusOut, onCallbackEvent);
  freeOnFocusOut.view = focusOutView;
  assert(!puglRealize(focusOutView));
  assert(!puglShow(focusOutView, PUGL_SHOW_PASSIVE));

  NSView* const focusOutNative = (NSView*)puglGetNativeView(focusOutView);
  assert(focusOutNative);
  [(id)focusOutNative mouseDown:mouseDownEvent(host)];
  assert(freeOnFocusOut.focusIn == 1U);
  assert(freeOnFocusOut.buttonPresses == 1U);

  freeOnFocusOut.action = CALLBACK_FREE_ON_FOCUS_OUT;
  assert(!puglHide(focusOutView));
  assert(freeOnFocusOut.view == NULL);
  assert(freeOnFocusOut.focusOut == 1U);
  assert([host contentView] == parent);

  // Native focus loss can retire the wrapper without freeing the C view.
  CallbackState unrealizeOnFocusOut = {NULL, CALLBACK_NONE, 0U, 0U, 0U, 0U};
  unrealizeOnFocusOut.view =
    newEmbeddedView(world, parent, &unrealizeOnFocusOut, onCallbackEvent);
  CHECK(!puglRealize(unrealizeOnFocusOut.view));
  CHECK(!puglShow(unrealizeOnFocusOut.view, PUGL_SHOW_PASSIVE));
  NSView* const unrealizeNative =
    (NSView*)puglGetNativeView(unrealizeOnFocusOut.view);
  [(id)unrealizeNative mouseDown:mouseDownEvent(host)];
  unrealizeOnFocusOut.action = CALLBACK_UNREALIZE_ON_FOCUS_OUT;
  CHECK(!puglHide(unrealizeOnFocusOut.view));
  CHECK(unrealizeOnFocusOut.focusOut == 1U);
  CHECK(!puglGetNativeView(unrealizeOnFocusOut.view));
  puglFreeView(unrealizeOnFocusOut.view);

  // A nested re-show from native focus loss wins over the outer hide.
  CallbackState showOnFocusOut = {NULL, CALLBACK_NONE, 0U, 0U, 0U, 0U};
  showOnFocusOut.view =
    newEmbeddedView(world, parent, &showOnFocusOut, onCallbackEvent);
  CHECK(!puglRealize(showOnFocusOut.view));
  CHECK(!puglShow(showOnFocusOut.view, PUGL_SHOW_PASSIVE));
  NSView* const showNative = (NSView*)puglGetNativeView(showOnFocusOut.view);
  [(id)showNative mouseDown:mouseDownEvent(host)];
  showOnFocusOut.action = CALLBACK_SHOW_ON_FOCUS_OUT;
  CHECK(!puglHide(showOnFocusOut.view));
  CHECK(showOnFocusOut.focusOut == 1U);
  CHECK(showOnFocusOut.configures == 2U);
  CHECK(![showNative isHidden]);
  CHECK(puglGetVisible(showOnFocusOut.view));
  puglFreeView(showOnFocusOut.view);

  // A nested re-show from the unmapped configuration also wins.
  CallbackState showOnConfigure = {NULL, CALLBACK_NONE, 0U, 0U, 0U, 0U};
  showOnConfigure.view =
    newEmbeddedView(world, parent, &showOnConfigure, onCallbackEvent);
  CHECK(!puglRealize(showOnConfigure.view));
  CHECK(!puglShow(showOnConfigure.view, PUGL_SHOW_PASSIVE));
  showOnConfigure.action = CALLBACK_SHOW_ON_UNMAPPED_CONFIGURE;
  CHECK(!puglHide(showOnConfigure.view));
  CHECK(showOnConfigure.configures == 3U);
  CHECK(puglGetVisible(showOnConfigure.view));
  puglFreeView(showOnConfigure.view);

  // A consumer's native visibility callback can retire the view while show
  // is unwinding, before it can access the drawing view or configure state.
  testNativeShowRetirement(world, parent, CALLBACK_FREE_ON_NATIVE_SHOW);
  testNativeShowRetirement(world, parent, CALLBACK_UNREALIZE_ON_NATIVE_SHOW);

  puglFreeWorld(world);

  [host orderOut:nil];
  [sentinel release];
  [parent release];
  [host release];
  [pool drain];
  return 0;
}
