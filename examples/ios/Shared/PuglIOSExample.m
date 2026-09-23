// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: 0BSD OR ISC

#include "PuglIOSExample.h"

#include <pugl/stub.h>

#import <UIKit/UIKit.h>

#include <math.h>
#include <stdint.h>
#include <string.h>

static UIView*
puglExampleNativeView(PuglView* const view)
{
  return view ? (UIView*)(uintptr_t)puglGetNativeView(view) : nil;
}

static void
puglExampleSetStatus(PuglIOSExample* const example, NSString* const text)
{
  if (example && example->statusLabel) {
    example->statusLabel.text = text;
  }
}

static PuglStatus
puglExampleEvent(PuglView* const view, const PuglEvent* const event)
{
  PuglIOSExample* const example = (PuglIOSExample*)puglGetHandle(view);
  if (!example || !event) {
    return PUGL_SUCCESS;
  }

  switch (event->type) {
  case PUGL_REALIZE:
    puglExampleSetStatus(example, @"Pugl realized");
    break;

  case PUGL_CONFIGURE:
    puglExampleSetStatus(
      example,
      [NSString stringWithFormat:@"Pugl %u × %u",
                                 event->configure.width,
                                 event->configure.height]);
    break;

  case PUGL_POINTER_DOWN:
    if (puglIsTextInputActive(view)) {
      const PuglStatus st = puglStopTextInput(view);
      puglExampleSetStatus(
        example,
        st ? @"Text input stop failed" : @"Text input stopped");
    } else {
      const PuglStatus focusStatus = puglGrabFocus(view);
      const PuglStatus inputStatus =
        focusStatus ? focusStatus : puglStartTextInput(view);
      puglExampleSetStatus(
        example,
        inputStatus ? @"Text input start failed" : @"Text input active");
    }
    break;

  case PUGL_FOCUS_IN:
    puglExampleSetStatus(example, @"Pugl keyboard focus in");
    break;

  case PUGL_FOCUS_OUT:
    puglExampleSetStatus(example, @"Pugl keyboard focus out");
    break;

  case PUGL_KEY_PRESS:
    puglExampleSetStatus(
      example,
      [NSString stringWithFormat:@"Hardware key: 0x%X", event->key.key]);
    break;

  case PUGL_TEXT:
    ++example->textLength;
    (void)puglSetTextInputFlags(
      view, example->textLength ? PUGL_TEXT_INPUT_HAS_TEXT : 0U);
    NSString* const committed =
      [NSString stringWithUTF8String:event->text.string];
    puglExampleSetStatus(
      example,
      [NSString stringWithFormat:@"Committed text: %@",
                                 committed ? committed : @"?"]);
    break;

  case PUGL_TEXT_EDIT:
    if (event->textEdit.edit == PUGL_TEXT_DELETE_BACKWARD &&
        example->textLength) {
      --example->textLength;
    }
    (void)puglSetTextInputFlags(
      view, example->textLength ? PUGL_TEXT_INPUT_HAS_TEXT : 0U);
    puglExampleSetStatus(
      example,
      [NSString stringWithFormat:@"Delete backward (%zu left)",
                                 example->textLength]);
    break;

  default:
    break;
  }

  return PUGL_SUCCESS;
}

static PuglArea
puglExampleInitialSize(UIView* const parent)
{
  UIScreen* const screen =
    (parent && parent.window.screen) ? parent.window.screen : [UIScreen mainScreen];
  const CGFloat scale = screen.scale > 0.0 ? screen.scale : 1.0;
  CGSize size = parent ? parent.bounds.size : screen.bounds.size;

  if (size.width < 1.0 || size.height < 1.0) {
    size = CGSizeMake(640.0, 360.0);
  }

  const double width = fmin(32767.0, fmax(1.0, round(size.width * scale)));
  const double height = fmin(32767.0, fmax(1.0, round(size.height * scale)));
  const PuglArea area = {(PuglSpan)width, (PuglSpan)height};
  return area;
}

bool
puglIOSExampleInit(PuglIOSExample* const example,
                   const PuglWorldType   worldType,
                   UIView* const         parent)
{
  if (!example || (worldType == PUGL_MODULE && !parent)) {
    return false;
  }

  memset(example, 0, sizeof(*example));

  example->world = puglNewWorld(worldType, 0U);
  if (!example->world) {
    return false;
  }

  example->view = puglNewView(example->world);
  if (!example->view) {
    puglIOSExampleDestroy(example);
    return false;
  }

  const PuglArea size = puglExampleInitialSize(parent);
  if (puglSetBackend(example->view, puglStubBackend()) ||
      puglSetEventFunc(example->view, puglExampleEvent) ||
      puglSetSizeHint(
        example->view, PUGL_DEFAULT_SIZE, size.width, size.height) ||
      (parent &&
       puglSetParent(example->view, (PuglNativeView)(uintptr_t)parent))) {
    puglIOSExampleDestroy(example);
    return false;
  }

  puglSetHandle(example->view, example);

  if (puglShow(example->view, PUGL_SHOW_PASSIVE)) {
    puglIOSExampleDestroy(example);
    return false;
  }

  UIView* const nativeView = puglExampleNativeView(example->view);
  if (!nativeView) {
    puglIOSExampleDestroy(example);
    return false;
  }

  nativeView.backgroundColor = [UIColor colorWithRed:0.10
                                                green:0.12
                                                 blue:0.16
                                                alpha:1.0];
  nativeView.clipsToBounds = YES;

  if (parent) {
    puglIOSExampleLayoutInParent(example, parent);
  }

  UILabel* const label = [[UILabel alloc] initWithFrame:CGRectInset(
    nativeView.bounds, 24.0, 24.0)];
  if (!label) {
    puglIOSExampleDestroy(example);
    return false;
  }

  label.autoresizingMask =
    UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  label.numberOfLines = 0;
  label.textAlignment = NSTextAlignmentCenter;
  label.textColor = [UIColor whiteColor];
  label.font = [UIFont monospacedSystemFontOfSize:17.0
                                           weight:UIFontWeightMedium];
  label.text =
    worldType == PUGL_MODULE
      ? @"Pugl AUv3\nTouch to toggle text input"
      : @"Pugl standalone iOS\nTouch to toggle text input";
  label.userInteractionEnabled = NO;

  [nativeView addSubview:label];
  example->statusLabel = label;
  [label release];

  return true;
}

void
puglIOSExampleLayoutInParent(PuglIOSExample* const example, UIView* const parent)
{
  if (!example || !example->view || !parent) {
    return;
  }

  UIView* const nativeView = puglExampleNativeView(example->view);
  if (!nativeView) {
    return;
  }

  nativeView.frame = parent.bounds;
  nativeView.autoresizingMask =
    UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [nativeView setNeedsLayout];
}

void
puglIOSExampleUpdate(PuglIOSExample* const example)
{
  if (example && example->world) {
    (void)puglUpdate(example->world, 0.0);
  }
}

void
puglIOSExampleDestroy(PuglIOSExample* const example)
{
  if (!example) {
    return;
  }

  example->statusLabel = nil;

  if (example->view) {
    puglFreeView(example->view);
    example->view = NULL;
  }

  if (example->world) {
    puglFreeWorld(example->world);
    example->world = NULL;
  }
}
