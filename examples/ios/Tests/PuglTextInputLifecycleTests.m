// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include "internal.h"
#include "ios.h"

#include <pugl/pugl.h>
#include <pugl/stub.h>

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  unsigned focusIn;
  unsigned focusOut;
  unsigned text;
  uint32_t lastCharacter;
} PuglIOSTest45State;

static PuglStatus
puglIosTest45Event(PuglView* const view, const PuglEvent* const event)
{
  PuglIOSTest45State* const state =
    (PuglIOSTest45State*)puglGetHandle(view);
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
    state->lastCharacter = event->text.character;
    break;
  default:
    break;
  }

  return PUGL_SUCCESS;
}

static UIWindow*
puglIosTest45MakeWindow(UIViewController** const controller)
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
puglIosTest45ReleaseWindow(UIWindow* const window,
                           UIViewController* const controller)
{
  window.hidden = YES;
  window.rootViewController = nil;
  [controller release];
  [window release];
}

static PuglView*
puglIosTest45MakeView(PuglWorld* const          world,
                      UIView* const             parent,
                      PuglIOSTest45State* const state)
{
  PuglView* const view = puglNewView(world);
  if (!view || puglSetBackend(view, puglStubBackend()) ||
      puglSetEventFunc(view, puglIosTest45Event) ||
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

typedef enum {
  PUGL_IOS_TEST45_REENTER_NONE,
  PUGL_IOS_TEST45_REENTER_HIDE,
  PUGL_IOS_TEST45_REENTER_UNREALIZE,
  PUGL_IOS_TEST45_REENTER_FREE,
} PuglIOSTest45Reentry;

@interface PuglIOSTest45ReentrantTextInputView : PuglTextInputView {
@public
  PuglIOSTest45Reentry reentry;
}
@end

@implementation PuglIOSTest45ReentrantTextInputView

- (BOOL)becomeFirstResponder
{
  const BOOL changed = [super becomeFirstResponder];
  PuglView* const view = self->puglview;
  const PuglIOSTest45Reentry action = reentry;
  reentry = PUGL_IOS_TEST45_REENTER_NONE;

  if (changed && view) {
    switch (action) {
    case PUGL_IOS_TEST45_REENTER_HIDE:
      (void)puglHide(view);
      break;
    case PUGL_IOS_TEST45_REENTER_UNREALIZE:
      (void)puglUnrealize(view);
      break;
    case PUGL_IOS_TEST45_REENTER_FREE:
      puglFreeView(view);
      break;
    case PUGL_IOS_TEST45_REENTER_NONE:
      break;
    }
  }

  return changed;
}

@end

static PuglIOSTest45ReentrantTextInputView*
puglIosTest45InstallReentrantTextInput(PuglView* const view)
{
  PuglTextInputView* const oldTextInput = view->impl->textInputView;
  PuglIOSTest45ReentrantTextInputView* const textInput =
    [[PuglIOSTest45ReentrantTextInputView alloc] initWithFrame:CGRectZero];
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

typedef enum {
  PUGL_IOS_TEST45_CONFIGURE_FAIL,
  PUGL_IOS_TEST45_CREATE_FAIL_WITH_DRAW_VIEW,
  PUGL_IOS_TEST45_CREATE_SUCCESS_WITHOUT_DRAW_VIEW,
} PuglIOSTest45BackendMode;

typedef struct {
  PuglIOSTest45BackendMode mode;
  unsigned configureCalls;
  unsigned createCalls;
  unsigned destroyCalls;
} PuglIOSTest45BackendState;

static PuglStatus
puglIosTest45FaultEvent(PuglView* const view, const PuglEvent* const event)
{
  (void)view;
  (void)event;
  return PUGL_SUCCESS;
}

static PuglStatus
puglIosTest45Configure(PuglView* const view)
{
  PuglIOSTest45BackendState* const state =
    (PuglIOSTest45BackendState*)puglGetHandle(view);
  ++state->configureCalls;
  return state->mode == PUGL_IOS_TEST45_CONFIGURE_FAIL
           ? PUGL_BAD_CONFIGURATION
           : PUGL_SUCCESS;
}

static PuglStatus
puglIosTest45Create(PuglView* const view)
{
  PuglIOSTest45BackendState* const state =
    (PuglIOSTest45BackendState*)puglGetHandle(view);
  ++state->createCalls;

  if (state->mode == PUGL_IOS_TEST45_CREATE_FAIL_WITH_DRAW_VIEW) {
    view->impl->drawView = [[UIView alloc] initWithFrame:CGRectZero];
    return view->impl->drawView ? PUGL_BACKEND_FAILED : PUGL_NO_MEMORY;
  }

  return PUGL_SUCCESS;
}

static void
puglIosTest45Destroy(PuglView* const view)
{
  PuglIOSTest45BackendState* const state =
    (PuglIOSTest45BackendState*)puglGetHandle(view);
  ++state->destroyCalls;

  if (view->impl->drawView) {
    [view->impl->drawView removeFromSuperview];
    [view->impl->drawView release];
    view->impl->drawView = nil;
  }
}

static PuglStatus
puglIosTest45Enter(PuglView* const view, const PuglExposeEvent* const expose)
{
  (void)view;
  (void)expose;
  return PUGL_SUCCESS;
}

static PuglStatus
puglIosTest45Leave(PuglView* const view, const PuglExposeEvent* const expose)
{
  (void)view;
  (void)expose;
  return PUGL_SUCCESS;
}

static void*
puglIosTest45GetContext(PuglView* const view)
{
  (void)view;
  return NULL;
}

static const PuglBackend puglIosTest45FaultBackend = {
  puglIosTest45Configure,
  puglIosTest45Create,
  puglIosTest45Destroy,
  puglIosTest45Enter,
  puglIosTest45Leave,
  puglIosTest45GetContext,
};

static PuglView*
puglIosTest45PrepareFaultView(PuglWorld* const                 world,
                              UIView* const                    parent,
                              PuglIOSTest45BackendState* const state)
{
  PuglView* const view = puglNewView(world);
  if (!view || puglSetBackend(view, &puglIosTest45FaultBackend) ||
      puglSetEventFunc(view, puglIosTest45FaultEvent) ||
      puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 320U, 180U) ||
      puglSetParent(view, (PuglNativeView)(uintptr_t)parent)) {
    if (view) {
      puglFreeView(view);
    }
    return NULL;
  }

  puglSetHandle(view, state);
  return view;
}

@interface PuglIOSTest45LifecycleTests : XCTestCase
@end

@implementation PuglIOSTest45LifecycleTests

- (void)testSharedHostTeardownDoesNotAffectActiveSibling
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosTest45MakeWindow(&controller);
  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(world, NULL);

  PuglIOSTest45State stateA = {0U};
  PuglIOSTest45State stateB = {0U};
  PuglView* const viewA =
    puglIosTest45MakeView(world, controller.view, &stateA);
  PuglView* const viewB =
    puglIosTest45MakeView(world, controller.view, &stateB);
  XCTAssertNotEqual(viewA, NULL);
  XCTAssertNotEqual(viewB, NULL);

  XCTAssertEqual(puglGrabFocus(viewB), PUGL_SUCCESS);
  XCTAssertEqual(puglStartTextInput(viewB), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(viewB));

  XCTAssertEqual(puglHide(viewA), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(viewB));
  XCTAssertEqual(puglShow(viewA, PUGL_SHOW_PASSIVE), PUGL_SUCCESS);
  XCTAssertEqual(puglUnrealize(viewA), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(viewB));
  puglFreeView(viewA);

  XCTAssertTrue(puglIsTextInputActive(viewB));
  [viewB->impl->textInputView insertText:@"Z"];
  XCTAssertEqual(stateB.text, 1U);
  XCTAssertEqual(stateB.lastCharacter, (uint32_t)'Z');

  puglFreeView(viewB);
  puglFreeWorld(world);
  puglIosTest45ReleaseWindow(window, controller);
}

- (void)testReentrantHideDuringStartRecoversTransferState
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosTest45MakeWindow(&controller);
  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(world, NULL);

  PuglIOSTest45State state = {0U};
  PuglView* const view =
    puglIosTest45MakeView(world, controller.view, &state);
  XCTAssertNotEqual(view, NULL);
  XCTAssertEqual(puglGrabFocus(view), PUGL_SUCCESS);

  PuglIOSTest45ReentrantTextInputView* const textInput =
    puglIosTest45InstallReentrantTextInput(view);
  XCTAssertNotEqual(textInput, nil);
  textInput->reentry = PUGL_IOS_TEST45_REENTER_HIDE;

  const PuglStatus startStatus = puglStartTextInput(view);
  const bool activeAfterHide = puglIsTextInputActive(view);
  const bool focusedAfterHide = puglHasFocus(view);

  // Reentrant hide can race responder reconciliation.  Preserve UIKit truth:
  // text input may remain active, focus may fall back to the wrapper, or both
  // responders may resign.  The published logical focus must match whichever
  // native responder actually owns focus after the transaction unwinds.
  XCTAssertEqual(startStatus,
                 activeAfterHide ? PUGL_SUCCESS : PUGL_FAILURE);
  XCTAssertFalse(puglGetVisible(view));
  XCTAssertFalse(view->impl->responderTransfer);
  XCTAssertEqual(textInput.isFirstResponder, activeAfterHide);
  XCTAssertEqual(state.focusOut, focusedAfterHide ? 0U : 1U);

  if (focusedAfterHide) {
    XCTAssertTrue(activeAfterHide || view->impl->wrapperView.isFirstResponder);
  } else {
    XCTAssertFalse(activeAfterHide);
    XCTAssertFalse(view->impl->wrapperView.isFirstResponder);
  }

  // Once the reentrant transition has unwound, ordinary lifecycle operations
  // must remain usable regardless of the responder outcome above.
  XCTAssertEqual(puglShow(view, PUGL_SHOW_PASSIVE), PUGL_SUCCESS);
  XCTAssertEqual(puglStopTextInput(view), PUGL_SUCCESS);
  XCTAssertFalse(puglIsTextInputActive(view));
  XCTAssertEqual(puglGrabFocus(view), PUGL_SUCCESS);
  XCTAssertEqual(puglStartTextInput(view), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(view));
  XCTAssertEqual(puglStopTextInput(view), PUGL_SUCCESS);

  puglFreeView(view);
  puglFreeWorld(world);
  puglIosTest45ReleaseWindow(window, controller);
}

- (void)testReentrantUnrealizeDuringStartIsSafeAndReopenable
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosTest45MakeWindow(&controller);
  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(world, NULL);

  PuglIOSTest45State state = {0U};
  PuglView* const view =
    puglIosTest45MakeView(world, controller.view, &state);
  XCTAssertNotEqual(view, NULL);
  XCTAssertEqual(puglGrabFocus(view), PUGL_SUCCESS);

  PuglIOSTest45ReentrantTextInputView* const textInput =
    puglIosTest45InstallReentrantTextInput(view);
  XCTAssertNotEqual(textInput, nil);
  textInput->reentry = PUGL_IOS_TEST45_REENTER_UNREALIZE;

  XCTAssertEqual(puglStartTextInput(view), PUGL_FAILURE);
  XCTAssertFalse(puglIsTextInputActive(view));
  XCTAssertEqual(view->impl->wrapperView, nil);
  XCTAssertEqual(view->impl->textInputView, nil);

  XCTAssertEqual(puglShow(view, PUGL_SHOW_PASSIVE), PUGL_SUCCESS);
  XCTAssertEqual(puglGrabFocus(view), PUGL_SUCCESS);
  XCTAssertEqual(puglStartTextInput(view), PUGL_SUCCESS);
  XCTAssertEqual(puglStopTextInput(view), PUGL_SUCCESS);

  puglFreeView(view);
  puglFreeWorld(world);
  puglIosTest45ReleaseWindow(window, controller);
}

- (void)testReentrantFreeDuringStartDoesNotPoisonWorld
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosTest45MakeWindow(&controller);
  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(world, NULL);

  PuglIOSTest45State state = {0U};
  PuglView* const view =
    puglIosTest45MakeView(world, controller.view, &state);
  XCTAssertNotEqual(view, NULL);
  XCTAssertEqual(puglGrabFocus(view), PUGL_SUCCESS);

  PuglIOSTest45ReentrantTextInputView* const textInput =
    puglIosTest45InstallReentrantTextInput(view);
  XCTAssertNotEqual(textInput, nil);
  textInput->reentry = PUGL_IOS_TEST45_REENTER_FREE;

  XCTAssertEqual(puglStartTextInput(view), PUGL_FAILURE);
  XCTAssertEqual(world->numViews, 0U);

  PuglIOSTest45State replacementState = {0U};
  PuglView* const replacement =
    puglIosTest45MakeView(world, controller.view, &replacementState);
  XCTAssertNotEqual(replacement, NULL);
  XCTAssertEqual(puglGrabFocus(replacement), PUGL_SUCCESS);
  XCTAssertEqual(puglStartTextInput(replacement), PUGL_SUCCESS);
  XCTAssertEqual(puglStopTextInput(replacement), PUGL_SUCCESS);
  puglFreeView(replacement);

  puglFreeWorld(world);
  puglIosTest45ReleaseWindow(window, controller);
}

- (void)testRepeatedCreateStartStopUnrealizeDestroy
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosTest45MakeWindow(&controller);
  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(world, NULL);

  for (unsigned i = 0U; i < 16U; ++i) {
    PuglIOSTest45State state = {0U};
    PuglView* const view =
      puglIosTest45MakeView(world, controller.view, &state);
    XCTAssertNotEqual(view, NULL);
    XCTAssertEqual(puglGrabFocus(view), PUGL_SUCCESS);
    XCTAssertEqual(puglStartTextInput(view), PUGL_SUCCESS);
    [view->impl->textInputView insertText:@"x"];
    XCTAssertEqual(state.text, 1U);
    XCTAssertEqual(puglStopTextInput(view), PUGL_SUCCESS);
    XCTAssertEqual(puglUnrealize(view), PUGL_SUCCESS);
    puglFreeView(view);
    XCTAssertEqual(world->numViews, 0U);
  }

  puglFreeWorld(world);
  puglIosTest45ReleaseWindow(window, controller);
}

- (void)testPartialConstructionCleansNativeTextResources
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosTest45MakeWindow(&controller);
  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(world, NULL);

  PuglIOSTest45BackendState configureFailure = {
    PUGL_IOS_TEST45_CONFIGURE_FAIL, 0U, 0U, 0U};
  PuglView* view = puglIosTest45PrepareFaultView(
    world, controller.view, &configureFailure);
  XCTAssertNotEqual(view, NULL);
  XCTAssertEqual(puglShow(view, PUGL_SHOW_PASSIVE), PUGL_BAD_CONFIGURATION);
  XCTAssertEqual(configureFailure.configureCalls, 1U);
  XCTAssertEqual(configureFailure.createCalls, 0U);
  XCTAssertEqual(configureFailure.destroyCalls, 0U);
  XCTAssertEqual(view->impl->wrapperView, nil);
  XCTAssertEqual(view->impl->textInputView, nil);
  XCTAssertEqual(view->impl->drawView, nil);
  puglFreeView(view);

  PuglIOSTest45BackendState createFailure = {
    PUGL_IOS_TEST45_CREATE_FAIL_WITH_DRAW_VIEW, 0U, 0U, 0U};
  view = puglIosTest45PrepareFaultView(world, controller.view, &createFailure);
  XCTAssertNotEqual(view, NULL);
  XCTAssertEqual(puglShow(view, PUGL_SHOW_PASSIVE), PUGL_BACKEND_FAILED);
  XCTAssertEqual(createFailure.configureCalls, 1U);
  XCTAssertEqual(createFailure.createCalls, 1U);
  XCTAssertEqual(createFailure.destroyCalls, 1U);
  XCTAssertEqual(view->impl->wrapperView, nil);
  XCTAssertEqual(view->impl->textInputView, nil);
  XCTAssertEqual(view->impl->drawView, nil);
  puglFreeView(view);

  PuglIOSTest45BackendState missingDrawView = {
    PUGL_IOS_TEST45_CREATE_SUCCESS_WITHOUT_DRAW_VIEW, 0U, 0U, 0U};
  view = puglIosTest45PrepareFaultView(world, controller.view, &missingDrawView);
  XCTAssertNotEqual(view, NULL);
  XCTAssertEqual(puglShow(view, PUGL_SHOW_PASSIVE), PUGL_BACKEND_FAILED);
  XCTAssertEqual(missingDrawView.configureCalls, 1U);
  XCTAssertEqual(missingDrawView.createCalls, 1U);
  XCTAssertEqual(missingDrawView.destroyCalls, 1U);
  XCTAssertEqual(view->impl->wrapperView, nil);
  XCTAssertEqual(view->impl->textInputView, nil);
  XCTAssertEqual(view->impl->drawView, nil);
  puglFreeView(view);

  XCTAssertEqual(world->numViews, 0U);
  puglFreeWorld(world);
  puglIosTest45ReleaseWindow(window, controller);
}

@end
