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
} PuglIOSTestStopState;

static PuglStatus
puglIosTestStopEvent(PuglView* const view, const PuglEvent* const event)
{
  PuglIOSTestStopState* const state =
    (PuglIOSTestStopState*)puglGetHandle(view);
  if (!state) {
    return PUGL_SUCCESS;
  }

  if (event->type == PUGL_FOCUS_IN) {
    ++state->focusIn;
  } else if (event->type == PUGL_FOCUS_OUT) {
    ++state->focusOut;
  }

  return PUGL_SUCCESS;
}

@interface PuglIOSTestStopWrapper : PuglWrapperView {
@public
  BOOL refuseBecome;
}
@end

@implementation PuglIOSTestStopWrapper

- (BOOL)becomeFirstResponder
{
  return refuseBecome ? NO : [super becomeFirstResponder];
}

@end

static UIWindow*
puglIosTestStopMakeWindow(UIViewController** const controller)
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
puglIosTestStopReleaseWindow(UIWindow* const window,
                             UIViewController* const controller)
{
  window.hidden = YES;
  window.rootViewController = nil;
  [controller release];
  [window release];
}

static PuglView*
puglIosTestStopMakeView(PuglWorld* const             world,
                        UIView* const                parent,
                        PuglIOSTestStopState* const  state)
{
  PuglView* const view = puglNewView(world);
  if (!view || puglSetBackend(view, puglStubBackend()) ||
      puglSetEventFunc(view, puglIosTestStopEvent) ||
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

static PuglIOSTestStopWrapper*
puglIosTestStopInstallWrapper(PuglView* const view)
{
  PuglWrapperView* const oldWrapper = view->impl->wrapperView;
  UIView* const parent = oldWrapper.superview;
  PuglIOSTestStopWrapper* const wrapper =
    [[PuglIOSTestStopWrapper alloc] initWithFrame:oldWrapper.frame];
  if (!wrapper || !parent) {
    [wrapper release];
    return nil;
  }

  wrapper.hidden = oldWrapper.hidden;
  wrapper.autoresizingMask = oldWrapper.autoresizingMask;
  wrapper->puglview = view;
  [parent insertSubview:wrapper aboveSubview:oldWrapper];

  NSArray* const children = [oldWrapper.subviews copy];
  for (UIView* const child in children) {
    [wrapper addSubview:child];
  }
  [children release];

  oldWrapper->puglview = NULL;
  [oldWrapper removeFromSuperview];
  [oldWrapper release];
  view->impl->wrapperView = wrapper;
  return wrapper;
}

@interface PuglTextInputStopTests : XCTestCase
@end

@implementation PuglTextInputStopTests

- (void)testStopSucceedsWhenTextResignsButWrapperCannotRecoverFocus
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosTestStopMakeWindow(&controller);
  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(world, NULL);

  PuglIOSTestStopState state = {0U};
  PuglView* const view =
    puglIosTestStopMakeView(world, controller.view, &state);
  XCTAssertNotEqual(view, NULL);

  PuglIOSTestStopWrapper* const wrapper =
    puglIosTestStopInstallWrapper(view);
  XCTAssertNotEqual(wrapper, nil);

  XCTAssertEqual(puglGrabFocus(view), PUGL_SUCCESS);
  XCTAssertEqual(state.focusIn, 1U);
  XCTAssertEqual(state.focusOut, 0U);
  XCTAssertEqual(puglStartTextInput(view), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(view));
  XCTAssertTrue(puglHasFocus(view));

  wrapper->refuseBecome = YES;
  XCTAssertEqual(puglStopTextInput(view), PUGL_SUCCESS);
  XCTAssertFalse(puglIsTextInputActive(view));
  XCTAssertFalse(puglHasFocus(view));
  XCTAssertEqual(state.focusIn, 1U);
  XCTAssertEqual(state.focusOut, 1U);

  puglFreeView(view);
  puglFreeWorld(world);
  puglIosTestStopReleaseWindow(window, controller);
}

- (void)testHideAndUnrealizeEndNormalActiveSession
{
  UIViewController* controller = nil;
  UIWindow* const window = puglIosTestStopMakeWindow(&controller);
  PuglWorld* const world = puglNewWorld(PUGL_MODULE, 0U);
  XCTAssertNotEqual(world, NULL);

  PuglIOSTestStopState state = {0U};
  PuglView* const view =
    puglIosTestStopMakeView(world, controller.view, &state);
  XCTAssertNotEqual(view, NULL);

  XCTAssertEqual(puglGrabFocus(view), PUGL_SUCCESS);
  XCTAssertEqual(puglStartTextInput(view), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(view));
  XCTAssertEqual(puglHide(view), PUGL_SUCCESS);
  XCTAssertFalse(puglIsTextInputActive(view));
  XCTAssertFalse(puglGetVisible(view));

  XCTAssertEqual(puglShow(view, PUGL_SHOW_PASSIVE), PUGL_SUCCESS);
  XCTAssertEqual(puglGrabFocus(view), PUGL_SUCCESS);
  XCTAssertEqual(puglStartTextInput(view), PUGL_SUCCESS);
  XCTAssertTrue(puglIsTextInputActive(view));
  XCTAssertEqual(puglUnrealize(view), PUGL_SUCCESS);
  XCTAssertFalse(puglIsTextInputActive(view));
  XCTAssertEqual(view->impl->wrapperView, nil);
  XCTAssertEqual(view->impl->textInputView, nil);

  puglFreeView(view);
  puglFreeWorld(world);
  puglIosTestStopReleaseWindow(window, controller);
}

@end
