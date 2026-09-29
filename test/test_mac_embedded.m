// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

// Tests native MacOS embedded view visibility and focus lifecycle

#undef NDEBUG

#include <pugl/pugl.h>
#include <pugl/stub.h>

#import <Cocoa/Cocoa.h>

#include <assert.h>
#include <stdbool.h>

@interface PuglTestResponderView : NSView
@end

@implementation PuglTestResponderView
- (BOOL)acceptsFirstResponder
{
  return YES;
}
@end

typedef struct {
  unsigned focusIn;
  unsigned focusOut;
  unsigned buttonPresses;
} TestState;

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
  TestState* const state = (TestState*)puglGetHandle(view);

  switch (event->type) {
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

int
main(void)
{
  NSAutoreleasePool* const pool = [NSAutoreleasePool new];

  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  assert(world);

  NSRect const frame = NSMakeRect(0.0, 0.0, 320.0, 200.0);
  NSWindow* const host =
    [[NSWindow alloc] initWithContentRect:frame
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

  TestState state = {0U, 0U, 0U};

  puglSetWorldString(world, PUGL_CLASS_NAME, "PuglMacEmbeddedTest");
  puglSetBackend(view, puglStubBackend());
  puglSetHandle(view, &state);
  puglSetEventFunc(view, onEvent);
  puglSetParent(view, (PuglNativeView)parent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 160U, 100U);

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
  assert(!puglHasFocus(view));
  assert(puglGrabFocus(view) == PUGL_FAILURE);
  assert(![host isVisible]);
  assert([host firstResponder] == sentinel);

  [NSApp activateIgnoringOtherApps:YES];
  [host makeKeyAndOrderFront:nil];
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
  assert(state.focusIn == 1U);
  assert(state.focusOut == 1U);

  // Passive show does not steal focus back.
  assert(!puglShow(view, PUGL_SHOW_PASSIVE));
  assert(![nativeView isHidden]);
  assert(puglGetVisible(view));
  assert(!puglHasFocus(view));
  assert([host firstResponder] != nativeView);
  assert(state.focusIn == 1U);

  [(id)nativeView mouseDown:mouseDownEvent(host)];
  assert(state.focusIn == 2U);
  assert([host firstResponder] == nativeView);
  assert(puglHasFocus(view));

  // Borrowed-window key transitions are reflected as Pugl focus transitions.
  [host resignKeyWindow];
  assert(state.focusOut == 2U);
  assert(!puglHasFocus(view));
  [host makeKeyWindow];
  assert(state.focusIn == 3U);
  assert(puglHasFocus(view));

  // Destroying a focused child must not call user code or destroy its host.
  puglFreeView(view);
  assert(state.focusOut == 2U);
  assert([host isVisible]);
  assert([host contentView] == parent);

  puglFreeWorld(world);

  [host orderOut:nil];
  [sentinel release];
  [parent release];
  [host release];
  [pool drain];
  return 0;
}
