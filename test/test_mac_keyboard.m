// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

// Tests Cocoa keyboard callbacks and temporary embedded responder handoffs.

#undef NDEBUG

#include <pugl/pugl.h>
#include <pugl/stub.h>

#import <Cocoa/Cocoa.h>
#import <objc/message.h>
#import <objc/runtime.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static void
check(const bool condition, const char* const expression, const unsigned line)
{
  if (!condition) {
    fprintf(stderr, "test_mac_keyboard.m:%u: %s\n", line, expression);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(condition) check((condition), #condition, __LINE__)

@interface NSView (PuglKeyboardTestFocusHook)
- (BOOL)puglPreserveEmbeddedFocus;
@end

@interface PuglKeyboardTestApplication : NSApplication {
@public
  NSEvent* puglTestCurrentEvent;
}
@end

@implementation PuglKeyboardTestApplication
- (NSEvent*)currentEvent
{
  return puglTestCurrentEvent;
}
@end

@interface PuglKeyboardTestWindow : NSWindow {
@public
  BOOL puglTestKeyWindow;
}
@end

@implementation PuglKeyboardTestWindow
- (BOOL)isKeyWindow
{
  return puglTestKeyWindow;
}
@end

@interface PuglKeyboardTestParent : NSView
@end

@implementation PuglKeyboardTestParent
- (BOOL)acceptsFirstResponder
{
  return YES;
}
@end

typedef struct {
  PuglView*          view;
  PuglEventType      retireOn;
  bool               unrealizeOnly;
  unsigned           presses;
  unsigned           releases;
  unsigned           text;
  unsigned           focusIn;
  unsigned           focusOut;
  unsigned           interpretations;
  unsigned           deallocations;
  bool               preserveFocus;
  NSWindow*          window;
  NSView*            parent;
  NSAutoreleasePool* callbackPool;
  PuglTextEvent      lastText;
} TestState;

static const char stateKey = 0;
static Class      wrapperClass;

static TestState*
wrapperState(id const wrapper)
{
  return (TestState*)[(NSValue*)objc_getAssociatedObject(wrapper, &stateKey)
    pointerValue];
}

static BOOL
preserveEmbeddedFocus(id const self, SEL const selector)
{
  (void)selector;
  TestState* const   state     = wrapperState(self);
  NSWindow* const    window    = [(NSView*)self window];
  NSResponder* const responder = [window firstResponder];
  return state->preserveFocus && window == state->window &&
         [(NSView*)self superview] == state->parent && [window isKeyWindow] &&
         ![(NSView*)self isHiddenOrHasHiddenAncestor] &&
         (responder == self || responder == state->parent);
}

static void
interpretKeyEvents(id const self, SEL const selector, NSArray* const events)
{
  (void)selector;
  (void)events;
  ++wrapperState(self)->interpretations;
}

static void
deallocateWrapper(id const self, SEL const selector)
{
  ++wrapperState(self)->deallocations;
  struct objc_super superclass = {self, class_getSuperclass(wrapperClass)};
  ((void (*)(struct objc_super*, SEL))objc_msgSendSuper)(&superclass, selector);
}

static void
observeWrapper(NSView* const wrapper, TestState* const state)
{
  if (!wrapperClass) {
    Class const baseClass = object_getClass(wrapper);
    wrapperClass =
      objc_allocateClassPair(baseClass, "PuglKeyboardTestWrapper", 0U);
    CHECK(wrapperClass);
    CHECK(class_addMethod(wrapperClass,
                          @selector(puglPreserveEmbeddedFocus),
                          (IMP)preserveEmbeddedFocus,
                          method_getTypeEncoding(class_getInstanceMethod(
                            baseClass, @selector(puglPreserveEmbeddedFocus)))));
    CHECK(class_addMethod(wrapperClass,
                          @selector(interpretKeyEvents:),
                          (IMP)interpretKeyEvents,
                          "v@:@"));
    CHECK(class_addMethod(
      wrapperClass, @selector(dealloc), (IMP)deallocateWrapper, "v@:"));
    objc_registerClassPair(wrapperClass);
  }

  objc_setAssociatedObject(wrapper,
                           &stateKey,
                           [NSValue valueWithPointer:state],
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  object_setClass(wrapper, wrapperClass);
}

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
  TestState* const state = (TestState*)puglGetHandle(view);
  switch (event->type) {
  case PUGL_KEY_PRESS:
    ++state->presses;
    break;
  case PUGL_KEY_RELEASE:
    ++state->releases;
    break;
  case PUGL_TEXT:
    ++state->text;
    state->lastText = event->text;
    break;
  case PUGL_FOCUS_IN:
    ++state->focusIn;
    break;
  case PUGL_FOCUS_OUT:
    ++state->focusOut;
    break;
  default:
    break;
  }

  if (state->retireOn == event->type) {
    state->retireOn = PUGL_NOTHING;
    if (state->unrealizeOnly) {
      CHECK(!puglUnrealize(view));
    } else {
      state->view = NULL;
      puglFreeView(view);
    }

    // Release AppKit's temporary references while application code is still
    // on the callback stack.  The native method must keep its target alive.
    NSAutoreleasePool* const pool = state->callbackPool;
    state->callbackPool           = nil;
    [pool drain];
    CHECK(state->deallocations == 0U);
  }

  return PUGL_SUCCESS;
}

static NSView*
newEmbeddedView(PuglWorld* const world,
                NSView* const    parent,
                TestState* const state)
{
  state->view   = puglNewView(world);
  state->parent = parent;
  state->window = [parent window];
  CHECK(state->view);
  CHECK(!puglSetBackend(state->view, puglStubBackend()));
  puglSetHandle(state->view, state);
  CHECK(!puglSetEventFunc(state->view, onEvent));
  CHECK(!puglSetParent(state->view, (PuglNativeView)parent));
  CHECK(!puglSetSizeHint(state->view, PUGL_DEFAULT_SIZE, 160U, 100U));
  CHECK(!puglRealize(state->view));
  CHECK(!puglShow(state->view, PUGL_SHOW_PASSIVE));

  NSView* const native = (NSView*)puglGetNativeView(state->view);
  CHECK(native);
  CHECK(![native puglPreserveEmbeddedFocus]);
  observeWrapper(native, state);
  return native;
}

static NSEvent*
keyEvent(NSWindow* const window, const NSEventType type)
{
  return
    [NSEvent keyEventWithType:type
                         location:NSZeroPoint
                    modifierFlags:(type == NSEventTypeFlagsChanged
                                     ? NSEventModifierFlagShift
                                     : 0U)
                        timestamp:1.0
                     windowNumber:[window windowNumber]
                          context:nil
                       characters:@"a"
      charactersIgnoringModifiers:@"a"
                        isARepeat:NO
                          keyCode:(type == NSEventTypeFlagsChanged ? 56U : 0U)];
}

static void
testRetirement(PuglWorld* const  world,
               NSView* const     parent,
               const NSEventType type,
               const bool        unrealizeOnly)
{
  TestState state = {0};
  state.retireOn = type == NSEventTypeKeyUp ? PUGL_KEY_RELEASE : PUGL_KEY_PRESS;
  state.unrealizeOnly = unrealizeOnly;
  state.callbackPool  = [NSAutoreleasePool new];

  NSView* const  native = newEmbeddedView(world, parent, &state);
  NSEvent* const event  = [keyEvent([parent window], type) retain];
  if (type == NSEventTypeKeyDown) {
    [native keyDown:event];
  } else if (type == NSEventTypeKeyUp) {
    [native keyUp:event];
  } else {
    [native flagsChanged:event];
  }

  [event release];
  CHECK(!state.callbackPool);

  CHECK(state.presses + state.releases == 1U);
  CHECK(state.interpretations == 0U);
  CHECK(state.deallocations == 1U);
  CHECK(unrealizeOnly ? state.view != NULL : state.view == NULL);
  if (state.view) {
    puglFreeView(state.view);
  }
}

static void
testTextRetirement(PuglWorld* const world,
                   NSView* const    parent,
                   const bool       unrealizeOnly)
{
  TestState state      = {0};
  state.retireOn       = PUGL_TEXT;
  state.unrealizeOnly  = unrealizeOnly;
  state.callbackPool   = [NSAutoreleasePool new];
  NSView* const native = newEmbeddedView(world, parent, &state);
  [(id<NSTextInputClient>)native insertText:@"ab"
                           replacementRange:NSMakeRange(NSNotFound, 0)];

  CHECK(!state.callbackPool);
  CHECK(state.text == 1U);
  CHECK(state.deallocations == 1U);
  CHECK(unrealizeOnly ? state.view != NULL : state.view == NULL);
  if (state.view) {
    puglFreeView(state.view);
  }
}

int
main(void)
{
  NSAutoreleasePool* const           pool = [NSAutoreleasePool new];
  PuglKeyboardTestApplication* const app  = (PuglKeyboardTestApplication*)
    [PuglKeyboardTestApplication sharedApplication];
  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  CHECK(world);

  const NSRect    frame = NSMakeRect(0.0, 0.0, 320.0, 200.0);
  NSWindow* const window =
    [[PuglKeyboardTestWindow alloc] initWithContentRect:frame
                                              styleMask:NSWindowStyleMaskTitled
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
  ((PuglKeyboardTestWindow*)window)->puglTestKeyWindow = YES;
  NSView* const parent = [[PuglKeyboardTestParent alloc] initWithFrame:frame];
  [window setContentView:parent];
  [window setIsVisible:YES];

  // Text insertion need not originate from a native keyboard event.
  TestState     textState  = {0};
  NSView* const textNative = newEmbeddedView(world, parent, &textState);
  app->puglTestCurrentEvent =
    [NSEvent otherEventWithType:NSEventTypeAppKitDefined
                       location:NSZeroPoint
                  modifierFlags:0U
                      timestamp:2.0
                   windowNumber:[window windowNumber]
                        context:nil
                        subtype:0
                          data1:0
                          data2:0];
  [(id<NSTextInputClient>)textNative insertText:@"a"
                               replacementRange:NSMakeRange(NSNotFound, 0)];
  CHECK(textState.text == 1U);
  CHECK(textState.lastText.character == 'a');
  CHECK(textState.lastText.keycode == 0U);
  CHECK(textState.lastText.state == 0U);

  app->puglTestCurrentEvent = nil;
  [(id<NSTextInputClient>)textNative insertText:@"b"
                               replacementRange:NSMakeRange(NSNotFound, 0)];
  CHECK(textState.text == 2U);
  CHECK(textState.lastText.character == 'b');
  puglFreeView(textState.view);

  // Every keyboard callback can synchronously free or unrealize its own view.
  const NSEventType eventTypes[] = {
    NSEventTypeKeyDown, NSEventTypeKeyUp, NSEventTypeFlagsChanged};
  for (size_t i = 0U; i < sizeof(eventTypes) / sizeof(eventTypes[0]); ++i) {
    testRetirement(world, parent, eventTypes[i], false);
    testRetirement(world, parent, eventTypes[i], true);
  }
  testTextRetirement(world, parent, false);
  testTextRetirement(world, parent, true);

  // Multi-character insertion stops when the first text callback retires the
  // view.  A retained, retired wrapper safely ignores subsequent native input.
  for (unsigned unrealize = 0U; unrealize < 2U; ++unrealize) {
    TestState state      = {0};
    state.retireOn       = PUGL_TEXT;
    state.unrealizeOnly  = unrealize != 0U;
    state.callbackPool   = [NSAutoreleasePool new];
    NSView* const native = [newEmbeddedView(world, parent, &state) retain];
    [(id<NSTextInputClient>)native insertText:@"ab"
                             replacementRange:NSMakeRange(NSNotFound, 0)];
    CHECK(!state.callbackPool);
    CHECK(state.text == 1U);
    [native keyDown:keyEvent(window, NSEventTypeKeyDown)];
    [native keyUp:keyEvent(window, NSEventTypeKeyUp)];
    [native flagsChanged:keyEvent(window, NSEventTypeFlagsChanged)];
    [(id<NSTextInputClient>)native insertText:@"c"
                             replacementRange:NSMakeRange(NSNotFound, 0)];
    CHECK(state.text == 1U);
    CHECK(state.presses == 0U);
    CHECK(state.releases == 0U);
    [native release];
    CHECK(state.deallocations == 1U);
    if (state.view) {
      puglFreeView(state.view);
    }
  }

  // An opt-in subclass can preserve focus while briefly dispatching through
  // the parent responder, without a synthetic focus out/in pair.
  TestState     focusState  = {0};
  NSView* const focusNative = newEmbeddedView(world, parent, &focusState);
  CHECK([window makeFirstResponder:focusNative]);
  CHECK(focusState.focusIn == 1U);
  focusState.preserveFocus = true;
  CHECK([window makeFirstResponder:parent]);
  CHECK(focusState.focusOut == 0U);
  CHECK([window makeFirstResponder:focusNative]);
  focusState.preserveFocus = false;
  CHECK(focusState.focusIn == 1U);
  CHECK(focusState.focusOut == 0U);

  // A real borrowed-window focus transition is never suppressed by the hook.
  focusState.preserveFocus                             = true;
  ((PuglKeyboardTestWindow*)window)->puglTestKeyWindow = NO;
  [[NSNotificationCenter defaultCenter]
    postNotificationName:NSWindowDidResignKeyNotification
                  object:window];
  CHECK(focusState.focusOut == 1U);
  ((PuglKeyboardTestWindow*)window)->puglTestKeyWindow = YES;
  focusState.preserveFocus                             = false;
  [[NSNotificationCenter defaultCenter]
    postNotificationName:NSWindowDidBecomeKeyNotification
                  object:window];
  CHECK(focusState.focusIn == 2U);

  // Hide and detach still report genuine focus loss during a handoff.
  focusState.preserveFocus = true;
  CHECK(!puglHide(focusState.view));
  CHECK(focusState.focusOut == 2U);
  CHECK(!puglShow(focusState.view, PUGL_SHOW_PASSIVE));
  focusState.preserveFocus = false;
  CHECK([window makeFirstResponder:focusNative]);
  CHECK(focusState.focusIn == 3U);
  focusState.preserveFocus = true;
  [focusNative removeFromSuperview];
  CHECK(focusState.focusOut == 3U);
  puglFreeView(focusState.view);

  puglFreeWorld(world);
  [window orderOut:nil];
  [parent release];
  [window release];
  [pool drain];
  return 0;
}
