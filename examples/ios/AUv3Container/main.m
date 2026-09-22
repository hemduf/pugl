// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: 0BSD OR ISC

#import <UIKit/UIKit.h>

@interface PuglAUv3ContainerDelegate : UIResponder <UIApplicationDelegate> {
@private
  UIWindow* _window;
}
@end

@implementation PuglAUv3ContainerDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
  (void)application;
  (void)launchOptions;

  _window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
  if (!_window) {
    return NO;
  }

  UIViewController* const controller = [[UIViewController alloc] init];
  controller.view.backgroundColor = [UIColor systemBackgroundColor];

  UILabel* const label = [[UILabel alloc] initWithFrame:controller.view.bounds];
  label.autoresizingMask =
    UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  label.numberOfLines = 0;
  label.textAlignment = NSTextAlignmentCenter;
  label.textColor = [UIColor labelColor];
  label.text =
    @"Pugl AUv3 Example\n\n"
     "This container installs the AUv3 extension.\n"
     "Open an AUv3 host and instantiate “Pugl iOS Example”.";
  [controller.view addSubview:label];

  _window.rootViewController = controller;
  [_window makeKeyAndVisible];

  [label release];
  [controller release];

  return YES;
}

- (void)dealloc
{
  [_window release];
  [super dealloc];
}

@end

int
main(int argc, char** argv)
{
  @autoreleasepool {
    return UIApplicationMain(
      argc, argv, nil, NSStringFromClass([PuglAUv3ContainerDelegate class]));
  }
}
