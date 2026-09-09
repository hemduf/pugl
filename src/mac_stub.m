// Copyright 2019-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "internal.h"
#include "mac.h"
#include "stub.h"

#include <pugl/stub.h>

#import <Cocoa/Cocoa.h>

@interface PuglStubView : NSView<NSDraggingDestination>
@end

@implementation PuglStubView {
@public
  PuglView* puglview;
}

- (id<NSDraggingDestination>)dragDestination
{
  return (id<NSDraggingDestination>)[self superview];
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
  return [[self dragDestination] draggingEntered:sender];
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender
{
  return [[self dragDestination] draggingUpdated:sender];
}

- (void)draggingExited:(id<NSDraggingInfo>)sender
{
  [[self dragDestination] draggingExited:sender];
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender
{
  return [[self dragDestination] prepareForDragOperation:sender];
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
  return [[self dragDestination] performDragOperation:sender];
}

- (void)concludeDragOperation:(id<NSDraggingInfo>)sender
{
  [[self dragDestination] concludeDragOperation:sender];
}

- (void)draggingEnded:(id<NSDraggingInfo>)sender
{
  [[self dragDestination] draggingEnded:sender];
}

- (void)resizeWithOldSuperviewSize:(NSSize)oldSize
{
  PuglWrapperView* wrapper = (PuglWrapperView*)[self superview];

  [super resizeWithOldSuperviewSize:oldSize];
  [wrapper setReshaped];
}

- (void)drawRect:(NSRect)rect
{
  PuglWrapperView* wrapper = (PuglWrapperView*)[self superview];

  [wrapper dispatchExpose:rect];
}

@end

static PuglStatus
puglMacStubCreate(PuglView* view)
{
  PuglInternals* impl     = view->impl;
  PuglStubView*  drawView = [PuglStubView alloc];

  drawView->puglview = view;
  [drawView initWithFrame:NSMakeRect(0,
                                     0,
                                     view->lastConfigure.width,
                                     view->lastConfigure.height)];

  if (view->hints[PUGL_RESIZABLE]) {
    [drawView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  } else {
    [drawView setAutoresizingMask:NSViewNotSizable];
  }

  if (impl->registeredDropTypes) {
    [drawView registerForDraggedTypes:impl->registeredDropTypes];
  }

  impl->drawView = drawView;
  return PUGL_SUCCESS;
}

static void
puglMacStubDestroy(PuglView* view)
{
  PuglStubView* const drawView = (PuglStubView*)view->impl->drawView;

  [drawView removeFromSuperview];
  [drawView release];

  view->impl->drawView = nil;
}

const PuglBackend*
puglStubBackend(void)
{
  static const PuglBackend backend = {puglStubConfigure,
                                      puglMacStubCreate,
                                      puglMacStubDestroy,
                                      puglStubEnter,
                                      puglStubLeave,
                                      puglStubGetContext};

  return &backend;
}
