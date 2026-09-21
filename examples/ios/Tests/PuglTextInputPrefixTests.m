// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include "ios.h"

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

@interface PuglTextInputPrefixTests : XCTestCase
@end

@implementation PuglTextInputPrefixTests

- (void)testRuntimeResponderTopologyUsesConsumerPrefix
{
  XCTAssertEqualObjects(NSStringFromClass([PuglWrapperView class]),
                        @"PuglIOSTestWrapperView");
  XCTAssertEqualObjects(NSStringFromClass([PuglTextInputView class]),
                        @"PuglIOSTestTextInputView");

  XCTAssertFalse([PuglWrapperView conformsToProtocol:@protocol(UIKeyInput)]);
  XCTAssertTrue([PuglTextInputView conformsToProtocol:@protocol(UIKeyInput)]);
}

@end
