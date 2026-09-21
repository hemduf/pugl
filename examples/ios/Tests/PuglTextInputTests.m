// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include "internal.h"
#include "ios.h"

#include <pugl/pugl.h>
#include <pugl/stub.h>

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

typedef struct {
  unsigned focusIn;
  unsigned focusOut;
  unsigned text;
  unsigned edit;
  uint32_t lastKeycode;
  uint32_t lastCharacter;
  PuglMods lastState;
} PuglIOSTestState;

static PuglStatus
puglIosTestEvent(PuglView* const view, const PuglEvent* const event)
{
  PuglIOSTestState* const state = (PuglIOSTestState*)puglGetHandle(view);
  if (!state) {
    return PUGL_SUCCESS;
  }

  switch (event->type) {
  case PUGL_FOCUS_IN:
    ++state->focusIn;
    break;
  case PUGL_FOCUS_OUT:
    ++state->focusOut;
    break;
  case PUGL_TEXT:
    ++state->text;
    state->lastKeycode = event->text.keycode;
    state->lastCharacter = event->text.character;
    state->lastState = event->text.state;
    break;
  case PUGL_TEXT_EDIT:
    ++state->edit;
    break;
  default:
    break;
  }

  return PUGL_SUCCESS;
}

static PuglView*
puglIosMakeTestView(PuglWorld* const      world,
                    UIView* const         parent,
                    PuglIOSTestState* const state)
{
  PuglView* const view = puglNewView(world);
  if (!view ||
      puglSetBackend(view, puglStubBackend()) ||
      puglSetEventFunc(view, puglIosTestEvent) ||
      puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 320U, 180U) ||
      puglSetParent(view, (PuglNativeView)(uintptr_t)parent)) {
    if (view) {
      puglFreeView(view);
    }
    return NULL;
  }

  puglSetHandle(view, state);
  if (puglShow(view, PUGL_SHOW_PASSIVE)) {
    puglFreeView(view);
    return NULL;
  }

  return view;
}

@interface PuglTextInputTests : XCTestCase
@end

@implementation PuglTextInputTests

- (void)testTextInputLifecycleAndInstanceIsolation
{
  UIWindow* const window =
    [[UIWindow alloc] initWithFrame:CGRectMake(0.0, 0.0, 640.0, 360.0)];
  UIViewController* const controller = [[UIViewController alloc] init];
  window.rootViewController = controller;
  [window makeKeyAndVisible];
  [controller.view layoutIfNeeded];

  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(world, NULL);

  PuglIOSTestState stateA = {0U};
  PuglIOSTestState stateB = {0U};
  PuglView* const viewA =
    puglIosMakeTestView(world, controller.view, &stateA);
  PuglView* const viewB =
    puglIosMakeTestView(world, controller.view, &stateB);
  XCTAssertNotEqual(viewA, NULL);
  XCTAssertNotEqual(viewB, NULL);

  XCTAssertEqual(puglGrabFocus(viewA), PUGL_SUCCESS);
  XCTAssertTrue(puglHasFocus(viewA));
  XCTAssertEqual(stateA.focusIn, 1U);

  XCTAssertEqual(puglStartTextInput(viewA), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(viewA));
  XCTAssertTrue(puglHasFocus(viewA));
  XCTAssertEqual(stateA.focusIn, 1U);
  XCTAssertEqual(stateA.focusOut, 0U);

  XCTAssertEqual(puglGrabFocus(viewA), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(viewA));

  XCTAssertEqual(
    puglSetTextInputFlags(viewA, PUGL_TEXT_INPUT_HAS_TEXT), PUGL_SUCCESS);
  XCTAssertTrue([viewA->impl->textInputView hasText]);

  [viewA->impl->textInputView insertText:@"A😀"];
  XCTAssertEqual(stateA.text, 2U);
  XCTAssertEqual(stateA.lastCharacter, 0x1F600U);
  XCTAssertEqual(stateA.lastKeycode, 0U);
  XCTAssertEqual(stateA.lastState, 0U);

  [viewA->impl->textInputView deleteBackward];
  XCTAssertEqual(stateA.edit, 1U);

  XCTAssertEqual(puglGrabFocus(viewB), PUGL_SUCCESS);
  XCTAssertFalse(puglIsTextInputActive(viewA));
  XCTAssertEqual(stateA.focusOut, 1U);
  XCTAssertEqual(stateB.focusIn, 1U);

  XCTAssertEqual(puglStartTextInput(viewB), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(viewB));
  XCTAssertEqual(stateB.focusIn, 1U);
  XCTAssertEqual(stateB.focusOut, 0U);

  XCTAssertEqual(puglUnrealize(viewA), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(viewB));
  puglFreeView(viewA);

  const unsigned focusOutBeforeFree = stateB.focusOut;
  puglFreeView(viewB);
  XCTAssertEqual(stateB.focusOut, focusOutBeforeFree);

  puglFreeWorld(world);

  window.hidden = YES;
  window.rootViewController = nil;
  [controller release];
  [window release];
}

@end
