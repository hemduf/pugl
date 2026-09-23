// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#ifndef PUGL_SRC_IOS_H
#define PUGL_SRC_IOS_H

#include <pugl/pugl.h>

#import <UIKit/UIKit.h>

#include <mach/mach_time.h>

#include <stdint.h>

/*
   Objective-C class names are process-global, even when Pugl is statically
   linked into a plug-in bundle.  Consumers that may coexist with another
   embedded copy of Pugl must define PUGL_OBJC_CLASS_PREFIX to a consumer-unique
   identifier (for example, MyPluginPugl) to avoid runtime class collisions.
*/
#ifndef PUGL_OBJC_CLASS_PREFIX
#  error "PUGL_OBJC_CLASS_PREFIX must be a consumer-unique Objective-C class prefix"
#endif

#define PUGL_OBJC_JOIN_INNER(a, b) a##b
#define PUGL_OBJC_JOIN(a, b) PUGL_OBJC_JOIN_INNER(a, b)
#define PuglWrapperView PUGL_OBJC_JOIN(PUGL_OBJC_CLASS_PREFIX, WrapperView)
#define PuglTextInputView PUGL_OBJC_JOIN(PUGL_OBJC_CLASS_PREFIX, TextInputView)
#define PuglOpenGLView PUGL_OBJC_JOIN(PUGL_OBJC_CLASS_PREFIX, OpenGLView)
#define PuglStubView PUGL_OBJC_JOIN(PUGL_OBJC_CLASS_PREFIX, StubView)

@interface PuglWrapperView : UIView {
@public
  PuglView*             puglview;
  NSMutableDictionary*  userTimers;
  NSMutableArray*       pendingEvents;
  NSLock*               pendingEventLock;
  NSMutableDictionary*  activeTouches;
  PuglPointerId         nextPointerId;
  PuglPointerId         primaryPointerId;
}

- (void)dispatchExpose:(CGRect)rect;
- (PuglStatus)dispatchCurrentConfiguration;
- (PuglStatus)queueEvent:(const PuglEvent*)event;
- (void)drainPendingEvents;

@end

@interface PuglTextInputView : UIView <UIKeyInput> {
@public
  PuglView* puglview;
}
@end

struct PuglWorldInternalsImpl {
  UIScreen*                 screen;
  struct mach_timebase_info timebaseInfo;
};

struct PuglInternalsImpl {
  PuglWrapperView*   wrapperView;
  PuglTextInputView* textInputView;
  UIView*            drawView;
  UIWindow*        window;
  UIViewController* viewController;
  NSData*          clipboardData;
  NSString*        clipboardType;
  bool             responderTransfer;
};

#endif // PUGL_SRC_IOS_H
