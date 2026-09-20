// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include "ios.h"

#include "internal.h"
#include "macros.h"
#include "platform.h"

#include <pugl/pugl.h>

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static CGFloat
puglIosScale(const PuglView* const view)
{
  if (view && view->impl && view->impl->wrapperView.window.screen) {
    return view->impl->wrapperView.window.screen.scale;
  }

  if (view && view->world && view->world->impl && view->world->impl->screen) {
    return view->world->impl->screen.scale;
  }

  return [UIScreen mainScreen].scale;
}

static CGRect
puglIosRectToPoints(const PuglView* const view,
                    const PuglCoord      x,
                    const PuglCoord      y,
                    const PuglSpan       width,
                    const PuglSpan       height)
{
  const CGFloat scale = puglIosScale(view);
  return CGRectMake((CGFloat)x / scale,
                    (CGFloat)y / scale,
                    (CGFloat)width / scale,
                    (CGFloat)height / scale);
}

static PuglViewStyleFlags
puglIosViewStyle(const PuglView* const view)
{
  if (!view || !view->impl || !view->impl->wrapperView) {
    return 0U;
  }

  PuglWrapperView* const wrapper = view->impl->wrapperView;
  const bool hidden = wrapper.hidden ||
                      (view->impl->window && view->impl->window.hidden);
  return hidden ? PUGL_VIEW_STYLE_HIDDEN : PUGL_VIEW_STYLE_MAPPED;
}

static uint32_t
puglIosModifiers(const UIKeyModifierFlags flags)
{
  return ((flags & UIKeyModifierShift) ? PUGL_MOD_SHIFT : 0U) |
         ((flags & UIKeyModifierControl) ? PUGL_MOD_CTRL : 0U) |
         ((flags & UIKeyModifierAlternate) ? PUGL_MOD_ALT : 0U) |
         ((flags & UIKeyModifierCommand) ? PUGL_MOD_SUPER : 0U) |
         ((flags & UIKeyModifierAlphaShift) ? PUGL_MOD_CAPS_LOCK : 0U);
}

static uint32_t
puglIosFirstScalar(NSString* const string)
{
  if (!string.length) {
    return 0U;
  }

  const unichar first = [string characterAtIndex:0U];
  if (first >= 0xD800U && first <= 0xDBFFU && string.length > 1U) {
    const unichar second = [string characterAtIndex:1U];
    if (second >= 0xDC00U && second <= 0xDFFFU) {
      return 0x10000U + (((uint32_t)first - 0xD800U) << 10U) +
             ((uint32_t)second - 0xDC00U);
    }
  }

  return (first >= 0xDC00U && first <= 0xDFFFU) ? 0xFFFDU : (uint32_t)first;
}

static size_t
puglIosEncodeUtf8(const uint32_t character, char output[8])
{
  if (character <= 0x7FU) {
    output[0] = (char)character;
    output[1] = '\0';
    return 1U;
  }

  if (character <= 0x7FFU) {
    output[0] = (char)(0xC0U | (character >> 6U));
    output[1] = (char)(0x80U | (character & 0x3FU));
    output[2] = '\0';
    return 2U;
  }

  if (character <= 0xFFFFU) {
    output[0] = (char)(0xE0U | (character >> 12U));
    output[1] = (char)(0x80U | ((character >> 6U) & 0x3FU));
    output[2] = (char)(0x80U | (character & 0x3FU));
    output[3] = '\0';
    return 3U;
  }

  if (character <= 0x10FFFFU) {
    output[0] = (char)(0xF0U | (character >> 18U));
    output[1] = (char)(0x80U | ((character >> 12U) & 0x3FU));
    output[2] = (char)(0x80U | ((character >> 6U) & 0x3FU));
    output[3] = (char)(0x80U | (character & 0x3FU));
    output[4] = '\0';
    return 4U;
  }

  output[0] = (char)0xEF;
  output[1] = (char)0xBF;
  output[2] = (char)0xBD;
  output[3] = '\0';
  return 3U;
}

static uint32_t
puglIosKey(const UIKey* const key)
{
  if (!key) {
    return 0U;
  }

  const uint32_t hid = (uint32_t)key.keyCode;
  if (hid >= 0x04U && hid <= 0x1DU) {
    return (uint32_t)'a' + (hid - 0x04U);
  }

  switch (hid) {
  case 0x28U: return PUGL_KEY_ENTER;
  case 0x29U: return PUGL_KEY_ESCAPE;
  case 0x2AU: return PUGL_KEY_BACKSPACE;
  case 0x2BU: return PUGL_KEY_TAB;
  case 0x2CU: return PUGL_KEY_SPACE;
  case 0x3AU: return PUGL_KEY_F1;
  case 0x3BU: return PUGL_KEY_F2;
  case 0x3CU: return PUGL_KEY_F3;
  case 0x3DU: return PUGL_KEY_F4;
  case 0x3EU: return PUGL_KEY_F5;
  case 0x3FU: return PUGL_KEY_F6;
  case 0x40U: return PUGL_KEY_F7;
  case 0x41U: return PUGL_KEY_F8;
  case 0x42U: return PUGL_KEY_F9;
  case 0x43U: return PUGL_KEY_F10;
  case 0x44U: return PUGL_KEY_F11;
  case 0x45U: return PUGL_KEY_F12;
  case 0x49U: return PUGL_KEY_INSERT;
  case 0x4AU: return PUGL_KEY_HOME;
  case 0x4BU: return PUGL_KEY_PAGE_UP;
  case 0x4CU: return PUGL_KEY_DELETE;
  case 0x4DU: return PUGL_KEY_END;
  case 0x4EU: return PUGL_KEY_PAGE_DOWN;
  case 0x4FU: return PUGL_KEY_RIGHT;
  case 0x50U: return PUGL_KEY_LEFT;
  case 0x51U: return PUGL_KEY_DOWN;
  case 0x52U: return PUGL_KEY_UP;
  default: break;
  }

  return puglIosFirstScalar(key.charactersIgnoringModifiers);
}

static void
puglIosDispatchText(PuglWrapperView* const wrapper,
                    const UIKey* const      key,
                    const PuglMods          state)
{
  if (!wrapper || !key || !key.characters.length ||
      (state & (PUGL_MOD_CTRL | PUGL_MOD_SUPER))) {
    return;
  }

  NSString* const text = key.characters;
  for (NSUInteger i = 0U; i < text.length;) {
    const unichar first = [text characterAtIndex:i++];
    uint32_t scalar = first;

    if (first >= 0xD800U && first <= 0xDBFFU && i < text.length) {
      const unichar second = [text characterAtIndex:i];
      if (second >= 0xDC00U && second <= 0xDFFFU) {
        ++i;
        scalar = 0x10000U + (((uint32_t)first - 0xD800U) << 10U) +
                 ((uint32_t)second - 0xDC00U);
      }
    }

    if ((scalar < 0x20U && scalar != '\t' && scalar != '\n' &&
         scalar != '\r') ||
        scalar == 0x7FU) {
      continue;
    }

    PuglTextEvent textEvent = {
      PUGL_TEXT,
      0U,
      puglGetTime(wrapper->puglview->world),
      0.0,
      0.0,
      0.0,
      0.0,
      state,
      (uint32_t)key.keyCode,
      scalar,
      {0},
    };
    puglIosEncodeUtf8(scalar, textEvent.string);

    PuglEvent event;
    memset(&event, 0, sizeof(event));
    event.text = textEvent;
    puglDispatchEvent(wrapper->puglview, &event);
  }
}

@implementation PuglWrapperView

- (id)initWithFrame:(CGRect)frame
{
  self = [super initWithFrame:frame];
  if (self) {
    self.multipleTouchEnabled = YES;
    self.autoresizesSubviews = YES;
    userTimers = [[NSMutableDictionary alloc] init];
    pendingEvents = [[NSMutableArray alloc] init];
    pendingEventLock = [[NSLock alloc] init];
  }
  return self;
}

- (void)dealloc
{
  for (NSTimer* timer in [userTimers allValues]) {
    [timer invalidate];
  }

  [activeTouch release];
  [pendingEventLock release];
  [pendingEvents release];
  [userTimers release];
  [super dealloc];
}

- (BOOL)canBecomeFirstResponder
{
  return YES;
}

- (BOOL)becomeFirstResponder
{
  const BOOL changed = [super becomeFirstResponder];
  if (changed && puglview && puglview->stage >= PUGL_VIEW_STAGE_REALIZED) {
    const PuglFocusEvent focus = {
      PUGL_FOCUS_IN, 0U, PUGL_CROSSING_NORMAL};
    PuglEvent event;
    memset(&event, 0, sizeof(event));
    event.focus = focus;
    puglDispatchEvent(puglview, &event);
  }
  return changed;
}

- (BOOL)resignFirstResponder
{
  const BOOL wasFirstResponder = self.isFirstResponder;
  const BOOL changed = [super resignFirstResponder];
  if (wasFirstResponder && changed && puglview &&
      puglview->stage >= PUGL_VIEW_STAGE_REALIZED) {
    const PuglFocusEvent focus = {
      PUGL_FOCUS_OUT, 0U, PUGL_CROSSING_NORMAL};
    PuglEvent event;
    memset(&event, 0, sizeof(event));
    event.focus = focus;
    puglDispatchEvent(puglview, &event);
  }
  return changed;
}

- (void)layoutSubviews
{
  [super layoutSubviews];

  if (puglview && puglview->impl->drawView) {
    puglview->impl->drawView.frame = self.bounds;
  }

  if (puglview && puglview->stage >= PUGL_VIEW_STAGE_REALIZED) {
    [self dispatchCurrentConfiguration];
  }
}

- (PuglStatus)dispatchCurrentConfiguration
{
  if (!puglview || puglview->stage < PUGL_VIEW_STAGE_REALIZED) {
    return PUGL_SUCCESS;
  }

  const CGFloat scale = puglIosScale(puglview);
  const CGRect frame = self.frame;
  const PuglConfigureEvent configure = {
    PUGL_CONFIGURE,
    0U,
    (PuglCoord)lrint(frame.origin.x * scale),
    (PuglCoord)lrint(frame.origin.y * scale),
    (PuglSpan)MAX(1.0, round(frame.size.width * scale)),
    (PuglSpan)MAX(1.0, round(frame.size.height * scale)),
    puglIosViewStyle(puglview),
  };

  if (puglview->lastConfigure.type == PUGL_CONFIGURE &&
      puglview->lastConfigure.x == configure.x &&
      puglview->lastConfigure.y == configure.y &&
      puglview->lastConfigure.width == configure.width &&
      puglview->lastConfigure.height == configure.height &&
      puglview->lastConfigure.style == configure.style) {
    return PUGL_SUCCESS;
  }

  PuglEvent event;
  memset(&event, 0, sizeof(event));
  event.configure = configure;
  return puglDispatchEvent(puglview, &event);
}

- (void)dispatchExpose:(CGRect)rect
{
  if (!puglview || puglview->stage < PUGL_VIEW_STAGE_CONFIGURED || self.hidden) {
    return;
  }

  const CGFloat scale = puglIosScale(puglview);
  const PuglExposeEvent expose = {
    PUGL_EXPOSE,
    0U,
    (PuglCoord)lrint(rect.origin.x * scale),
    (PuglCoord)lrint(rect.origin.y * scale),
    (PuglSpan)MAX(1.0, ceil(rect.size.width * scale)),
    (PuglSpan)MAX(1.0, ceil(rect.size.height * scale)),
  };

  PuglEvent event;
  memset(&event, 0, sizeof(event));
  event.expose = expose;
  puglDispatchEvent(puglview, &event);
}

- (void)queueEvent:(const PuglEvent*)event
{
  if (!event) {
    return;
  }

  NSData* const data = [NSData dataWithBytes:event length:sizeof(PuglEvent)];
  [pendingEventLock lock];
  [pendingEvents addObject:data];
  [pendingEventLock unlock];

  CFRunLoopWakeUp(CFRunLoopGetMain());
}

- (void)drainPendingEvents
{
  [pendingEventLock lock];
  NSArray* const events = [pendingEvents copy];
  [pendingEvents removeAllObjects];
  [pendingEventLock unlock];

  for (NSData* const data in events) {
    if (data.length == sizeof(PuglEvent)) {
      PuglEvent event;
      memcpy(&event, data.bytes, sizeof(event));
      puglDispatchEvent(puglview, &event);
    }
  }

  [events release];
}

- (void)timerTick:(NSTimer*)timer
{
  const uintptr_t timerId =
    (uintptr_t)[(NSNumber*)timer.userInfo unsignedLongLongValue];
  const PuglTimerEvent timerEvent = {PUGL_TIMER, 0U, timerId};

  PuglEvent event;
  memset(&event, 0, sizeof(event));
  event.timer = timerEvent;
  puglDispatchEvent(puglview, &event);
}

- (void)dispatchPointerType:(PuglEventType)type touch:(UITouch*)touch
{
  if (!puglview || !touch) {
    return;
  }

  const CGFloat scale = puglIosScale(puglview);
  const CGPoint local = [touch locationInView:self];
  const CGPoint root = [touch locationInView:nil];
  const double time = puglGetTime(puglview->world);

  PuglEvent event;
  memset(&event, 0, sizeof(event));

  if (type == PUGL_BUTTON_PRESS || type == PUGL_BUTTON_RELEASE) {
    event.button = (PuglButtonEvent){
      type,
      0U,
      time,
      local.x * scale,
      local.y * scale,
      root.x * scale,
      root.y * scale,
      0U,
      0U,
    };
  } else if (type == PUGL_MOTION) {
    event.motion = (PuglMotionEvent){
      PUGL_MOTION,
      0U,
      time,
      local.x * scale,
      local.y * scale,
      root.x * scale,
      root.y * scale,
      0U,
    };
  } else {
    event.crossing = (PuglCrossingEvent){
      type,
      0U,
      time,
      local.x * scale,
      local.y * scale,
      root.x * scale,
      root.y * scale,
      0U,
      PUGL_CROSSING_NORMAL,
    };
  }

  puglDispatchEvent(puglview, &event);
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
  (void)event;
  if (!activeTouch) {
    activeTouch = [[touches anyObject] retain];
    if (activeTouch) {
      [self dispatchPointerType:PUGL_POINTER_IN touch:activeTouch];
      [self dispatchPointerType:PUGL_BUTTON_PRESS touch:activeTouch];
    }
  }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
  (void)event;
  if (activeTouch && [touches containsObject:activeTouch]) {
    [self dispatchPointerType:PUGL_MOTION touch:activeTouch];
  }
}

- (void)finishTouches:(NSSet<UITouch*>*)touches
{
  if (activeTouch && [touches containsObject:activeTouch]) {
    [self dispatchPointerType:PUGL_BUTTON_RELEASE touch:activeTouch];
    [self dispatchPointerType:PUGL_POINTER_OUT touch:activeTouch];
    [activeTouch release];
    activeTouch = nil;
  }
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
  (void)event;
  [self finishTouches:touches];
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
  (void)event;
  [self finishTouches:touches];
}

- (void)dispatchPresses:(NSSet<UIPress*>*)presses type:(PuglEventType)type
{
  for (UIPress* const press in presses) {
    UIKey* const key = press.key;
    if (!key) {
      continue;
    }

    const PuglMods state = puglIosModifiers(key.modifierFlags);
    const PuglKeyEvent keyEvent = {
      type,
      0U,
      puglGetTime(puglview->world),
      0.0,
      0.0,
      0.0,
      0.0,
      state,
      (uint32_t)key.keyCode,
      puglIosKey(key),
    };

    PuglEvent event;
    memset(&event, 0, sizeof(event));
    event.key = keyEvent;
    puglDispatchEvent(puglview, &event);

    if (type == PUGL_KEY_PRESS) {
      puglIosDispatchText(self, key, state);
    }
  }
}

- (void)pressesBegan:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event
{
  (void)event;
  [self dispatchPresses:presses type:PUGL_KEY_PRESS];
}

- (void)pressesEnded:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event
{
  (void)event;
  [self dispatchPresses:presses type:PUGL_KEY_RELEASE];
}

@end

PuglWorldInternals*
puglInitWorldInternals(PuglWorldType type, PuglWorldFlags flags)
{
  (void)type;
  (void)flags;

  PuglWorldInternals* const impl =
    (PuglWorldInternals*)calloc(1U, sizeof(PuglWorldInternals));
  if (!impl) {
    return NULL;
  }

  impl->screen = [UIScreen mainScreen];
  if (mach_timebase_info(&impl->timebaseInfo)) {
    free(impl);
    return NULL;
  }

  return impl;
}

void
puglFreeWorldInternals(PuglWorld* world)
{
  free(world->impl);
}

void*
puglGetNativeWorld(PuglWorld* world)
{
  return world && world->impl ? world->impl->screen : NULL;
}

PuglInternals*
puglInitViewInternals(PuglWorld* world)
{
  (void)world;
  return (PuglInternals*)calloc(1U, sizeof(PuglInternals));
}

PuglStatus
puglApplySizeHint(PuglView* const view, const PuglSizeHint hint)
{
  (void)view;
  (void)hint;
  return PUGL_SUCCESS;
}

PuglPoint
puglGetAncestorCenter(const PuglView* const view)
{
  const CGFloat scale = puglIosScale(view);
  CGRect bounds = view->world->impl->screen.bounds;

  if (view->transientParent) {
    UIView* const parent = (UIView*)view->transientParent;
    bounds = parent.bounds;
  } else if (view->parent) {
    UIView* const parent = (UIView*)view->parent;
    bounds = parent.bounds;
  }

  const PuglPoint center = {
    (PuglCoord)lrint(CGRectGetMidX(bounds) * scale),
    (PuglCoord)lrint(CGRectGetMidY(bounds) * scale),
  };
  return center;
}

PuglStatus
puglRealize(PuglView* view)
{
  if (!view || !view->impl || view->impl->wrapperView) {
    return PUGL_FAILURE;
  }

  PuglStatus status = puglPreRealize(view);
  if (status) {
    return status;
  }

  puglEnsureHint(view, PUGL_RED_BITS, 8);
  puglEnsureHint(view, PUGL_GREEN_BITS, 8);
  puglEnsureHint(view, PUGL_BLUE_BITS, 8);
  puglEnsureHint(view, PUGL_ALPHA_BITS, 8);
  view->hints[PUGL_REFRESH_RATE] =
    (int)MAX(1, view->world->impl->screen.maximumFramesPerSecond);

  const PuglArea size = puglGetInitialSize(view);
  const PuglPoint position = puglGetInitialPosition(view, size);
  const CGRect frame =
    puglIosRectToPoints(view, position.x, position.y, size.width, size.height);

  PuglWrapperView* const wrapper =
    [[PuglWrapperView alloc] initWithFrame:frame];
  if (!wrapper) {
    return PUGL_NO_MEMORY;
  }

  wrapper->puglview = view;
  view->impl->wrapperView = wrapper;

  if ((status = view->backend->configure(view)) ||
      (status = view->backend->create(view))) {
    if (view->impl->drawView) {
      view->backend->destroy(view);
    }
    wrapper->puglview = NULL;
    [wrapper release];
    view->impl->wrapperView = nil;
    return status;
  }

  if (!view->impl->drawView) {
    view->backend->destroy(view);
    wrapper->puglview = NULL;
    [wrapper release];
    view->impl->wrapperView = nil;
    return PUGL_BACKEND_FAILED;
  }

  view->impl->drawView.frame = wrapper.bounds;
  view->impl->drawView.autoresizingMask =
    UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [wrapper addSubview:view->impl->drawView];

  if (view->parent) {
    UIView* const parent = (UIView*)view->parent;
    [parent addSubview:wrapper];
  } else {
    UIWindow* const window = [[UIWindow alloc] initWithFrame:frame];
    UIViewController* const controller = [[UIViewController alloc] init];
    if (!window || !controller) {
      [controller release];
      [window release];
      view->backend->destroy(view);
      wrapper->puglview = NULL;
      [wrapper release];
      view->impl->wrapperView = nil;
      return PUGL_NO_MEMORY;
    }

    controller.view = wrapper;
    window.rootViewController = controller;
    window.hidden = YES;
    view->impl->window = window;
    view->impl->viewController = controller;
  }

  status = puglDispatchSimpleEvent(view, PUGL_REALIZE);
  if (status) {
    return status;
  }

  // A realized view is initially hidden on every Pugl platform.  Publish that
  // state in the first configuration so puglGetVisible() remains authoritative
  // before the first puglShow().
  wrapper.hidden = YES;
  if (view->impl->window) {
    view->impl->window.hidden = YES;
  }

  status = [wrapper dispatchCurrentConfiguration];
  if (!status) {
    [view->impl->drawView setNeedsDisplay];
  }
  return status;
}

PuglStatus
puglUnrealize(PuglView* const view)
{
  if (!view || !view->impl || !view->impl->wrapperView) {
    return PUGL_FAILURE;
  }

  PuglInternals* const impl = view->impl;
  (void)[impl->wrapperView resignFirstResponder];

  PuglStatus status = puglDispatchSimpleEvent(view, PUGL_UNREALIZE);

  if (view->backend && impl->drawView) {
    view->backend->destroy(view);
  }

  [impl->wrapperView removeFromSuperview];
  impl->wrapperView->puglview = NULL;

  if (impl->window) {
    impl->window.hidden = YES;
    impl->window.rootViewController = nil;
  }
  if (impl->viewController) {
    impl->viewController.view = nil;
    [impl->viewController release];
    impl->viewController = nil;
  }
  if (impl->window) {
    [impl->window release];
    impl->window = nil;
  }

  [impl->wrapperView release];
  impl->wrapperView = nil;
  memset(&view->lastConfigure, 0, sizeof(PuglConfigureEvent));
  return status;
}

PuglStatus
puglShow(PuglView* view, const PuglShowCommand command)
{
  (void)command;

  PuglStatus status =
    (view && view->impl && view->impl->wrapperView) ? PUGL_SUCCESS
                                                    : puglRealize(view);
  if (status || !view->impl->wrapperView) {
    return status;
  }

  view->impl->wrapperView.hidden = NO;
  if (view->impl->window) {
    view->impl->window.hidden = NO;
    [view->impl->window makeKeyWindow];
  }

  status = [view->impl->wrapperView dispatchCurrentConfiguration];
  [view->impl->drawView setNeedsDisplay];
  return status;
}

PuglStatus
puglHide(PuglView* view)
{
  if (!view || !view->impl || !view->impl->wrapperView) {
    return PUGL_FAILURE;
  }

  view->impl->wrapperView.hidden = YES;
  if (view->impl->window) {
    view->impl->window.hidden = YES;
  }

  return [view->impl->wrapperView dispatchCurrentConfiguration];
}

void
puglFreeViewInternals(PuglView* view)
{
  if (!view || !view->impl) {
    return;
  }

  if (view->impl->wrapperView) {
    if (view->stage >= PUGL_VIEW_STAGE_REALIZED) {
      (void)puglUnrealize(view);
    } else {
      if (view->backend && view->impl->drawView) {
        view->backend->destroy(view);
      }
      view->impl->wrapperView->puglview = NULL;
      [view->impl->wrapperView removeFromSuperview];
      [view->impl->wrapperView release];
      view->impl->wrapperView = nil;
    }
  }

  [view->impl->clipboardData release];
  [view->impl->clipboardType release];
  free(view->impl);
}

PuglStatus
puglGrabFocus(PuglView* view)
{
  if (!view || !view->impl || !view->impl->wrapperView) {
    return PUGL_FAILURE;
  }

  return [view->impl->wrapperView becomeFirstResponder] ? PUGL_SUCCESS
                                                        : PUGL_FAILURE;
}

bool
puglHasFocus(const PuglView* view)
{
  return view && view->impl && view->impl->wrapperView &&
         view->impl->wrapperView.isFirstResponder;
}

PuglStatus
puglSetViewStyle(PuglView* const view, const PuglViewStyleFlags flags)
{
  if (!view || !view->impl || !view->impl->wrapperView) {
    return PUGL_FAILURE;
  }

  const PuglViewStyleFlags unsupported =
    flags & ~(PUGL_VIEW_STYLE_MAPPED | PUGL_VIEW_STYLE_HIDDEN);
  if (unsupported) {
    return PUGL_UNSUPPORTED;
  }

  return (flags & PUGL_VIEW_STYLE_HIDDEN) ? puglHide(view) : puglShow(view, PUGL_SHOW_PASSIVE);
}

PuglStatus
puglStartTimer(PuglView* view, const uintptr_t id, const double timeout)
{
  if (!view || !view->impl || !view->impl->wrapperView ||
      !isfinite(timeout) || timeout <= 0.0) {
    return PUGL_BAD_PARAMETER;
  }

  (void)puglStopTimer(view, id);

  NSNumber* const key = [NSNumber numberWithUnsignedLongLong:(unsigned long long)id];
  NSTimer* const timer = [NSTimer timerWithTimeInterval:timeout
                                                 target:view->impl->wrapperView
                                               selector:@selector(timerTick:)
                                               userInfo:key
                                                repeats:YES];
  [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
  view->impl->wrapperView->userTimers[key] = timer;
  return PUGL_SUCCESS;
}

PuglStatus
puglStopTimer(PuglView* view, const uintptr_t id)
{
  if (!view || !view->impl || !view->impl->wrapperView) {
    return PUGL_FAILURE;
  }

  NSNumber* const key = [NSNumber numberWithUnsignedLongLong:(unsigned long long)id];
  NSTimer* const timer = view->impl->wrapperView->userTimers[key];
  if (!timer) {
    return PUGL_UNKNOWN_ERROR;
  }

  [timer invalidate];
  [view->impl->wrapperView->userTimers removeObjectForKey:key];
  return PUGL_SUCCESS;
}

PuglStatus
puglSendEvent(PuglView* view, const PuglEvent* event)
{
  if (!view || !view->impl || !view->impl->wrapperView || !event ||
      view->world->state == PUGL_WORLD_EXPOSING) {
    return PUGL_FAILURE;
  }

  if (event->type != PUGL_CLIENT && event->type != PUGL_CLOSE) {
    return PUGL_UNSUPPORTED;
  }

  [view->impl->wrapperView queueEvent:event];
  return PUGL_SUCCESS;
}

PuglStatus
puglUpdate(PuglWorld* world, const double timeout)
{
  if (!world || world->state != PUGL_WORLD_IDLE) {
    return PUGL_BAD_CALL;
  }

  world->state = PUGL_WORLD_UPDATING;

  if (world->type == PUGL_PROGRAM && timeout != 0.0) {
    NSDate* const limit =
      timeout < 0.0 ? [NSDate distantFuture]
                    : [NSDate dateWithTimeIntervalSinceNow:timeout];
    [[NSRunLoop mainRunLoop] runMode:NSDefaultRunLoopMode beforeDate:limit];
  }

  for (size_t i = 0U; i < world->numViews; ++i) {
    PuglView* const view = world->views[i];
    if (!view || !view->impl || !view->impl->wrapperView ||
        view->stage < PUGL_VIEW_STAGE_REALIZED) {
      continue;
    }

    [view->impl->wrapperView drainPendingEvents];

    if (!(puglIosViewStyle(view) & PUGL_VIEW_STYLE_HIDDEN)) {
      puglDispatchSimpleEvent(view, PUGL_UPDATE);
    }
  }

  world->state = PUGL_WORLD_IDLE;
  return PUGL_SUCCESS;
}

double
puglGetTime(const PuglWorld* world)
{
  const struct mach_timebase_info base = world->impl->timebaseInfo;
  const double rate = (1.0E9 / base.numer) * base.denom;
  return (double)mach_absolute_time() / rate;
}

PuglStatus
puglObscureView(PuglView* view)
{
  if (!view || !view->impl || !view->impl->drawView) {
    return PUGL_FAILURE;
  }
  if (view->world->state == PUGL_WORLD_EXPOSING) {
    return PUGL_BAD_CALL;
  }

  [view->impl->drawView setNeedsDisplay];
  return PUGL_SUCCESS;
}

PuglStatus
puglObscureRegion(PuglView*      view,
                  const int      x,
                  const int      y,
                  const unsigned width,
                  const unsigned height)
{
  if (!view || !view->impl || !view->impl->drawView) {
    return PUGL_FAILURE;
  }
  if (view->world->state == PUGL_WORLD_EXPOSING) {
    return PUGL_BAD_CALL;
  }
  if (!puglIsValidPosition(x, y) || !puglIsValidSize(width, height)) {
    return PUGL_BAD_PARAMETER;
  }

  const CGRect rect = puglIosRectToPoints(view, x, y, width, height);
  [view->impl->drawView setNeedsDisplayInRect:rect];
  return PUGL_SUCCESS;
}

PuglNativeView
puglGetNativeView(const PuglView* view)
{
  return (view && view->impl)
           ? (PuglNativeView)(uintptr_t)view->impl->wrapperView
           : (PuglNativeView)0U;
}

PuglStatus
puglApplyViewString(PuglView* const view,
                    const PuglStringHint key,
                    const char* const value)
{
  (void)view;
  (void)value;

  switch (key) {
  case PUGL_WINDOW_TITLE:
    return PUGL_SUCCESS;
  case PUGL_APPLICATION_NAME:
  case PUGL_CLASS_NAME:
    return PUGL_UNSUPPORTED;
  }

  return PUGL_BAD_PARAMETER;
}

double
puglGetScaleFactor(const PuglView* const view)
{
  return puglIosScale(view);
}

PuglStatus
puglSetWindowPosition(PuglView* const view, const int x, const int y)
{
  if (!view || !view->impl || !view->impl->wrapperView) {
    return PUGL_FAILURE;
  }

  const CGFloat scale = puglIosScale(view);
  CGRect frame = view->impl->wrapperView.frame;
  frame.origin = CGPointMake((CGFloat)x / scale, (CGFloat)y / scale);
  view->impl->wrapperView.frame = frame;
  if (view->impl->window) {
    view->impl->window.frame = frame;
  }
  return [view->impl->wrapperView dispatchCurrentConfiguration];
}

PuglStatus
puglSetWindowSize(PuglView* const view,
                  const unsigned width,
                  const unsigned height)
{
  if (!view || !view->impl || !view->impl->wrapperView) {
    return PUGL_FAILURE;
  }
  if (!puglIsValidSize(width, height)) {
    return PUGL_BAD_PARAMETER;
  }

  const CGFloat scale = puglIosScale(view);
  CGRect frame = view->impl->wrapperView.frame;
  frame.size = CGSizeMake((CGFloat)width / scale, (CGFloat)height / scale);
  view->impl->wrapperView.frame = frame;
  if (view->impl->window) {
    CGRect windowFrame = view->impl->window.frame;
    windowFrame.size = frame.size;
    view->impl->window.frame = windowFrame;
  }

  [view->impl->wrapperView setNeedsLayout];
  [view->impl->wrapperView layoutIfNeeded];
  return [view->impl->wrapperView dispatchCurrentConfiguration];
}

PuglStatus
puglSetTransientParent(PuglView* view, const PuglNativeView parent)
{
  if (!view || view->parent) {
    return PUGL_FAILURE;
  }

  view->transientParent = parent;
  return PUGL_SUCCESS;
}

PuglStatus
puglRegisterDropType(PuglView* view, const char* type)
{
  (void)view;
  (void)type;
  return PUGL_UNSUPPORTED;
}

static bool
puglIosPrepareClipboard(PuglView* const view)
{
  UIPasteboard* const pasteboard = [UIPasteboard generalPasteboard];
  NSString* type = nil;

  if (pasteboard.string) {
    type = @"text/plain";
  } else if (pasteboard.types.count) {
    type = [pasteboard.types objectAtIndex:0U];
  }

  [view->impl->clipboardType release];
  view->impl->clipboardType = [type copy];
  return view->impl->clipboardType != nil;
}

PuglStatus
puglPaste(PuglView* const view)
{
  if (!view || !view->impl) {
    return PUGL_FAILURE;
  }

  // An empty clipboard is still a valid offer with zero advertised types.
  // This matches the desktop contract and lets the client decide how to handle
  // a paste request without treating normal empty state as a backend error.
  (void)puglIosPrepareClipboard(view);

  const PuglDataOfferEvent offer = {
    PUGL_DATA_OFFER,
    0U,
    puglGetTime(view->world),
    0.0,
    0.0,
    PUGL_CLIPBOARD_GENERAL,
  };
  PuglEvent event;
  memset(&event, 0, sizeof(event));
  event.offer = offer;
  return puglDispatchEvent(view, &event);
}

uint32_t
puglGetNumClipboardTypes(const PuglView* const view,
                         const PuglClipboard clipboard)
{
  if (!view || !view->impl || clipboard != PUGL_CLIPBOARD_GENERAL) {
    return 0U;
  }

  return view->impl->clipboardType ? 1U : 0U;
}

const char*
puglGetClipboardType(const PuglView* const view,
                     const PuglClipboard clipboard,
                     const uint32_t typeIndex)
{
  if (!view || !view->impl || clipboard != PUGL_CLIPBOARD_GENERAL ||
      typeIndex != 0U || !view->impl->clipboardType) {
    return NULL;
  }

  return [view->impl->clipboardType UTF8String];
}

PuglStatus
puglAcceptOffer(PuglView* const view,
                const PuglDataOfferEvent* const offer,
                const uint32_t typeIndex,
                const PuglDataAction action,
                const int regionX,
                const int regionY,
                const unsigned regionWidth,
                const unsigned regionHeight)
{
  (void)action;
  (void)regionWidth;
  (void)regionHeight;

  if (!view || !offer || offer->clipboard != PUGL_CLIPBOARD_GENERAL ||
      typeIndex != 0U || !view->impl->clipboardType) {
    return PUGL_BAD_PARAMETER;
  }

  const PuglDataEvent data = {
    PUGL_DATA,
    0U,
    puglGetTime(view->world),
    (double)regionX,
    (double)regionY,
    PUGL_CLIPBOARD_GENERAL,
    0U,
  };
  PuglEvent event;
  memset(&event, 0, sizeof(event));
  event.data = data;
  return puglDispatchEvent(view, &event);
}

PuglStatus
puglRejectOffer(PuglView* const view,
                const PuglDataOfferEvent* const offer,
                const int regionX,
                const int regionY,
                const unsigned regionWidth,
                const unsigned regionHeight)
{
  (void)view;
  (void)regionX;
  (void)regionY;
  (void)regionWidth;
  (void)regionHeight;

  return offer && offer->clipboard == PUGL_CLIPBOARD_GENERAL
           ? PUGL_SUCCESS
           : PUGL_BAD_PARAMETER;
}

const void*
puglGetClipboard(PuglView* const view,
                 const PuglClipboard clipboard,
                 const uint32_t typeIndex,
                 size_t* const len)
{
  if (!len) {
    return NULL;
  }
  *len = 0U;

  if (!view || !view->impl || clipboard != PUGL_CLIPBOARD_GENERAL ||
      typeIndex != 0U || !view->impl->clipboardType) {
    return NULL;
  }

  UIPasteboard* const pasteboard = [UIPasteboard generalPasteboard];
  NSData* data = nil;
  if ([view->impl->clipboardType isEqualToString:@"text/plain"]) {
    data = [pasteboard.string dataUsingEncoding:NSUTF8StringEncoding];
  } else {
    data = [pasteboard dataForPasteboardType:view->impl->clipboardType];
  }

  [view->impl->clipboardData release];
  view->impl->clipboardData = [data copy];
  if (!view->impl->clipboardData) {
    return NULL;
  }

  *len = view->impl->clipboardData.length;
  return view->impl->clipboardData.bytes;
}

PuglStatus
puglSetCursor(PuglView* view, const PuglCursor cursor)
{
  (void)view;
  return cursor == PUGL_CURSOR_ARROW ? PUGL_SUCCESS : PUGL_UNSUPPORTED;
}

PuglStatus
puglSetClipboard(PuglView* const view,
                 const PuglClipboard clipboard,
                 const char* const type,
                 const void* const data,
                 const size_t len)
{
  (void)view;

  if (clipboard != PUGL_CLIPBOARD_GENERAL || !type || (!data && len)) {
    return PUGL_BAD_PARAMETER;
  }

  UIPasteboard* const pasteboard = [UIPasteboard generalPasteboard];
  NSString* const pasteboardType = [NSString stringWithUTF8String:type];
  if (!pasteboardType) {
    return PUGL_BAD_PARAMETER;
  }

  if (!strcmp(type, "text/plain")) {
    NSString* const string =
      [[[NSString alloc] initWithBytes:data
                                length:len
                              encoding:NSUTF8StringEncoding] autorelease];
    if (!string) {
      return PUGL_BAD_PARAMETER;
    }

    pasteboard.string = string;
    return PUGL_SUCCESS;
  }

  NSData* const blob = [NSData dataWithBytes:data length:len];
  [pasteboard setData:blob forPasteboardType:pasteboardType];
  return PUGL_SUCCESS;
}
