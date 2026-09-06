// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include "emscripten_platform.h"

#include "internal.h"
#include "platform.h"
#include "types.h"

#include <pugl/pugl.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static PuglCoord
puglClampCoord(const int value)
{
  return (PuglCoord)(value < INT16_MIN   ? INT16_MIN
                     : value > INT16_MAX ? INT16_MAX
                                         : value);
}

static bool
puglCreateDomView(const PuglView* const view,
                  const PuglPoint       position,
                  const PuglArea        size)
{
  char script[1024] = {0};
  snprintf(script,
           sizeof(script),
           "(()=>{if(typeof document==='undefined'||!document.body)return 0;"
           "const id='pugl-view-%" PRIuPTR "';"
           "if(document.getElementById(id))return 0;"
           "const e=document.createElement('canvas');"
           "e.id=id;e.dataset.puglView='%" PRIuPTR "';e.tabIndex=0;"
           "e.style.boxSizing='border-box';e.style.display='none';"
           "e.style.position='absolute';e.style.left='%dpx';e.style.top='%dpx';"
           "e.style.width='%upx';e.style.height='%upx';e.style.outline='none';"
           "const r=(typeof window!=='undefined'&&window.devicePixelRatio>0)"
           "?window.devicePixelRatio:1;"
           "e.width=Math.max(1,Math.round(%u*r));"
           "e.height=Math.max(1,Math.round(%u*r));"
           "document.body.appendChild(e);return 1;})()",
           view->impl->id,
           view->impl->id,
           position.x,
           position.y,
           size.width,
           size.height,
           size.width,
           size.height);
  return emscripten_run_script_int(script) != 0;
}

static void
puglDestroyDomView(const uintptr_t id)
{
  char script[256] = {0};
  snprintf(script,
           sizeof(script),
           "(()=>{const e=document.getElementById('pugl-view-%" PRIuPTR
           "');if(e)e.remove();})()",
           id);
  emscripten_run_script(script);
}

static void
puglSetDomVisible(const uintptr_t id, const bool visible)
{
  char script[256] = {0};
  snprintf(script,
           sizeof(script),
           "(()=>{const e=document.getElementById('pugl-view-%" PRIuPTR
           "');if(e)e.style.display='%s';})()",
           id,
           visible ? "block" : "none");
  emscripten_run_script(script);
}

static void
puglSetDomGeometry(const PuglView* const view,
                   const PuglPoint       position,
                   const unsigned        width,
                   const unsigned        height)
{
  char script[768] = {0};
  snprintf(script,
           sizeof(script),
           "(()=>{const e=document.getElementById('pugl-view-%" PRIuPTR
           "');if(!e)return;"
           "e.style.left='%dpx';e.style.top='%dpx';"
           "e.style.width='%upx';e.style.height='%upx';"
           "const r=(typeof window!=='undefined'&&window.devicePixelRatio>0)"
           "?window.devicePixelRatio:1;"
           "e.width=Math.max(1,Math.round(%u*r));"
           "e.height=Math.max(1,Math.round(%u*r));})()",
           view->impl->id,
           position.x,
           position.y,
           width,
           height,
           width,
           height);
  emscripten_run_script(script);
}

static int
puglGetBrowserDimension(const char* const name)
{
  char script[128] = {0};
  snprintf(script,
           sizeof(script),
           "typeof window==='undefined'?0:Math.round(window.%s/2)",
           name);
  return emscripten_run_script_int(script);
}

static PuglStatus
puglDispatchConfigure(PuglView* const view, const PuglViewStyleFlags style)
{
  const PuglArea  size  = puglGetInitialSize(view);
  const PuglPoint point = puglGetInitialPosition(view, size);
  PuglEvent       event = {0};

  event.configure.type   = PUGL_CONFIGURE;
  event.configure.flags  = 0U;
  event.configure.x      = point.x;
  event.configure.y      = point.y;
  event.configure.width  = size.width;
  event.configure.height = size.height;
  event.configure.style  = style;

  return puglDispatchEvent(view, &event);
}

static PuglArea
puglCurrentArea(const PuglView* const view)
{
  return view->lastConfigure.type == PUGL_CONFIGURE
           ? (PuglArea){view->lastConfigure.width, view->lastConfigure.height}
           : puglGetInitialSize(view);
}

static void
puglMergeExpose(PuglExposeEvent* const       pending,
                const PuglExposeEvent* const event)
{
  if (pending->type != PUGL_EXPOSE) {
    *pending = *event;
    return;
  }

  const int left   = event->x < pending->x ? event->x : pending->x;
  const int top    = event->y < pending->y ? event->y : pending->y;
  const int right0 = pending->x + pending->width;
  const int right1 = event->x + event->width;
  const int bottom0 = pending->y + pending->height;
  const int bottom1 = event->y + event->height;
  const int right  = right1 > right0 ? right1 : right0;
  const int bottom = bottom1 > bottom0 ? bottom1 : bottom0;

  pending->x      = (PuglCoord)left;
  pending->y      = (PuglCoord)top;
  pending->width  = (PuglSpan)(right - left);
  pending->height = (PuglSpan)(bottom - top);
}

PuglWorldInternals*
puglInitWorldInternals(const PuglWorldType type, const PuglWorldFlags flags)
{
  (void)type;
  (void)flags;

  PuglWorldInternals* const impl =
    (PuglWorldInternals*)calloc(1U, sizeof(PuglWorldInternals));
  if (impl) {
    impl->nextViewId  = 1U;
    impl->scaleFactor = emscripten_get_device_pixel_ratio();
    if (impl->scaleFactor <= 0.0) {
      impl->scaleFactor = 1.0;
    }
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
  const PuglPoint center = {
    puglClampCoord(puglGetBrowserDimension("innerWidth")),
    puglClampCoord(puglGetBrowserDimension("innerHeight"))};
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

  const PuglArea  size = puglGetInitialSize(view);
  const PuglPoint pos  = puglGetInitialPosition(view, size);

  impl->id = view->world->impl->nextViewId++;
  snprintf(impl->canvasSelector,
           sizeof(impl->canvasSelector),
           "#pugl-view-%" PRIuPTR,
           impl->id);

  if (!puglCreateDomView(view, pos, size)) {
    impl->id = 0U;
    return PUGL_REALIZE_FAILED;
  }

  if ((st = view->backend->create(view))) {
    puglDestroyDomView(impl->id);
    impl->id = 0U;
    return st;
  }

  st = puglDispatchSimpleEvent(view, PUGL_REALIZE);
  if (st) {
    view->backend->destroy(view);
    puglDestroyDomView(impl->id);
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

  const uintptr_t id = impl->id;
  const PuglStatus st = puglDispatchSimpleEvent(view, PUGL_UNREALIZE);
  view->backend->destroy(view);
  puglDestroyDomView(id);

  impl->id                = 0U;
  impl->mapped            = false;
  impl->canvasSelector[0] = '\0';
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
    puglSetDomVisible(impl->id, true);
    st = puglDispatchConfigure(view, PUGL_VIEW_STYLE_MAPPED);
    if (!st) {
      st = puglObscureView(view);
    }
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
    puglSetDomVisible(impl->id, false);
    return puglDispatchConfigure(view, PUGL_VIEW_STYLE_HIDDEN);
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

  if (view->world->state == PUGL_WORLD_EXPOSING) {
    return PUGL_BAD_CALL;
  }

  if (event->type == PUGL_CLIENT) {
    return puglDispatchEvent(view, event);
  }

  if (event->type == PUGL_EXPOSE) {
    return puglObscureRegion(view,
                             event->expose.x,
                             event->expose.y,
                             event->expose.width,
                             event->expose.height);
  }

  return PUGL_UNSUPPORTED;
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
    if (!view || !view->impl->id || !view->impl->mapped) {
      continue;
    }

    st = puglDispatchSimpleEvent(view, PUGL_UPDATE);
    if (!st && view->impl->pendingExpose.type == PUGL_EXPOSE &&
        view->stage == PUGL_VIEW_STAGE_CONFIGURED) {
      const PuglEvent expose = view->impl->pendingExpose;
      memset(&view->impl->pendingExpose, 0, sizeof(view->impl->pendingExpose));
      st = puglDispatchEvent(view, &expose);
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
  if (!view || !view->impl->id) {
    return PUGL_BAD_PARAMETER;
  }

  const PuglArea size = puglCurrentArea(view);
  return puglObscureRegion(view, 0, 0, size.width, size.height);
}

PuglStatus
puglObscureRegion(PuglView* const view,
                  const int       x,
                  const int       y,
                  const unsigned  width,
                  const unsigned  height)
{
  if (!view || !view->impl->id || !puglIsValidPosition(x, y) ||
      !puglIsValidSize(width, height)) {
    return PUGL_BAD_PARAMETER;
  }

  if (view->world->state == PUGL_WORLD_EXPOSING) {
    return PUGL_BAD_CALL;
  }

  const PuglArea area = puglCurrentArea(view);
  const int64_t left64 = x < 0 ? 0 : x;
  const int64_t top64 = y < 0 ? 0 : y;
  const int64_t right64 = (int64_t)x + (int64_t)width;
  const int64_t bottom64 = (int64_t)y + (int64_t)height;
  const int64_t right = right64 < 0
                          ? 0
                          : right64 > area.width ? area.width : right64;
  const int64_t bottom = bottom64 < 0
                           ? 0
                           : bottom64 > area.height ? area.height : bottom64;

  if (left64 >= right || top64 >= bottom) {
    return PUGL_SUCCESS;
  }

  const PuglExposeEvent event = {
    PUGL_EXPOSE,
    0U,
    (PuglCoord)left64,
    (PuglCoord)top64,
    (PuglSpan)(right - left64),
    (PuglSpan)(bottom - top64),
  };

  puglMergeExpose(&view->impl->pendingExpose.expose, &event);
  return PUGL_SUCCESS;
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
  const double ratio = emscripten_get_device_pixel_ratio();
  return ratio > 0.0 ? ratio : view->world->impl->scaleFactor;
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
  if (!view || !view->impl->id || !puglIsValidSize(width, height)) {
    return PUGL_BAD_PARAMETER;
  }

  const PuglPoint position =
    view->lastConfigure.type == PUGL_CONFIGURE
      ? (PuglPoint){view->lastConfigure.x, view->lastConfigure.y}
      : puglGetInitialPosition(
          view, (PuglArea){(PuglSpan)width, (PuglSpan)height});

  puglSetDomGeometry(view, position, width, height);

  if (view->lastConfigure.type == PUGL_CONFIGURE) {
    PuglEvent event         = {0};
    event.configure        = view->lastConfigure;
    event.configure.width  = (PuglSpan)width;
    event.configure.height = (PuglSpan)height;
    const PuglStatus st    = puglDispatchEvent(view, &event);
    return st ? st : puglObscureView(view);
  }

  return PUGL_SUCCESS;
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
