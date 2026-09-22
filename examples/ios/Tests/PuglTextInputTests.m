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
  unsigned keyPress;
  unsigned keyRelease;
  unsigned text;
  unsigned edit;
  uint32_t lastKeycode;
  uint32_t lastCharacter;
  uint32_t lastHardwareKeycode;
  PuglMods lastState;
  bool startTextInputOnKeyPress;
  bool startTextInputOnText;
  bool unrealizeOnText;
  bool freeOnText;
} PuglIOSTestState;

@interface PuglIOSTestFaultTextInputView : PuglTextInputView {
@public
  BOOL refuseBecome;
  BOOL refuseResign;
}
@end

@implementation PuglIOSTestFaultTextInputView

- (BOOL)becomeFirstResponder
{
  return refuseBecome ? NO : [super becomeFirstResponder];
}

- (BOOL)resignFirstResponder
{
  return refuseResign ? NO : [super resignFirstResponder];
}

@end

@interface PuglIOSTestKey : NSObject
@end

@implementation PuglIOSTestKey

- (UIKeyboardHIDUsage)keyCode
{
  return (UIKeyboardHIDUsage)0x04U;
}

- (UIKeyModifierFlags)modifierFlags
{
  return 0U;
}

- (NSString*)characters
{
  return @"a";
}

- (NSString*)charactersIgnoringModifiers
{
  return @"a";
}

@end

@interface PuglIOSTestMultiKey : PuglIOSTestKey
@end

@implementation PuglIOSTestMultiKey

- (NSString*)characters
{
  return @"ab";
}

- (NSString*)charactersIgnoringModifiers
{
  return @"ab";
}

@end

@interface PuglIOSTestPress : NSObject {
@private
  PuglIOSTestKey* testKey;
}

- (id)initWithKey:(PuglIOSTestKey*)key;
- (UIKey*)key;

@end

@implementation PuglIOSTestPress

- (id)initWithKey:(PuglIOSTestKey*)key
{
  self = [super init];
  if (self) {
    testKey = [key retain];
  }
  return self;
}

- (void)dealloc
{
  [testKey release];
  [super dealloc];
}

- (UIKey*)key
{
  return (UIKey*)testKey;
}

@end

@interface PuglWrapperView (PuglIOSTestPressDispatch)
- (void)dispatchPresses:(NSSet<UIPress*>*)presses type:(PuglEventType)type;
@end

@interface PuglTextInputView (PuglIOSTestPressDispatch)
- (void)dispatchPresses:(NSSet<UIPress*>*)presses type:(PuglEventType)type;
@end

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
  case PUGL_KEY_PRESS:
    ++state->keyPress;
    state->lastHardwareKeycode = event->key.keycode;
    if (state->startTextInputOnKeyPress) {
      state->startTextInputOnKeyPress = false;
      (void)puglStartTextInput(view);
    }
    break;
  case PUGL_KEY_RELEASE:
    ++state->keyRelease;
    state->lastHardwareKeycode = event->key.keycode;
    break;
  case PUGL_TEXT:
    ++state->text;
    state->lastKeycode = event->text.keycode;
    state->lastCharacter = event->text.character;
    state->lastState = event->text.state;
    if (state->startTextInputOnText) {
      state->startTextInputOnText = false;
      (void)puglStartTextInput(view);
    }
    if (state->unrealizeOnText) {
      state->unrealizeOnText = false;
      (void)puglUnrealize(view);
    } else if (state->freeOnText) {
      state->freeOnText = false;
      puglFreeView(view);
    }
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
puglIosMakeTestView(PuglWorld* const        world,
                    UIView* const           parent,
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

static PuglIOSTestFaultTextInputView*
puglIosInstallFaultTextInput(PuglView* const view)
{
  PuglTextInputView* const oldTextInput = view->impl->textInputView;
  PuglIOSTestFaultTextInputView* const textInput =
    [[PuglIOSTestFaultTextInputView alloc] initWithFrame:CGRectZero];
  if (!textInput) {
    return nil;
  }

  textInput->puglview = view;
  textInput.backgroundColor = [UIColor clearColor];
  [view->impl->wrapperView addSubview:textInput];

  oldTextInput->puglview = NULL;
  [oldTextInput removeFromSuperview];
  [oldTextInput release];
  view->impl->textInputView = textInput;
  return textInput;
}

static UIWindow*
puglIosMakeTestWindow(UIViewController** const controller)
{
  UIWindow* const window =
    [[UIWindow alloc] initWithFrame:CGRectMake(0.0, 0.0, 640.0, 360.0)];
  *controller = [[UIViewController alloc] init];
  window.rootViewController = *controller;
  [window makeKeyAndVisible];
  [(*controller).view layoutIfNeeded];
  return window;
}

static void
puglIosReleaseTestWindow(UIWindow* const window,
                         UIViewController* const controller)
{
  window.hidden = YES;
  window.rootViewController = nil;
  [controller release];
  [window release];
}

@interface PuglTextInputTests : XCTestCase
@end

@implementation PuglTextInputTests

- (void)testTextInputLifecycleAndInstanceIsolation
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosMakeTestWindow(&controller);

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

  XCTAssertFalse(puglHasFocus(viewA));
  XCTAssertFalse(puglIsTextInputActive(viewA));
  XCTAssertEqual(puglStartTextInput(viewA), PUGL_BAD_CALL);

  XCTAssertEqual(puglGrabFocus(viewA), PUGL_SUCCESS);
  XCTAssertTrue(puglHasFocus(viewA));
  XCTAssertFalse(puglIsTextInputActive(viewA));
  XCTAssertEqual(stateA.focusIn, 1U);

  XCTAssertEqual(puglStartTextInput(viewA), PUGL_SUCCESS);
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

  XCTAssertEqual(puglStopTextInput(viewA), PUGL_SUCCESS);
  XCTAssertEqual(puglStopTextInput(viewA), PUGL_SUCCESS);
  XCTAssertFalse(puglIsTextInputActive(viewA));
  XCTAssertEqual(puglStartTextInput(viewA), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(viewA));

  XCTAssertEqual(puglGrabFocus(viewB), PUGL_SUCCESS);
  XCTAssertFalse(puglIsTextInputActive(viewA));
  XCTAssertEqual(stateA.focusOut, 1U);
  XCTAssertEqual(stateB.focusIn, 1U);

  XCTAssertEqual(puglStartTextInput(viewB), PUGL_SUCCESS);
  XCTAssertEqual(puglStartTextInput(viewB), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(viewB));
  XCTAssertEqual(stateB.focusIn, 1U);
  XCTAssertEqual(stateB.focusOut, 0U);

  XCTAssertEqual(puglHide(viewA), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(viewB));
  XCTAssertEqual(puglShow(viewA, PUGL_SHOW_PASSIVE), PUGL_SUCCESS);
  XCTAssertEqual(puglUnrealize(viewA), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(viewB));
  puglFreeView(viewA);

  const unsigned focusOutBeforeFree = stateB.focusOut;
  puglFreeView(viewB);
  XCTAssertEqual(stateB.focusOut, focusOutBeforeFree);

  puglFreeWorld(world);
  puglIosReleaseTestWindow(window, controller);
}

- (void)testHardwarePathDoesNotDuplicateCommittedText
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosMakeTestWindow(&controller);
  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(world, NULL);

  PuglIOSTestState state = {0U};
  PuglView* const view = puglIosMakeTestView(world, controller.view, &state);
  XCTAssertNotEqual(view, NULL);
  XCTAssertEqual(puglGrabFocus(view), PUGL_SUCCESS);

  PuglIOSTestKey* const key = [[PuglIOSTestKey alloc] init];
  PuglIOSTestPress* const press = [[PuglIOSTestPress alloc] initWithKey:key];
  NSSet* const presses = [NSSet setWithObject:(id)press];

  [view->impl->wrapperView dispatchPresses:(NSSet<UIPress*>*)presses
                                      type:PUGL_KEY_PRESS];
  XCTAssertEqual(state.keyPress, 1U);
  XCTAssertEqual(state.text, 1U);
  XCTAssertEqual(state.lastHardwareKeycode, 0x04U);
  XCTAssertEqual(state.lastKeycode, 0x04U);
  XCTAssertEqual(state.lastCharacter, (uint32_t)'a');

  state.keyPress = 0U;
  state.keyRelease = 0U;
  state.text = 0U;

  XCTAssertEqual(puglStartTextInput(view), PUGL_SUCCESS);
  [view->impl->textInputView dispatchPresses:(NSSet<UIPress*>*)presses
                                        type:PUGL_KEY_PRESS];
  [view->impl->textInputView dispatchPresses:(NSSet<UIPress*>*)presses
                                        type:PUGL_KEY_RELEASE];
  XCTAssertEqual(state.keyPress, 1U);
  XCTAssertEqual(state.keyRelease, 1U);
  XCTAssertEqual(state.text, 0U);
  XCTAssertEqual(state.lastHardwareKeycode, 0x04U);

  [view->impl->textInputView insertText:@"a"];
  XCTAssertEqual(state.text, 1U);
  XCTAssertEqual(state.lastKeycode, 0U);
  XCTAssertEqual(state.lastCharacter, (uint32_t)'a');
  XCTAssertEqual(state.lastState, 0U);

  [press release];
  [key release];
  puglFreeView(view);
  puglFreeWorld(world);
  puglIosReleaseTestWindow(window, controller);
}

- (void)testHardwareTextCallbackStopsWrapperCommittedTextAfterSessionStarts
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosMakeTestWindow(&controller);
  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(world, NULL);

  PuglIOSTestState state = {0U};
  state.startTextInputOnText = true;
  PuglView* const view = puglIosMakeTestView(world, controller.view, &state);
  XCTAssertNotEqual(view, NULL);
  XCTAssertEqual(puglGrabFocus(view), PUGL_SUCCESS);

  PuglIOSTestMultiKey* const key = [[PuglIOSTestMultiKey alloc] init];
  PuglIOSTestPress* const press =
    [[PuglIOSTestPress alloc] initWithKey:(PuglIOSTestKey*)key];
  NSSet* const presses = [NSSet setWithObject:(id)press];

  [view->impl->wrapperView dispatchPresses:(NSSet<UIPress*>*)presses
                                      type:PUGL_KEY_PRESS];

  XCTAssertEqual(state.keyPress, 1U);
  XCTAssertTrue(puglIsTextInputActive(view));
  XCTAssertTrue(puglHasFocus(view));
  XCTAssertEqual(state.text, 1U);
  XCTAssertEqual(state.lastCharacter, (uint32_t)'a');

  [view->impl->textInputView insertText:@"b"];
  XCTAssertEqual(state.text, 2U);
  XCTAssertEqual(state.lastKeycode, 0U);
  XCTAssertEqual(state.lastCharacter, (uint32_t)'b');

  [press release];
  [key release];
  puglFreeView(view);
  puglFreeWorld(world);
  puglIosReleaseTestWindow(window, controller);
}

- (void)testKeyPressCallbackCanStartTextInputWithoutDuplicateCommittedText
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosMakeTestWindow(&controller);
  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(world, NULL);

  PuglIOSTestState state = {0U};
  state.startTextInputOnKeyPress = true;
  PuglView* const view = puglIosMakeTestView(world, controller.view, &state);
  XCTAssertNotEqual(view, NULL);
  XCTAssertEqual(puglGrabFocus(view), PUGL_SUCCESS);

  PuglIOSTestKey* const key = [[PuglIOSTestKey alloc] init];
  PuglIOSTestPress* const press = [[PuglIOSTestPress alloc] initWithKey:key];
  NSSet* const presses = [NSSet setWithObject:(id)press];

  [view->impl->wrapperView dispatchPresses:(NSSet<UIPress*>*)presses
                                      type:PUGL_KEY_PRESS];

  XCTAssertEqual(state.keyPress, 1U);
  XCTAssertTrue(puglIsTextInputActive(view));
  XCTAssertTrue(puglHasFocus(view));
  XCTAssertEqual(state.text, 0U);

  [view->impl->textInputView insertText:@"a"];
  XCTAssertEqual(state.text, 1U);
  XCTAssertEqual(state.lastKeycode, 0U);
  XCTAssertEqual(state.lastCharacter, (uint32_t)'a');

  [press release];
  [key release];
  puglFreeView(view);
  puglFreeWorld(world);
  puglIosReleaseTestWindow(window, controller);
}

- (void)testResponderFailuresRemainTruthful
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosMakeTestWindow(&controller);
  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(world, NULL);

  PuglIOSTestState state = {0U};
  PuglView* const view = puglIosMakeTestView(world, controller.view, &state);
  XCTAssertNotEqual(view, NULL);

  PuglIOSTestFaultTextInputView* const textInput =
    puglIosInstallFaultTextInput(view);
  XCTAssertNotEqual(textInput, nil);

  XCTAssertEqual(puglGrabFocus(view), PUGL_SUCCESS);
  textInput->refuseBecome = YES;
  XCTAssertEqual(puglStartTextInput(view), PUGL_FAILURE);
  XCTAssertFalse(puglIsTextInputActive(view));
  XCTAssertTrue(puglHasFocus(view));
  XCTAssertEqual(state.focusOut, 0U);

  textInput->refuseBecome = NO;
  XCTAssertEqual(puglStartTextInput(view), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(view));

  textInput->refuseResign = YES;
  XCTAssertEqual(puglStopTextInput(view), PUGL_FAILURE);
  XCTAssertTrue(puglIsTextInputActive(view));
  XCTAssertTrue(puglHasFocus(view));
  XCTAssertEqual(state.focusOut, 0U);

  textInput->refuseResign = NO;
  XCTAssertEqual(puglStopTextInput(view), PUGL_SUCCESS);
  XCTAssertFalse(puglIsTextInputActive(view));
  XCTAssertTrue(puglHasFocus(view));

  puglFreeView(view);
  puglFreeWorld(world);
  puglIosReleaseTestWindow(window, controller);
}

- (void)testTextCallbackCanUnrealizeOwner
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosMakeTestWindow(&controller);
  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(world, NULL);

  PuglIOSTestState state = {0U};
  state.unrealizeOnText = true;
  PuglView* const view = puglIosMakeTestView(world, controller.view, &state);
  XCTAssertNotEqual(view, NULL);

  XCTAssertEqual(puglGrabFocus(view), PUGL_SUCCESS);
  XCTAssertEqual(puglStartTextInput(view), PUGL_SUCCESS);
  [view->impl->textInputView insertText:@"AB"];

  XCTAssertEqual(state.text, 1U);
  XCTAssertFalse(puglIsTextInputActive(view));
  XCTAssertEqual(puglShow(view, PUGL_SHOW_PASSIVE), PUGL_SUCCESS);

  puglFreeView(view);
  puglFreeWorld(world);
  puglIosReleaseTestWindow(window, controller);
}

- (void)testTextCallbackCanFreeOwner
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosMakeTestWindow(&controller);
  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(world, NULL);

  PuglIOSTestState state = {0U};
  state.freeOnText = true;
  PuglView* const view = puglIosMakeTestView(world, controller.view, &state);
  XCTAssertNotEqual(view, NULL);

  XCTAssertEqual(puglGrabFocus(view), PUGL_SUCCESS);
  XCTAssertEqual(puglStartTextInput(view), PUGL_SUCCESS);
  [view->impl->textInputView insertText:@"AB"];
  XCTAssertEqual(state.text, 1U);

  puglFreeWorld(world);
  puglIosReleaseTestWindow(window, controller);
}

- (void)testReopenAndIndependentWorldIsolation
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosMakeTestWindow(&controller);

  PuglWorld* const worldA = puglNewWorld(PUGL_MODULE, 0U);
  PuglWorld* const worldB = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(worldA, NULL);
  XCTAssertNotEqual(worldB, NULL);

  PuglIOSTestState stateA = {0U};
  PuglIOSTestState stateB = {0U};
  PuglView* const viewA =
    puglIosMakeTestView(worldA, controller.view, &stateA);
  PuglView* const viewB =
    puglIosMakeTestView(worldB, controller.view, &stateB);
  XCTAssertNotEqual(viewA, NULL);
  XCTAssertNotEqual(viewB, NULL);

  XCTAssertEqual(puglGrabFocus(viewA), PUGL_SUCCESS);
  XCTAssertEqual(puglStartTextInput(viewA), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(viewA));

  XCTAssertEqual(puglUnrealize(viewA), PUGL_SUCCESS);
  XCTAssertFalse(puglIsTextInputActive(viewA));
  XCTAssertEqual(puglSetTextInputFlags(viewA, 0U), PUGL_SUCCESS);
  XCTAssertEqual(puglShow(viewA, PUGL_SHOW_PASSIVE), PUGL_SUCCESS);
  XCTAssertEqual(puglGrabFocus(viewA), PUGL_SUCCESS);
  XCTAssertEqual(puglStartTextInput(viewA), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(viewA));

  XCTAssertEqual(puglGrabFocus(viewB), PUGL_SUCCESS);
  XCTAssertEqual(puglStartTextInput(viewB), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(viewB));

  puglFreeView(viewA);
  puglFreeWorld(worldA);
  XCTAssertTrue(puglIsTextInputActive(viewB));

  [viewB->impl->textInputView insertText:@"B"];
  XCTAssertEqual(stateB.text, 1U);
  XCTAssertEqual(stateB.lastCharacter, (uint32_t)'B');
  XCTAssertEqual(stateB.lastKeycode, 0U);
  XCTAssertEqual(stateB.lastState, 0U);

  puglFreeView(viewB);
  puglFreeWorld(worldB);
  puglIosReleaseTestWindow(window, controller);
}

@end
