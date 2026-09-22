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

  // Before realization the wrapper has no window yet, but an embedded parent
  // may already belong to an external screen with a different scale.
  if (view && view->parent) {
    UIView* const parent = (UIView*)view->parent;
    if (parent.window.screen) {
      return parent.window.screen.scale;
    }
  }

  if (view && view->transientParent) {
    UIView* const parent = (UIView*)view->transientParent;
    if (parent.window.screen) {
      return parent.window.screen.scale;
    }
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
  case 0x39U: return PUGL_KEY_CAPS_LOCK;
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
  case 0x46U: return PUGL_KEY_PRINT_SCREEN;
  case 0x47U: return PUGL_KEY_SCROLL_LOCK;
  case 0x48U: return PUGL_KEY_PAUSE;
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
  case 0x53U: return PUGL_KEY_NUM_LOCK;
  case 0x65U: return PUGL_KEY_MENU;
  case 0xE0U: return PUGL_KEY_CTRL_L;
  case 0xE1U: return PUGL_KEY_SHIFT_L;
  case 0xE2U: return PUGL_KEY_ALT_L;
  case 0xE3U: return PUGL_KEY_SUPER_L;
  case 0xE4U: return PUGL_KEY_CTRL_R;
  case 0xE5U: return PUGL_KEY_SHIFT_R;
  case 0xE6U: return PUGL_KEY_ALT_R;
  case 0xE7U: return PUGL_KEY_SUPER_R;
  default: break;
  }

  return puglIosFirstScalar(key.charactersIgnoringModifiers);
}

static bool
puglIosTextScalarAllowed(const uint32_t scalar)
{
  return !((scalar < 0x20U && scalar != '\t' && scalar != '\n' &&
            scalar != '\r') ||
           scalar == 0x7FU);
}

static void
puglIosDispatchTextScalar(PuglView* const view,
                          const uint32_t  scalar,
                          const PuglMods  state,
                          const uint32_t  keycode)
{
  if (!view || !puglIosTextScalarAllowed(scalar)) {
    return;
  }

  PuglTextEvent textEvent = {
    PUGL_TEXT,
    0U,
    puglGetTime(view->world),
    0.0,
    0.0,
    0.0,
    0.0,
    state,
    keycode,
    scalar,
    {0},
  };
  puglIosEncodeUtf8(scalar, textEvent.string);

  PuglEvent event;
  memset(&event, 0, sizeof(event));
  event.text = textEvent;
  puglDispatchEvent(view, &event);
}

static void
puglIosDispatchHardwareKey(PuglView* const      view,
                           UIKey* const          key,
                           const PuglEventType   type)
{
  if (!view || !key) {
    return;
  }

  const PuglMods state = puglIosModifiers(key.modifierFlags);
  const PuglKeyEvent keyEvent = {
    type,
    0U,
    puglGetTime(view->world),
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
  puglDispatchEvent(view, &event);
}

static void
puglIosDispatchHardwareText(PuglWrapperView* const wrapper,
                            UIKey* const            key,
                            const PuglMods          state)
{
  if (!wrapper || !wrapper->puglview || !key || !key.characters.length ||
      (state & (PUGL_MOD_CTRL | PUGL_MOD_SUPER))) {
    return;
  }

  NSString* const text = key.characters;
  for (NSUInteger i = 0U; i < text.length;) {
    PuglView* const view = wrapper->puglview;
    if (!view || !wrapper.isFirstResponder || puglIsTextInputActive(view)) {
      break;
    }

    const unichar first = [text characterAtIndex:i++];
    uint32_t scalar = first;

    if (first >= 0xD800U && first <= 0xDBFFU) {
      if (i < text.length) {
        const unichar second = [text characterAtIndex:i];
        if (second >= 0xDC00U && second <= 0xDFFFU) {
          ++i;
          scalar = 0x10000U + (((uint32_t)first - 0xD800U) << 10U) +
                   ((uint32_t)second - 0xDC00U);
        } else {
          scalar = 0xFFFDU;
        }
      } else {
        scalar = 0xFFFDU;
      }
    } else if (first >= 0xDC00U && first <= 0xDFFFU) {
      scalar = 0xFFFDU;
    }

    puglIosDispatchTextScalar(view, scalar, state, (uint32_t)key.keyCode);
  }
}

static bool
puglIosHasLogicalFocus(const PuglView* const view)
{
  return view && view->impl &&
         ((view->impl->wrapperView &&
           view->impl->wrapperView.isFirstResponder) ||
          (view->impl->textInputView &&
           view->impl->textInputView.isFirstResponder));
}

static void
puglIosDispatchFocusDelta(PuglView* const view, const bool before)
{
  if (!view || !view->impl || view->impl->responderTransfer ||
      view->stage < PUGL_VIEW_STAGE_REALIZED) {
    return;
  }

  const bool after = puglIosHasLogicalFocus(view);
  if (before == after) {
    return;
  }

  const PuglFocusEvent focus = {
    after ? PUGL_FOCUS_IN : PUGL_FOCUS_OUT,
    0U,
    PUGL_CROSSING_NORMAL,
  };
  PuglEvent event;
  memset(&event, 0, sizeof(event));
  event.focus = focus;
  puglDispatchEvent(view, &event);
}

static void
puglIosInvalidateTimers(PuglWrapperView* const wrapper)
{
  if (!wrapper || !wrapper->userTimers) {
    return;
  }

  for (NSTimer* const timer in [wrapper->userTimers allValues]) {
    [timer invalidate];
  }
  [wrapper->userTimers removeAllObjects];
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
    activeTouches = [[NSMutableDictionary alloc] init];
    nextPointerId = 1U;
  }
  return self;
}

- (void)dealloc
{
  puglIosInvalidateTimers(self);

  [activeTouches release];
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
  PuglWrapperView* const protectedSelf = [self retain];
  PuglView* const view = protectedSelf->puglview;
  const bool before = puglIosHasLogicalFocus(view);
  const BOOL changed = [super becomeFirstResponder];
  if (changed && protectedSelf->puglview == view) {
    puglIosDispatchFocusDelta(view, before);
  }
  [protectedSelf release];
  return changed;
}

- (BOOL)resignFirstResponder
{
  PuglWrapperView* const protectedSelf = [self retain];
  PuglView* const view = protectedSelf->puglview;
  const bool before = puglIosHasLogicalFocus(view);
  const BOOL changed = [super resignFirstResponder];
  if (changed && protectedSelf->puglview == view) {
    puglIosDispatchFocusDelta(view, before);
  }
  [protectedSelf release];
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

- (PuglStatus)queueEvent:(const PuglEvent*)event
{
  if (!event) {
    return PUGL_BAD_PARAMETER;
  }

  NSData* const data =
    [[NSData alloc] initWithBytes:event length:sizeof(PuglEvent)];
  if (!data) {
    return PUGL_NO_MEMORY;
  }

  [pendingEventLock lock];
  [pendingEvents addObject:data];
  [pendingEventLock unlock];
  [data release];

  CFRunLoopWakeUp(CFRunLoopGetMain());
  return PUGL_SUCCESS;
}

- (void)drainPendingEvents
{
  // A client event handler is allowed to tear down application state.  Keep
  // the native wrapper alive until this drain finishes and stop dispatching as
  // soon as the Pugl view has been detached.
  PuglWrapperView* const protectedSelf = [self retain];

  [pendingEventLock lock];
  NSArray* const events = [pendingEvents copy];
  [pendingEvents removeAllObjects];
  [pendingEventLock unlock];

  for (NSData* const data in events) {
    PuglView* const view = protectedSelf->puglview;
    if (!view) {
      break;
    }

    if (data.length == sizeof(PuglEvent)) {
      PuglEvent event;
      memcpy(&event, data.bytes, sizeof(event));
      puglDispatchEvent(view, &event);
    }
  }

  [events release];
  [protectedSelf release];
}

- (void)timerTick:(NSTimer*)timer
{
  if (!puglview) {
    [timer invalidate];
    return;
  }

  const uintptr_t timerId =
    (uintptr_t)[(NSNumber*)timer.userInfo unsignedLongLongValue];
  const PuglTimerEvent timerEvent = {PUGL_TIMER, 0U, timerId};

  PuglEvent event;
  memset(&event, 0, sizeof(event));
  event.timer = timerEvent;
  puglDispatchEvent(puglview, &event);
}

static PuglPointerType
puglIosPointerType(UITouch* const touch)
{
  switch (touch.type) {
  case UITouchTypeDirect:
    return PUGL_POINTER_TOUCH;
  case UITouchTypePencil:
    return PUGL_POINTER_PEN;
  case UITouchTypeIndirect:
  case UITouchTypeIndirectPointer:
    return PUGL_POINTER_MOUSE;
  }

  return PUGL_POINTER_UNKNOWN;
}

static double
puglIosPointerPressure(UITouch* const touch)
{
  const CGFloat maximum = touch.maximumPossibleForce;
  if (maximum <= 0.0) {
    return NAN;
  }

  return fmax(0.0, fmin(1.0, (double)(touch.force / maximum)));
}

static bool
puglIosIsMouseTouch(UITouch* const touch)
{
  return touch.type == UITouchTypeIndirect ||
         touch.type == UITouchTypeIndirectPointer;
}

- (void)dispatchLegacyPointerType:(PuglEventType)type touch:(UITouch*)touch
{
  if (!puglview || !touch) {
    return;
  }

  const CGFloat scale = puglIosScale(puglview);
  const CGPoint local = [touch locationInView:self];
  const CGPoint root = [touch locationInView:nil];

  PuglEvent event;
  memset(&event, 0, sizeof(event));

  if (type == PUGL_BUTTON_PRESS || type == PUGL_BUTTON_RELEASE) {
    event.button = (PuglButtonEvent){
      type,
      0U,
      touch.timestamp,
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
      touch.timestamp,
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
      touch.timestamp,
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

- (PuglPointerId)pointerIdForTouch:(UITouch*)touch create:(BOOL)create
{
  if (!touch) {
    return 0U;
  }

  NSValue* const key = [NSValue valueWithNonretainedObject:touch];
  NSNumber* const existing = [activeTouches objectForKey:key];
  if (existing) {
    return (PuglPointerId)existing.unsignedIntValue;
  }

  if (!create) {
    return 0U;
  }

  PuglPointerId id = 0U;
  NSArray* const usedIds = [activeTouches allValues];
  do {
    id = nextPointerId++;
    if (!nextPointerId) {
      nextPointerId = 1U;
    }
  } while (!id ||
           [usedIds containsObject:[NSNumber numberWithUnsignedInt:id]]);

  [activeTouches setObject:[NSNumber numberWithUnsignedInt:id] forKey:key];
  if (!primaryPointerId && activeTouches.count == 1U) {
    primaryPointerId = id;
  }

  return id;
}

- (void)dispatchPointerType:(PuglEventType)type
                      touch:(UITouch*)touch
                  pointerId:(PuglPointerId)pointerId
                sampleFlags:(PuglPointerFlags)sampleFlags
{
  if (!puglview || !touch || !pointerId) {
    return;
  }

  const CGFloat scale = puglIosScale(puglview);
  const CGPoint local = [touch locationInView:self];
  const CGFloat radius = touch.majorRadius;
  const double contactSize =
    radius > 0.0 ? (double)(2.0 * radius * scale) : NAN;

  PuglPointerFlags pointerFlags = sampleFlags;
  if (pointerId == primaryPointerId) {
    pointerFlags |= PUGL_POINTER_IS_PRIMARY;
  }

  const PuglPointerEvent pointerEvent = {
    type,
    0U,
    touch.timestamp,
    local.x * scale,
    local.y * scale,
    0U,
    pointerId,
    puglIosPointerType(touch),
    pointerFlags,
    puglIosPointerPressure(touch),
    contactSize,
    contactSize,
  };

  PuglEvent event;
  memset(&event, 0, sizeof(event));
  event.pointer = pointerEvent;
  puglDispatchEvent(puglview, &event);
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
  (void)event;
  PuglWrapperView* const protectedSelf = [self retain];

  for (UITouch* const touch in touches) {
    if (!protectedSelf->puglview) {
      break;
    }

    if (puglIosIsMouseTouch(touch)) {
      [protectedSelf dispatchLegacyPointerType:PUGL_POINTER_IN touch:touch];
      if (protectedSelf->puglview) {
        [protectedSelf dispatchLegacyPointerType:PUGL_BUTTON_PRESS touch:touch];
      }
      continue;
    }

    const PuglPointerId pointerId =
      [protectedSelf pointerIdForTouch:touch create:YES];
    [protectedSelf dispatchPointerType:PUGL_POINTER_DOWN
                                 touch:touch
                             pointerId:pointerId
                           sampleFlags:0U];
  }

  [protectedSelf release];
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
  PuglWrapperView* const protectedSelf = [self retain];

  for (UITouch* const touch in touches) {
    if (!protectedSelf->puglview) {
      break;
    }

    if (puglIosIsMouseTouch(touch)) {
      [protectedSelf dispatchLegacyPointerType:PUGL_MOTION touch:touch];
      continue;
    }

    const PuglPointerId pointerId =
      [protectedSelf pointerIdForTouch:touch create:NO];
    if (!pointerId) {
      continue;
    }

    NSArray<UITouch*>* const samples = [event coalescedTouchesForTouch:touch];
    if (!samples.count) {
      [protectedSelf dispatchPointerType:PUGL_POINTER_MOVE
                                   touch:touch
                               pointerId:pointerId
                             sampleFlags:0U];
      continue;
    }

    for (NSUInteger i = 0U; i < samples.count; ++i) {
      if (!protectedSelf->puglview) {
        break;
      }

      const PuglPointerFlags flags =
        i + 1U < samples.count ? PUGL_POINTER_IS_COALESCED : 0U;
      [protectedSelf dispatchPointerType:PUGL_POINTER_MOVE
                                   touch:[samples objectAtIndex:i]
                               pointerId:pointerId
                             sampleFlags:flags];
    }
  }

  [protectedSelf release];
}

- (void)finishTouches:(NSSet<UITouch*>*)touches type:(PuglEventType)type
{
  PuglWrapperView* const protectedSelf = [self retain];

  for (UITouch* const touch in touches) {
    if (!protectedSelf->puglview) {
      break;
    }

    if (puglIosIsMouseTouch(touch)) {
      [protectedSelf dispatchLegacyPointerType:PUGL_BUTTON_RELEASE touch:touch];
      if (protectedSelf->puglview) {
        [protectedSelf dispatchLegacyPointerType:PUGL_POINTER_OUT touch:touch];
      }
      continue;
    }

    const PuglPointerId pointerId =
      [protectedSelf pointerIdForTouch:touch create:NO];
    if (!pointerId) {
      continue;
    }

    [protectedSelf dispatchPointerType:type
                                 touch:touch
                             pointerId:pointerId
                           sampleFlags:0U];

    NSValue* const key = [NSValue valueWithNonretainedObject:touch];
    [protectedSelf->activeTouches removeObjectForKey:key];
    if (protectedSelf->primaryPointerId == pointerId) {
      protectedSelf->primaryPointerId = 0U;
    }
  }

  [protectedSelf release];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
  (void)event;
  [self finishTouches:touches type:PUGL_POINTER_UP];
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
  (void)event;
  [self finishTouches:touches type:PUGL_POINTER_CANCEL];
}

- (void)dispatchPresses:(NSSet<UIPress*>*)presses type:(PuglEventType)type
{
  PuglWrapperView* const protectedSelf = [self retain];

  for (UIPress* const press in presses) {
    PuglView* const view = protectedSelf->puglview;
    if (!view) {
      break;
    }

    UIKey* const key = press.key;
    if (!key) {
      continue;
    }

    const PuglMods state = puglIosModifiers(key.modifierFlags);
    puglIosDispatchHardwareKey(view, key, type);

    if (type == PUGL_KEY_PRESS && protectedSelf->puglview == view &&
        protectedSelf.isFirstResponder && !puglIsTextInputActive(view)) {
      puglIosDispatchHardwareText(protectedSelf, key, state);
    }
  }

  [protectedSelf release];
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

@implementation PuglTextInputView

- (BOOL)canBecomeFirstResponder
{
  return YES;
}

- (BOOL)becomeFirstResponder
{
  PuglTextInputView* const protectedSelf = [self retain];
  PuglView* const view = protectedSelf->puglview;
  const bool before = puglIosHasLogicalFocus(view);
  const BOOL changed = [super becomeFirstResponder];
  if (changed && protectedSelf->puglview == view) {
    puglIosDispatchFocusDelta(view, before);
  }
  [protectedSelf release];
  return changed;
}

- (BOOL)resignFirstResponder
{
  PuglTextInputView* const protectedSelf = [self retain];
  PuglView* const view = protectedSelf->puglview;
  const bool before = puglIosHasLogicalFocus(view);
  const BOOL changed = [super resignFirstResponder];
  if (changed && protectedSelf->puglview == view) {
    puglIosDispatchFocusDelta(view, before);
  }
  [protectedSelf release];
  return changed;
}

- (BOOL)hasText
{
  return puglview &&
         (puglview->textInputFlags & PUGL_TEXT_INPUT_HAS_TEXT) != 0U;
}

- (void)insertText:(NSString*)text
{
  PuglTextInputView* const protectedSelf = [self retain];

  for (NSUInteger i = 0U; text && i < text.length;) {
    PuglView* const view = protectedSelf->puglview;
    if (!view || view->stage < PUGL_VIEW_STAGE_REALIZED ||
        !protectedSelf.isFirstResponder) {
      break;
    }

    const unichar first = [text characterAtIndex:i++];
    uint32_t scalar = first;
    if (first >= 0xD800U && first <= 0xDBFFU) {
      if (i < text.length) {
        const unichar second = [text characterAtIndex:i];
        if (second >= 0xDC00U && second <= 0xDFFFU) {
          ++i;
          scalar = 0x10000U + (((uint32_t)first - 0xD800U) << 10U) +
                   ((uint32_t)second - 0xDC00U);
        } else {
          scalar = 0xFFFDU;
        }
      } else {
        scalar = 0xFFFDU;
      }
    } else if (first >= 0xDC00U && first <= 0xDFFFU) {
      scalar = 0xFFFDU;
    }

    puglIosDispatchTextScalar(view, scalar, 0U, 0U);
  }

  [protectedSelf release];
}

- (void)deleteBackward
{
  PuglTextInputView* const protectedSelf = [self retain];
  PuglView* const view = protectedSelf->puglview;

  if (view && view->stage >= PUGL_VIEW_STAGE_REALIZED) {
    PuglEvent event;
    memset(&event, 0, sizeof(event));
    event.textEdit = (PuglTextEditEvent){
      PUGL_TEXT_EDIT,
      0U,
      puglGetTime(view->world),
      PUGL_TEXT_DELETE_BACKWARD,
    };
    puglDispatchEvent(view, &event);
  }

  [protectedSelf release];
}

- (void)dispatchPresses:(NSSet<UIPress*>*)presses type:(PuglEventType)type
{
  PuglTextInputView* const protectedSelf = [self retain];

  for (UIPress* const press in presses) {
    PuglView* const view = protectedSelf->puglview;
    if (!view) {
      break;
    }

    UIKey* const key = press.key;
    if (key) {
      puglIosDispatchHardwareKey(view, key, type);
    }
  }

  [protectedSelf release];
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

  // Modules are embedded guests and must never create or take ownership of a
  // process-level UIWindow in a plug-in host.
  if (view->world->type == PUGL_MODULE && !view->parent) {
    return PUGL_BAD_CONFIGURATION;
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

  // A realized Pugl view starts hidden.  Set this before attaching to an
  // application-owned parent so an embedded view can never flash visible
  // before its first explicit puglShow().
  wrapper.hidden = YES;
  wrapper->puglview = view;
  view->impl->wrapperView = wrapper;

  PuglTextInputView* const textInputView =
    [[PuglTextInputView alloc] initWithFrame:CGRectZero];
  if (!textInputView) {
    wrapper->puglview = NULL;
    [wrapper release];
    view->impl->wrapperView = nil;
    return PUGL_NO_MEMORY;
  }

  textInputView->puglview = view;
  textInputView.backgroundColor = [UIColor clearColor];
  view->impl->textInputView = textInputView;
  [wrapper addSubview:textInputView];

  if ((status = view->backend->configure(view)) ||
      (status = view->backend->create(view))) {
    if (view->impl->drawView) {
      view->backend->destroy(view);
    }
    textInputView->puglview = NULL;
    [textInputView removeFromSuperview];
    [textInputView release];
    view->impl->textInputView = nil;
    wrapper->puglview = NULL;
    [wrapper release];
    view->impl->wrapperView = nil;
    return status;
  }

  if (!view->impl->drawView) {
    view->backend->destroy(view);
    textInputView->puglview = NULL;
    [textInputView removeFromSuperview];
    [textInputView release];
    view->impl->textInputView = nil;
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
      textInputView->puglview = NULL;
      [textInputView removeFromSuperview];
      [textInputView release];
      view->impl->textInputView = nil;
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

  status = [wrapper dispatchCurrentConfiguration];
  if (!status) {
    [view->impl->drawView setNeedsDisplay];
  }
  return status;
}

static void
puglIosReleaseViewResources(PuglView* const view)
{
  if (!view || !view->impl) {
    return;
  }

  PuglInternals* const impl = view->impl;
  PuglWrapperView* const wrapper = impl->wrapperView;
  PuglTextInputView* const textInputView = impl->textInputView;

  impl->responderTransfer = false;

  if (wrapper) {
    puglIosInvalidateTimers(wrapper);
  }

  // Disable all future Pugl callbacks before native responder teardown.
  if (textInputView) {
    textInputView->puglview = NULL;
  }
  if (wrapper) {
    wrapper->puglview = NULL;
  }

  if (textInputView) {
    (void)[textInputView resignFirstResponder];
  }
  if (wrapper) {
    (void)[wrapper resignFirstResponder];
  }

  if (view->backend && impl->drawView) {
    view->backend->destroy(view);
  }

  if (textInputView) {
    [textInputView removeFromSuperview];
    [textInputView release];
    impl->textInputView = nil;
  }

  if (wrapper) {
    [wrapper removeFromSuperview];
  }

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

  if (wrapper) {
    [wrapper release];
    impl->wrapperView = nil;
  }

  memset(&view->lastConfigure, 0, sizeof(PuglConfigureEvent));
}

PuglStatus
puglUnrealize(PuglView* const view)
{
  if (!view || !view->impl || !view->impl->wrapperView) {
    return PUGL_FAILURE;
  }

  PuglWrapperView* const protectedWrapper = [view->impl->wrapperView retain];
  const PuglStatus textStatus = puglStopTextInput(view);

  // Ending the text session can synchronously publish a focus-out on failure.
  // The client may destroy or unrealize this view from that callback.
  if (protectedWrapper->puglview != view) {
    [protectedWrapper release];
    return textStatus;
  }

  const PuglStatus status = puglDispatchSimpleEvent(view, PUGL_UNREALIZE);

  // The unrealize callback itself may also destroy the view.  The retained
  // wrapper is a safe lifetime token whose back-pointer is cleared by teardown.
  if (protectedWrapper->puglview == view) {
    puglIosReleaseViewResources(view);
  }

  [protectedWrapper release];
  return status ? status : textStatus;
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

  PuglWrapperView* const protectedWrapper = [view->impl->wrapperView retain];
  const PuglStatus textStatus = puglStopTextInput(view);

  // A focus callback from puglStopTextInput() may synchronously destroy the
  // view.  Do not dereference the PuglView after teardown has cleared this
  // retained wrapper's non-owning back-pointer.
  if (protectedWrapper->puglview != view) {
    [protectedWrapper release];
    return textStatus;
  }

  protectedWrapper.hidden = YES;
  if (view->impl->window) {
    view->impl->window.hidden = YES;
  }

  const PuglStatus status = [protectedWrapper dispatchCurrentConfiguration];
  [protectedWrapper release];
  return status ? status : textStatus;
}

void
puglFreeViewInternals(PuglView* view)
{
  if (!view || !view->impl) {
    return;
  }

  // puglFreeView() is destructor-driven teardown.  Keep it callback-silent:
  // common state such as view strings has already begun destruction before
  // this platform hook runs.
  puglIosReleaseViewResources(view);

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

  if (puglIosHasLogicalFocus(view)) {
    return PUGL_SUCCESS;
  }

  return [view->impl->wrapperView becomeFirstResponder] ? PUGL_SUCCESS
                                                        : PUGL_FAILURE;
}

bool
puglHasFocus(const PuglView* view)
{
  return puglIosHasLogicalFocus(view);
}

PuglStatus
puglStartTextInput(PuglView* view)
{
  if (!view || !view->impl) {
    return PUGL_BAD_PARAMETER;
  }

  PuglInternals* const impl = view->impl;
  PuglWrapperView* const wrapper = impl->wrapperView;
  PuglTextInputView* const textInputView = impl->textInputView;
  if (!wrapper || !textInputView || view->stage < PUGL_VIEW_STAGE_REALIZED ||
      wrapper.hidden || !wrapper.window || !puglGetVisible(view) ||
      !puglIosHasLogicalFocus(view)) {
    return PUGL_BAD_CALL;
  }

  if (textInputView.isFirstResponder) {
    return PUGL_SUCCESS;
  }

  if (impl->responderTransfer) {
    return PUGL_BAD_CALL;
  }

  PuglWrapperView* const protectedWrapper = [wrapper retain];
  PuglTextInputView* const protectedText = [textInputView retain];
  const bool before = puglIosHasLogicalFocus(view);
  const bool wrapperWasFirst = wrapper.isFirstResponder;

  impl->responderTransfer = true;
  (void)[protectedText becomeFirstResponder];

  if (protectedText->puglview != view || protectedWrapper->puglview != view) {
    [protectedText release];
    [protectedWrapper release];
    return PUGL_FAILURE;
  }

  if (!protectedText.isFirstResponder && wrapperWasFirst &&
      !protectedWrapper.isFirstResponder) {
    (void)[protectedWrapper becomeFirstResponder];
    if (protectedText->puglview != view || protectedWrapper->puglview != view) {
      [protectedText release];
      [protectedWrapper release];
      return PUGL_FAILURE;
    }
  }

  if (protectedText.isFirstResponder &&
      (protectedWrapper.hidden || !protectedWrapper.window ||
       !puglGetVisible(view))) {
    (void)[protectedText resignFirstResponder];
    if (protectedText->puglview != view || protectedWrapper->puglview != view) {
      [protectedText release];
      [protectedWrapper release];
      return PUGL_FAILURE;
    }
  }

  const bool active = protectedText.isFirstResponder;
  view->impl->responderTransfer = false;
  puglIosDispatchFocusDelta(view, before);

  [protectedText release];
  [protectedWrapper release];
  return active ? PUGL_SUCCESS : PUGL_FAILURE;
}

PuglStatus
puglStopTextInput(PuglView* view)
{
  if (!view || !view->impl) {
    return PUGL_BAD_PARAMETER;
  }

  PuglInternals* const impl = view->impl;
  PuglWrapperView* const wrapper = impl->wrapperView;
  PuglTextInputView* const textInputView = impl->textInputView;
  if (!textInputView || !textInputView.isFirstResponder) {
    return PUGL_SUCCESS;
  }

  if (!wrapper || impl->responderTransfer) {
    return PUGL_BAD_CALL;
  }

  PuglWrapperView* const protectedWrapper = [wrapper retain];
  PuglTextInputView* const protectedText = [textInputView retain];
  const bool before = puglIosHasLogicalFocus(view);

  impl->responderTransfer = true;
  (void)[protectedWrapper becomeFirstResponder];

  if (protectedText->puglview != view || protectedWrapper->puglview != view) {
    [protectedText release];
    [protectedWrapper release];
    return PUGL_FAILURE;
  }

  if (!protectedWrapper.isFirstResponder && protectedText.isFirstResponder) {
    (void)[protectedText resignFirstResponder];
    if (protectedText->puglview != view || protectedWrapper->puglview != view) {
      [protectedText release];
      [protectedWrapper release];
      return PUGL_FAILURE;
    }

    if (!protectedText.isFirstResponder) {
      (void)[protectedWrapper becomeFirstResponder];
      if (protectedText->puglview != view ||
          protectedWrapper->puglview != view) {
        [protectedText release];
        [protectedWrapper release];
        return PUGL_FAILURE;
      }
    }
  }

  const bool active = protectedText.isFirstResponder;
  view->impl->responderTransfer = false;
  puglIosDispatchFocusDelta(view, before);

  [protectedText release];
  [protectedWrapper release];
  return active ? PUGL_FAILURE : PUGL_SUCCESS;
}

bool
puglIsTextInputActive(const PuglView* view)
{
  return view && view->impl && view->impl->textInputView &&
         view->impl->textInputView.isFirstResponder;
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

  return [view->impl->wrapperView queueEvent:event];
}

PuglStatus
puglUpdate(PuglWorld* world, const double timeout)
{
  @autoreleasepool {
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

    for (size_t i = 0U; i < world->numViews;) {
      PuglView* const view = world->views[i];
      if (!view || !view->impl || !view->impl->wrapperView ||
          view->stage < PUGL_VIEW_STAGE_REALIZED) {
        ++i;
        continue;
      }

      PuglWrapperView* const wrapper = [view->impl->wrapperView retain];
      [wrapper drainPendingEvents];

      // A queued client event may have destroyed this view.  The retained
      // wrapper survives long enough to tell us whether its PuglView is still
      // attached, without dereferencing a potentially freed PuglView.
      if (wrapper->puglview == view &&
          !(puglIosViewStyle(view) & PUGL_VIEW_STYLE_HIDDEN)) {
        puglDispatchSimpleEvent(view, PUGL_UPDATE);
      }

      const bool sameViewAtIndex =
        i < world->numViews && world->views[i] == view;
      [wrapper release];

      // If the callback removed this view, the next view has shifted into the
      // current slot and must not be skipped.
      if (sameViewAtIndex) {
        ++i;
      }
    }

    world->state = PUGL_WORLD_IDLE;
    return PUGL_SUCCESS;
  }
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
  } else if (pasteboard.items.count) {
    NSDictionary* const firstItem = [pasteboard.items objectAtIndex:0U];
    NSArray* const types = [firstItem allKeys];
    if (types.count) {
      type = [types objectAtIndex:0U];
    }
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
