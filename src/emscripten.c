// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include "emscripten_platform.h"

#include "internal.h"
#include "platform.h"
#include "types.h"

#include <pugl/pugl.h>

#include <emscripten.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static PuglStatus
puglDispatchConfigure(PuglView* const view, const PuglViewStyleFlags style)
{
  const PuglArea  size  = puglGetInitialSize(view);
  const PuglPoint point = puglGetInitialPosition(view, size);
  PuglEvent       event = {0};

  event.configure.type   = PUGL_CONFIGURE;
  event.configure.x      = point.x;
  event.configure.y      = point.y;
  event.configure.width  = size.width;
  event.configure.height = size.height;
  event.configure.style  = style;

  return puglDispatchEvent(view, &event);
}

PuglWorldInternals*
puglInitWorldInternals(const PuglWorldType type, const PuglWorldFlags flags)
{
  (void)type;
  (void)flags;

  PuglWorldInternals* const impl =
    (PuglWorldInternals*)calloc(1U, sizeof(PuglWorldInternals));
  if (impl) {
    impl->nextViewId = 1U;
  }

  return impl;
}

void
puglFreeWorldInternals(PuglWorld* const world)
{
  if (world) {
    free(world->impl);
  }
}

void*
puglGetNativeWorld(PuglWorld* const world)
{
  return world ? world->impl : NULL;
}

PuglInternals*
puglInitViewInternals(PuglWorld* const world)
{
  (void)world;
  return (PuglInternals*)calloc(1U, sizeof(PuglInternals));
}

void
puglFreeViewInternals(PuglView* const view)
{
  if (view && view->impl) {
    if (view->impl->id) {
      (void)puglUnrealize(view);
    }

    free(view->impl);
  }
}

PuglPoint
puglGetAncestorCenter(const PuglView* const view)
{
  (void)view;
  const PuglPoint center = {0, 0};
  return center;
}

PuglStatus
puglApplySizeHint(PuglView* const view, const PuglSizeHint hint)
{
  (void)view;
  (void)hint;
  return PUGL_SUCCESS;
}

PuglStatus
puglRealize(PuglView* const view)
{
  PuglInternals* const impl = view ? view->impl : NULL;
  if (!impl || impl->id) {
    return PUGL_FAILURE;
  }

  PuglStatus st = puglPreRealize(view);
  if (st) {
    return st;
  }

  if ((st = view->backend->configure(view))) {
    return st;
  }

  if ((st = view->backend->create(view))) {
    view->backend->destroy(view);
    return st;
  }

  impl->id = view->world->impl->nextViewId++;
  st       = puglDispatchSimpleEvent(view, PUGL_REALIZE);
  if (st) {
    view->backend->destroy(view);
    impl->id = 0U;
  }

  return st;
}

PuglStatus
puglUnrealize(PuglView* const view)
{
  PuglInternals* const impl = view ? view->impl : NULL;
  if (!impl || !impl->id) {
    return PUGL_FAILURE;
  }

  const PuglStatus st = puglDispatchSimpleEvent(view, PUGL_UNREALIZE);
  view->backend->destroy(view);

  impl->id     = 0U;
  impl->mapped = false;
  memset(&impl->pendingExpose, 0, sizeof(impl->pendingExpose));
  memset(&view->lastConfigure, 0, sizeof(view->lastConfigure));
  return st;
}

PuglStatus
puglShow(PuglView* const view, const PuglShowCommand command)
{
  (void)command;

  PuglInternals* const impl = view ? view->impl : NULL;
  if (!impl) {
    return PUGL_BAD_PARAMETER;
  }

  PuglStatus st = impl->id ? PUGL_SUCCESS : puglRealize(view);
  if (st) {
    return st;
  }

  if (!impl->mapped) {
    impl->mapped = true;
    st           = puglDispatchConfigure(view, PUGL_VIEW_STYLE_MAPPED);
  }

  return st;
}

PuglStatus
puglHide(PuglView* const view)
{
  PuglInternals* const impl = view ? view->impl : NULL;
  if (!impl || !impl->id) {
    return PUGL_FAILURE;
  }

  if (view->world->state == PUGL_WORLD_EXPOSING) {
    return PUGL_BAD_CALL;
  }

  if (impl->mapped) {
    impl->mapped = false;
    return puglDispatchConfigure(view, 0U);
  }

  return PUGL_SUCCESS;
}

PuglStatus
puglSetViewStyle(PuglView* const view, const PuglViewStyleFlags flags)
{
  const PuglViewStyleFlags supported =
    PUGL_VIEW_STYLE_MAPPED | PUGL_VIEW_STYLE_HIDDEN;

  if (flags & ~supported) {
    return PUGL_UNSUPPORTED;
  }

  return (flags & PUGL_VIEW_STYLE_HIDDEN) ? puglHide(view)
         : (flags & PUGL_VIEW_STYLE_MAPPED) ? puglShow(view, PUGL_SHOW_PASSIVE)
                                            : PUGL_SUCCESS;
}

PuglStatus
puglGrabFocus(PuglView* const view)
{
  (void)view;
  return PUGL_UNSUPPORTED;
}

bool
puglHasFocus(const PuglView* const view)
{
  (void)view;
  return false;
}

PuglStatus
puglStartTimer(PuglView* const view, const uintptr_t id, const double timeout)
{
  (void)view;
  (void)id;
  (void)timeout;
  return PUGL_UNSUPPORTED;
}

PuglStatus
puglStopTimer(PuglView* const view, const uintptr_t id)
{
  (void)view;
  (void)id;
  return PUGL_UNSUPPORTED;
}

PuglStatus
puglSendEvent(PuglView* const view, const PuglEvent* const event)
{
  if (!view || !event || !view->impl->id) {
    return PUGL_BAD_PARAMETER;
  }

  return event->type == PUGL_CLIENT ? puglDispatchEvent(view, event)
                                    : PUGL_UNSUPPORTED;
}

PuglStatus
puglUpdate(PuglWorld* const world, const double timeout)
{
  (void)timeout;

  if (!world) {
    return PUGL_BAD_PARAMETER;
  }

  const PuglWorldState oldState = world->state;
  world->state                  = PUGL_WORLD_UPDATING;
  PuglStatus st                 = PUGL_SUCCESS;

  for (size_t i = 0U; i < world->numViews && !st; ++i) {
    PuglView* const view = world->views[i];
    if (view && view->impl->id && view->impl->mapped) {
      st = puglDispatchSimpleEvent(view, PUGL_UPDATE);
    }
  }

  world->state = oldState;
  return st;
}

double
puglGetTime(const PuglWorld* const world)
{
  const double now = emscripten_get_now() / 1000.0;
  return world ? now - world->startTime : now;
}

PuglStatus
puglObscureView(PuglView* const view)
{
  (void)view;
  return PUGL_UNSUPPORTED;
}

PuglStatus
puglObscureRegion(PuglView* const view,
                  const int       x,
                  const int       y,
                  const unsigned  width,
                  const unsigned  height)
{
  (void)view;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  return PUGL_UNSUPPORTED;
}

PuglNativeView
puglGetNativeView(const PuglView* const view)
{
  return view && view->impl ? (PuglNativeView)view->impl->id : 0U;
}

PuglStatus
puglApplyViewString(PuglView* const      view,
                    const PuglStringHint key,
                    const char* const    value)
{
  (void)view;
  (void)key;
  (void)value;
  return PUGL_SUCCESS;
}

double
puglGetScaleFactor(const PuglView* const view)
{
  (void)view;
  return 1.0;
}

PuglStatus
puglSetWindowPosition(PuglView* const view, const int x, const int y)
{
  (void)view;
  (void)x;
  (void)y;
  return PUGL_UNSUPPORTED;
}

PuglStatus
puglSetWindowSize(PuglView* const view,
                  const unsigned  width,
                  const unsigned  height)
{
  (void)view;
  (void)width;
  (void)height;
  return PUGL_UNSUPPORTED;
}

PuglStatus
puglSetTransientParent(PuglView* const view, const PuglNativeView parent)
{
  if (view->parent) {
    return PUGL_FAILURE;
  }

  view->transientParent = parent;
  return PUGL_SUCCESS;
}

PuglStatus
puglAcceptOffer(PuglView* const                 view,
                const PuglDataOfferEvent* const offer,
                const uint32_t                  typeIndex,
                const PuglDataAction            action,
                const int                       regionX,
                const int                       regionY,
                const unsigned                  regionWidth,
                const unsigned                  regionHeight)
{
  (void)view;
  (void)offer;
  (void)typeIndex;
  (void)action;
  (void)regionX;
  (void)regionY;
  (void)regionWidth;
  (void)regionHeight;
  return PUGL_UNSUPPORTED;
}

PuglStatus
puglRejectOffer(PuglView* const                 view,
                const PuglDataOfferEvent* const offer,
                const int                       regionX,
                const int                       regionY,
                const unsigned                  regionWidth,
                const unsigned                  regionHeight)
{
  (void)view;
  (void)offer;
  (void)regionX;
  (void)regionY;
  (void)regionWidth;
  (void)regionHeight;
  return PUGL_UNSUPPORTED;
}

PuglStatus
puglPaste(PuglView* const view)
{
  (void)view;
  return PUGL_UNSUPPORTED;
}

PuglStatus
puglRegisterDropType(PuglView* const view, const char* const type)
{
  (void)view;
  (void)type;
  return PUGL_UNSUPPORTED;
}

uint32_t
puglGetNumClipboardTypes(const PuglView* const view,
                         const PuglClipboard   clipboard)
{
  (void)view;
  (void)clipboard;
  return 0U;
}

const char*
puglGetClipboardType(const PuglView* const view,
                     const PuglClipboard   clipboard,
                     const uint32_t        typeIndex)
{
  (void)view;
  (void)clipboard;
  (void)typeIndex;
  return NULL;
}

PuglStatus
puglSetClipboard(PuglView* const     view,
                 const PuglClipboard clipboard,
                 const char* const   type,
                 const void* const   data,
                 const size_t        len)
{
  (void)view;
  (void)clipboard;
  (void)type;
  (void)data;
  (void)len;
  return PUGL_UNSUPPORTED;
}

const void*
puglGetClipboard(PuglView* const     view,
                 const PuglClipboard clipboard,
                 const uint32_t      typeIndex,
                 size_t* const       len)
{
  (void)view;
  (void)clipboard;
  (void)typeIndex;
  if (len) {
    *len = 0U;
  }
  return NULL;
}

PuglStatus
puglSetCursor(PuglView* const view, const PuglCursor cursor)
{
  (void)view;
  (void)cursor;
  return PUGL_UNSUPPORTED;
}
