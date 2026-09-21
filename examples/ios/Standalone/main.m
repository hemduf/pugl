// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: 0BSD OR ISC

#include "../Shared/PuglIOSExample.h"

#import <UIKit/UIKit.h>

@interface PuglStandaloneExampleDelegate : UIResponder <UIApplicationDelegate> {
@private
  PuglIOSExample _example;
  CADisplayLink* _displayLink;
}
@end

@implementation PuglStandaloneExampleDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
  (void)application;
  (void)launchOptions;

  if (!puglIOSExampleInit(&_example, PUGL_PROGRAM, nil)) {
    return NO;
  }

  _displayLink =
    [[CADisplayLink displayLinkWithTarget:self selector:@selector(step:)] retain];
  [_displayLink addToRunLoop:[NSRunLoop mainRunLoop]
                     forMode:NSRunLoopCommonModes];

  return YES;
}

- (void)step:(CADisplayLink*)displayLink
{
  (void)displayLink;
  puglIOSExampleUpdate(&_example);
}

- (void)tearDown
{
  if (_displayLink) {
    [_displayLink invalidate];
    [_displayLink release];
    _displayLink = nil;
  }

  puglIOSExampleDestroy(&_example);
}

- (void)applicationWillTerminate:(UIApplication*)application
{
  (void)application;
  [self tearDown];
}

- (void)dealloc
{
  [self tearDown];
  [super dealloc];
}

@end

int
main(int argc, char** argv)
{
  @autoreleasepool {
    return UIApplicationMain(
      argc, argv, nil, NSStringFromClass([PuglStandaloneExampleDelegate class]));
  }
}
