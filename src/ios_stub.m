// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include "internal.h"
#include "ios.h"
#include "stub.h"

#include <pugl/stub.h>

#import <UIKit/UIKit.h>

@interface PuglStubView : UIView {
@public
  PuglView* puglview;
}
@end

@implementation PuglStubView

- (void)drawRect:(CGRect)rect
{
  PuglWrapperView* const wrapper = (PuglWrapperView*)self.superview;
  [wrapper dispatchExpose:rect];
}

@end

static PuglStatus
puglIosStubCreate(PuglView* view)
{
  PuglStubView* const drawView =
    [[PuglStubView alloc] initWithFrame:view->impl->wrapperView.bounds];
  if (!drawView) {
    return PUGL_NO_MEMORY;
  }

  drawView->puglview = view;
  view->impl->drawView = drawView;
  return PUGL_SUCCESS;
}

static void
puglIosStubDestroy(PuglView* view)
{
  PuglStubView* const drawView = (PuglStubView*)view->impl->drawView;
  if (!drawView) {
    return;
  }

  [drawView removeFromSuperview];
  drawView->puglview = NULL;
  [drawView release];
  view->impl->drawView = nil;
}

const PuglBackend*
puglStubBackend(void)
{
  static const PuglBackend backend = {
    puglStubConfigure,
    puglIosStubCreate,
    puglIosStubDestroy,
    puglStubEnter,
    puglStubLeave,
    puglStubGetContext,
  };

  return &backend;
}
