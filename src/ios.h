// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#ifndef PUGL_SRC_IOS_H
#define PUGL_SRC_IOS_H

#include <pugl/pugl.h>

#import <UIKit/UIKit.h>

#include <mach/mach_time.h>

#include <stdint.h>

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
- (void)queueEvent:(const PuglEvent*)event;
- (void)drainPendingEvents;

@end

struct PuglWorldInternalsImpl {
  UIScreen*                 screen;
  struct mach_timebase_info timebaseInfo;
};

struct PuglInternalsImpl {
  PuglWrapperView* wrapperView;
  UIView*          drawView;
  UIWindow*        window;
  UIViewController* viewController;
  NSData*          clipboardData;
  NSString*        clipboardType;
};

#endif // PUGL_SRC_IOS_H
