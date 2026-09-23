// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include "../../../subprojects/pugl-cpp/include/pugl/pugl.hpp"

#import <XCTest/XCTest.h>

#include <type_traits>

static_assert(std::is_same<pugl::TextInputFlag, PuglTextInputFlag>::value,
              "TextInputFlag must mirror the C API");
static_assert(std::is_same<pugl::TextInputFlags, PuglTextInputFlags>::value,
              "TextInputFlags must mirror the C API");
static_assert(std::is_same<pugl::TextEditType, PuglTextEditType>::value,
              "TextEditType must mirror the C API");
static_assert(pugl::TextEditEvent::type == PUGL_TEXT_EDIT,
              "TextEditEvent must dispatch PUGL_TEXT_EDIT");

@interface PuglIOSTestTextInputCppTests : XCTestCase
@end

@implementation PuglIOSTestTextInputCppTests

- (void)testPortableCppSurfaceBeforeRealization
{
  pugl::World world{pugl::WorldType::module};
  pugl::View view{world};

  XCTAssertTrue(view.setTextInputFlags(PUGL_TEXT_INPUT_HAS_TEXT) ==
                pugl::Status::success);
  XCTAssertFalse(view.isTextInputActive());
  XCTAssertTrue(view.startTextInput() == pugl::Status::badCall);
  XCTAssertTrue(view.stopTextInput() == pugl::Status::success);
}

@end
