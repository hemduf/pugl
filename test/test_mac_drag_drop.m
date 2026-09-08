// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

// Tests the native MacOS drag-and-drop destination lifecycle

#undef NDEBUG

#include <pugl/pugl.h>
#include <pugl/stub.h>

#import <Cocoa/Cocoa.h>

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  unsigned offers;
  unsigned dataEvents;
  bool     accept;
  double   dataX;
  double   dataY;
  char*    payload;
  size_t   payloadLen;
} TestState;

@interface TestDraggingInfo : NSObject {
@private
  NSPasteboard* pasteboard;
  NSPoint       location;
}

- (instancetype)initWithPasteboard:(NSPasteboard*)pasteboard
                          location:(NSPoint)location;
- (NSPasteboard*)draggingPasteboard;
- (NSPoint)draggingLocation;

@end

@implementation TestDraggingInfo

- (instancetype)initWithPasteboard:(NSPasteboard*)newPasteboard
                          location:(NSPoint)newLocation
{
  if ((self = [super init])) {
    pasteboard = [newPasteboard retain];
    location   = newLocation;
  }

  return self;
}

- (void)dealloc
{
  [pasteboard release];
  [super dealloc];
}

- (NSPasteboard*)draggingPasteboard
{
  return pasteboard;
}

- (NSPoint)draggingLocation
{
  return location;
}

@end

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
  TestState* const state = (TestState*)puglGetHandle(view);

  if (event->type == PUGL_DATA_OFFER) {
    ++state->offers;

    const uint32_t count =
      puglGetNumClipboardTypes(view, event->offer.clipboard);
    for (uint32_t i = 0U; i < count; ++i) {
      const char* const type =
        puglGetClipboardType(view, event->offer.clipboard, i);
      if (type && !strcmp(type, "text/uri-list")) {
        return state->accept
                 ? puglAcceptOffer(view,
                                   &event->offer,
                                   i,
                                   PUGL_DATA_ACTION_COPY,
                                   0,
                                   0,
                                   UINT32_MAX,
                                   UINT32_MAX)
                 : puglRejectOffer(
                     view, &event->offer, 0, 0, UINT32_MAX, UINT32_MAX);
      }
    }

    return PUGL_SUCCESS;
  }

  if (event->type == PUGL_DATA) {
    ++state->dataEvents;
    state->dataX = event->data.x;
    state->dataY = event->data.y;

    size_t      len  = 0U;
    const void* data = puglGetClipboard(
      view, event->data.clipboard, event->data.typeIndex, &len);
    assert(data);
    assert(len > 0U);

    free(state->payload);
    state->payload = (char*)calloc(len + 1U, 1U);
    assert(state->payload);
    memcpy(state->payload, data, len);
    state->payloadLen = len;
  }

  return PUGL_SUCCESS;
}

int
main(void)
{
  NSAutoreleasePool* const pool = [NSAutoreleasePool new];

  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglView* const  view  = puglNewView(world);
  assert(world);
  assert(view);

  TestState state = {0U, 0U, true, 0.0, 0.0, NULL, 0U};

  puglSetWorldString(world, PUGL_CLASS_NAME, "PuglMacDragDropTest");
  puglSetViewString(view, PUGL_WINDOW_TITLE, "Pugl Mac Drag Drop Test");
  puglSetBackend(view, puglStubBackend());
  puglSetHandle(view, &state);
  puglSetEventFunc(view, onEvent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 256U, 256U);
  assert(!puglRegisterDropType(view, "text/uri-list"));
  assert(!puglRealize(view));

  NSView* const destination = (NSView*)puglGetNativeView(view);
  assert(destination);

  NSPasteboard* const pasteboard =
    [NSPasteboard pasteboardWithName:@"PuglMacDragDropTestPasteboard"];
  [pasteboard clearContents];

  NSURL* const fileUri = [NSURL fileURLWithPath:@"/tmp/pugl drag test.wav"];
  assert([pasteboard writeObjects:@[fileUri]]);

  const NSPoint location = NSMakePoint(32.0, 48.0);
  TestDraggingInfo* const info =
    [[TestDraggingInfo alloc] initWithPasteboard:pasteboard location:location];

  id<NSDraggingDestination> const dragDestination =
    (id<NSDraggingDestination>)destination;

  // Entering/accepting an offer must not deliver PUGL_DATA before the drop.
  const NSDragOperation entered =
    [dragDestination draggingEntered:(id<NSDraggingInfo>)info];
  assert(entered == NSDragOperationCopy);
  assert(state.offers == 1U);
  assert(state.dataEvents == 0U);

  // Repeated updates may re-offer, but must still not deliver data.
  const NSDragOperation updated =
    [dragDestination draggingUpdated:(id<NSDraggingInfo>)info];
  assert(updated == NSDragOperationCopy);
  assert(state.offers == 2U);
  assert(state.dataEvents == 0U);

  assert([dragDestination prepareForDragOperation:(id<NSDraggingInfo>)info]);
  assert([dragDestination performDragOperation:(id<NSDraggingInfo>)info]);
  assert(state.dataEvents == 1U);
  assert(state.payload);

  const char* const expectedUri = [[fileUri absoluteString] UTF8String];
  assert(expectedUri);
  assert(strstr(state.payload, expectedUri));
  assert(state.payload[state.payloadLen - 1U] == '\n');

  // Cocoa drag locations are in window points.  Pugl events are view-relative
  // physical coordinates, so verify the same conversion used by other input.
  const NSPoint expectedPoint =
    [destination convertPoint:location fromView:nil];
  const double scale = puglGetScaleFactor(view);
  assert(fabs(state.dataX - expectedPoint.x * scale) < 0.001);
  assert(fabs(state.dataY - expectedPoint.y * scale) < 0.001);

  [dragDestination concludeDragOperation:(id<NSDraggingInfo>)info];

  // A rejected offer must produce no data and must not remain accepted.
  state.accept = false;
  const NSDragOperation rejected =
    [dragDestination draggingEntered:(id<NSDraggingInfo>)info];
  assert(rejected == NSDragOperationNone);
  assert(state.offers == 3U);
  assert(state.dataEvents == 1U);
  assert(![dragDestination prepareForDragOperation:(id<NSDraggingInfo>)info]);
  assert(![dragDestination performDragOperation:(id<NSDraggingInfo>)info]);
  assert(state.dataEvents == 1U);
  [dragDestination draggingEnded:(id<NSDraggingInfo>)info];

  [info release];
  free(state.payload);
  puglFreeView(view);
  puglFreeWorld(world);
  [pool drain];
  return 0;
}
