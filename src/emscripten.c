// Copyright 2026 Fabrizio Duhem
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

EM_JS(int,
      puglCreateDomView,
      (unsigned id,
       unsigned parent,
       int      x,
       int      y,
       int      width,
       int      height),
      {
        if (typeof document === 'undefined') {
          return 0;
        }

        const hostId = `pugl-view-${id}`;
        const canvasId = `pugl-canvas-${id}`;
        if (document.getElementById(hostId) || document.getElementById(canvasId)) {
          return 0;
        }

        let root = null;
        if (parent) {
          root = document.getElementById(`pugl-view-${parent}`);
        } else if (Module['puglRoot']) {
          const configuredRoot = Module['puglRoot'];
          root = typeof configuredRoot === 'string'
                   ? document.querySelector(configuredRoot)
                   : configuredRoot;
        } else {
          root = document.body;
        }

        if (!root) {
          return 0;
        }

        const host = document.createElement('div');
        host.id = hostId;
        host.dataset.puglView = String(id);
        host.style.boxSizing = 'border-box';
        host.style.display = 'none';
        host.style.height = `${height}px`;
        host.style.left = `${x}px`;
        host.style.position = parent ? 'absolute' : 'absolute';
        host.style.top = `${y}px`;
        host.style.width = `${width}px`;

        const canvas = document.createElement('canvas');
        canvas.id = canvasId;
        canvas.dataset.puglCanvas = String(id);
        canvas.tabIndex = 0;
        canvas.width = width;
        canvas.height = height;
        canvas.style.display = 'block';
        canvas.style.height = '100%';
        canvas.style.outline = 'none';
        canvas.style.width = '100%';

        host.appendChild(canvas);
        root.appendChild(host);
        return 1;
      });

EM_JS(void, puglDestroyDomView, (unsigned id), {
  if (typeof document === 'undefined') {
    return;
  }

  const host = document.getElementById(`pugl-view-${id}`);
  if (host) {
    host.remove();
  }
});

EM_JS(void, puglSetDomVisible, (unsigned id, int visible), {
  const host = typeof document !== 'undefined'
                 ? document.getElementById(`pugl-view-${id}`)
                 : null;
  if (host) {
    host.style.display = visible ? 'block' : 'none';
  }
});

EM_JS(void,
      puglSetDomGeometry,
      (unsigned id, int x, int y, int width, int height),
      {
        if (typeof document === 'undefined') {
          return;
        }

        const host = document.getElementById(`pugl-view-${id}`);
        const canvas = document.getElementById(`pugl-canvas-${id}`);
        if (host) {
          host.style.left = `${x}px`;
          host.style.top = `${y}px`;
          host.style.width = `${width}px`;
          host.style.height = `${height}px`;
        }
        if (canvas) {
          canvas.width = width;
          canvas.height = height;
        }
      });

EM_JS(void, puglSetDomPosition, (unsigned id, int x, int y), {
  const host = typeof document !== 'undefined'
                 ? document.getElementById(`pugl-view-${id}`)
                 : null;
  if (host) {
    host.style.left = `${x}px`;
    host.style.top = `${y}px`;
  }
});

EM_JS(int, puglFocusDomView, (unsigned id), {
  const canvas = typeof document !== 'undefined'
                   ? document.getElementById(`pugl-canvas-${id}`)
                   : null;
  if (!canvas) {
    return 0;
  }

  canvas.focus({preventScroll: true});
  return document.activeElement === canvas;
});

EM_JS(int, puglDomViewHasFocus, (unsigned id), {
  const canvas = typeof document !== 'undefined'
                   ? document.getElementById(`pugl-canvas-${id}`)
                   : null;
  return canvas && document.activeElement === canvas ? 1 : 0;
});

EM_JS(void, puglSetDomTitle, (unsigned id, const char* value), {
  const host = typeof document !== 'undefined'
                 ? document.getElementById(`pugl-view-${id}`)
                 : null;
  if (!host) {
    return;
  }

  const title = value ? UTF8ToString(value) : '';
  host.title = title;
  host.setAttribute('aria-label', title);
});

EM_JS(void, puglSetDomCursor, (unsigned id, const char* value), {
  const canvas = typeof document !== 'undefined'
                   ? document.getElementById(`pugl-canvas-${id}`)
                   : null;
  if (canvas) {
    canvas.style.cursor = UTF8ToString(value);
  }
});

EM_JS(int, puglGetDomCenterX, (unsigned parent), {
  if (typeof document === 'undefined' || typeof window === 'undefined') {
    return 0;
  }

  if (parent) {
    const host = document.getElementById(`pugl-view-${parent}`);
    if (host) {
      const bounds = host.getBoundingClientRect();
      return Math.round(bounds.width / 2);
    }
  }

  return Math.round(window.innerWidth / 2);
});

EM_JS(int, puglGetDomCenterY, (unsigned parent), {
  if (typeof document === 'undefined' || typeof window === 'undefined') {
    return 0;
  }

  if (parent) {
    const host = document.getElementById(`pugl-view-${parent}`);
    if (host) {
      const bounds = host.getBoundingClientRect();
      return Math.round(bounds.height / 2);
    }
  }

  return Math.round(window.innerHeight / 2);
});

static PuglCoord
puglClampCoord(const int value)
{
  return (PuglCoord)(value < INT16_MIN   ? INT16_MIN
                     : value > INT16_MAX ? INT16_MAX
                                         : value);
}

static PuglStatus
puglDispatchConfigure(PuglView* const view, const PuglViewStyleFlags style)
{
  const PuglArea size = puglGetInitialSize(view);
  const PuglPoint pos = puglGetInitialPosition(view, size);
  PuglEvent event     = {0};

  event.configure.type   = PUGL_CONFIGURE;
  event.configure.flags  = 0U;
  event.configure.x      = pos.x;
  event.configure.y      = pos.y;
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
  const unsigned parent = (unsigned)view->parent;
  const int x = puglGetDomCenterX(parent);
  const int y = puglGetDomCenterY(parent);
  const PuglPoint center = {puglClampCoord(x), puglClampCoord(y)};
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
  PuglInternals* const impl = view->impl;
  PuglStatus st              = PUGL_SUCCESS;

  if (impl->id) {
    return PUGL_FAILURE;
  }

  if ((st = puglPreRealize(view))) {
    return st;
  }

  puglEnsureHint(view, PUGL_RESIZABLE, PUGL_TRUE);
  puglEnsureHint(view, PUGL_VIEW_TYPE, PUGL_VIEW_TYPE_NORMAL);

  if ((st = view->backend->configure(view))) {
    view->backend->destroy(view);
    return st;
  }

  const PuglArea size = puglGetInitialSize(view);
  const PuglPoint pos = puglGetInitialPosition(view, size);

  impl->id = view->world->impl->nextViewId++;
  snprintf(impl->hostId, sizeof(impl->hostId), "pugl-view-%" PRIuPTR, impl->id);
  snprintf(
    impl->canvasId, sizeof(impl->canvasId), "pugl-canvas-%" PRIuPTR, impl->id);
  snprintf(impl->canvasSelector,
           sizeof(impl->canvasSelector),
           "#pugl-canvas-%" PRIuPTR,
           impl->id);

  if (!puglCreateDomView((unsigned)impl->id,
                         (unsigned)view->parent,
                         pos.x,
                         pos.y,
                         size.width,
                         size.height)) {
    impl->id = 0U;
    view->backend->destroy(view);
    return PUGL_REALIZE_FAILED;
  }

  if ((st = view->backend->create(view))) {
    puglDestroyDomView((unsigned)impl->id);
    impl->id = 0U;
    view->backend->destroy(view);
    return st;
  }

  if (view->strings[PUGL_WINDOW_TITLE]) {
    puglSetDomTitle((unsigned)impl->id, view->strings[PUGL_WINDOW_TITLE]);
  }

  return puglDispatchSimpleEvent(view, PUGL_REALIZE);
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
  puglDestroyDomView((unsigned)impl->id);

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

  PuglInternals* const impl = view->impl;
  PuglStatus st = impl->id ? PUGL_SUCCESS : puglRealize(view);
  if (st) {
    return st;
  }

  if (!impl->mapped) {
    impl->mapped = true;
    puglSetDomVisible((unsigned)impl->id, 1);

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
  if (view->world->state == PUGL_WORLD_EXPOSING) {
    return PUGL_BAD_CALL;
  }

  PuglInternals* const impl = view->impl;
  if (!impl->id) {
    return PUGL_FAILURE;
  }

  if (impl->mapped) {
    impl->mapped = false;
    puglSetDomVisible((unsigned)impl->id, 0);
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
  return view->impl->id && puglFocusDomView((unsigned)view->impl->id)
           ? PUGL_SUCCESS
           : PUGL_FAILURE;
}

bool
puglHasFocus(const PuglView* const view)
{
  return view->impl->id && puglDomViewHasFocus((unsigned)view->impl->id);
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

  switch (event->type) {
  case PUGL_CLIENT:
    return puglDispatchEvent(view, event);
  case PUGL_EXPOSE:
    return puglObscureRegion(view,
                             event->expose.x,
                             event->expose.y,
                             event->expose.width,
                             event->expose.height);
  default:
    return PUGL_UNSUPPORTED;
  }
}

PuglStatus
puglUpdate(PuglWorld* const world, const double timeout)
{
  (void)timeout;

  const PuglWorldState oldState = world->state;
  world->state                  = PUGL_WORLD_UPDATING;
  PuglStatus st                 = PUGL_SUCCESS;

  for (size_t i = 0U; i < world->numViews; ++i) {
    PuglView* const view = world->views[i];
    if (!view || !view->impl->id || !view->impl->mapped) {
      continue;
    }

    if (!(st = puglDispatchSimpleEvent(view, PUGL_UPDATE)) &&
        view->impl->pendingExpose.type == PUGL_EXPOSE &&
        view->stage == PUGL_VIEW_STAGE_CONFIGURED) {
      const PuglEvent expose = view->impl->pendingExpose;
      memset(&view->impl->pendingExpose, 0, sizeof(view->impl->pendingExpose));
      st = puglDispatchEvent(view, &expose);
    }

    if (st) {
      break;
    }
  }

  world->state = oldState;
  return st;
}

double
puglGetTime(const PuglWorld* const world)
{
  return (emscripten_get_now() / 1000.0) - world->startTime;
}

PuglStatus
puglObscureView(PuglView* const view)
{
  const PuglArea size = view->lastConfigure.type == PUGL_CONFIGURE
                          ? (PuglArea){view->lastConfigure.width,
                                       view->lastConfigure.height}
                          : puglGetInitialSize(view);
  return puglObscureRegion(view, 0, 0, size.width, size.height);
}

PuglStatus
puglObscureRegion(PuglView* const view,
                  const int       x,
                  const int       y,
                  const unsigned  width,
                  const unsigned  height)
{
  if (!view->impl->id || !puglIsValidSize(width, height)) {
    return PUGL_BAD_PARAMETER;
  }

  PuglExposeEvent* const pending = &view->impl->pendingExpose.expose;
  if (pending->type != PUGL_EXPOSE) {
    pending->type   = PUGL_EXPOSE;
    pending->flags  = 0U;
    pending->x      = puglClampCoord(x);
    pending->y      = puglClampCoord(y);
    pending->width  = (PuglSpan)width;
    pending->height = (PuglSpan)height;
    return PUGL_SUCCESS;
  }

  const int oldRight  = pending->x + pending->width;
  const int oldBottom = pending->y + pending->height;
  const int newRight  = x + (int)width;
  const int newBottom = y + (int)height;
  const int left      = x < pending->x ? x : pending->x;
  const int top       = y < pending->y ? y : pending->y;
  const int right     = newRight > oldRight ? newRight : oldRight;
  const int bottom    = newBottom > oldBottom ? newBottom : oldBottom;

  pending->x      = puglClampCoord(left);
  pending->y      = puglClampCoord(top);
  pending->width  = (PuglSpan)(right - left);
  pending->height = (PuglSpan)(bottom - top);
  return PUGL_SUCCESS;
}

PuglNativeView
puglGetNativeView(const PuglView* const view)
{
  return view->impl->id;
}

PuglStatus
puglApplyViewString(PuglView* const      view,
                    const PuglStringHint key,
                    const char* const    value)
{
  if (view->impl->id && key == PUGL_WINDOW_TITLE) {
    puglSetDomTitle((unsigned)view->impl->id, value);
  }
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
  if (!view->impl->id) {
    return PUGL_FAILURE;
  }

  puglSetDomPosition((unsigned)view->impl->id, x, y);
  if (view->lastConfigure.type == PUGL_CONFIGURE) {
    PuglEvent event       = {0};
    event.configure      = view->lastConfigure;
    event.configure.x    = puglClampCoord(x);
    event.configure.y    = puglClampCoord(y);
    return puglDispatchEvent(view, &event);
  }

  return PUGL_SUCCESS;
}

PuglStatus
puglSetWindowSize(PuglView* const view,
                  const unsigned  width,
                  const unsigned  height)
{
  if (!view->impl->id || !puglIsValidSize(width, height)) {
    return PUGL_FAILURE;
  }

  const PuglPoint pos = puglGetPositionHint(view, PUGL_CURRENT_POSITION);
  puglSetDomGeometry((unsigned)view->impl->id, pos.x, pos.y, width, height);

  if (view->lastConfigure.type == PUGL_CONFIGURE) {
    PuglEvent event          = {0};
    event.configure         = view->lastConfigure;
    event.configure.width   = (PuglSpan)width;
    event.configure.height  = (PuglSpan)height;
    const PuglStatus status = puglDispatchEvent(view, &event);
    return status ? status : puglObscureView(view);
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
  if (!view->impl->id) {
    return PUGL_FAILURE;
  }

  const char* css = NULL;
  switch (cursor) {
  case PUGL_CURSOR_ARROW:
    css = "default";
    break;
  case PUGL_CURSOR_CARET:
    css = "text";
    break;
  case PUGL_CURSOR_CROSSHAIR:
    css = "crosshair";
    break;
  case PUGL_CURSOR_HAND:
    css = "pointer";
    break;
  case PUGL_CURSOR_NO:
    css = "not-allowed";
    break;
  case PUGL_CURSOR_LEFT_RIGHT:
    css = "ew-resize";
    break;
  case PUGL_CURSOR_UP_DOWN:
    css = "ns-resize";
    break;
  case PUGL_CURSOR_UP_LEFT_DOWN_RIGHT:
    css = "nwse-resize";
    break;
  case PUGL_CURSOR_UP_RIGHT_DOWN_LEFT:
    css = "nesw-resize";
    break;
  case PUGL_CURSOR_ALL_SCROLL:
    css = "move";
    break;
  }

  if (!css) {
    return PUGL_BAD_PARAMETER;
  }

  puglSetDomCursor((unsigned)view->impl->id, css);
  return PUGL_SUCCESS;
}
