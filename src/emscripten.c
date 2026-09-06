// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include "emscripten_platform.h"

#include "emscripten_events.h"
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

typedef struct PuglBrowserTimerImpl PuglBrowserTimer;
typedef struct PuglBrowserPasteRequestImpl PuglBrowserPasteRequest;

struct PuglBrowserTimerImpl {
  PuglBrowserTimer* next;
  PuglView*         view;
  uintptr_t         id;
  long              intervalId;
};

struct PuglBrowserPasteRequestImpl {
  PuglBrowserPasteRequest* next;
  PuglView*                view;
  uintptr_t                token;
  unsigned char*           data;
  size_t                   len;
};

typedef struct {
  char*          type;
  unsigned char* data;
  size_t         len;
} PuglBrowserClipboard;

static PuglBrowserTimer*        puglBrowserTimers        = NULL;
static PuglBrowserPasteRequest* puglBrowserPasteRequests = NULL;
static PuglBrowserClipboard     puglBrowserClipboard     = {NULL, NULL, 0U};
static uintptr_t                puglNextPasteToken       = 1U;
static PuglView*                puglBrowserOfferView     = NULL;
static const PuglDataOfferEvent* puglBrowserOffer        = NULL;
static bool                     puglBrowserOfferHandled  = false;

#if defined(__GNUC__)
#  define PUGL_BROWSER_EXPORT __attribute__((visibility("default")))
#else
#  define PUGL_BROWSER_EXPORT
#endif

EM_JS(int, puglBrowserWriteClipboardText, (const char* text), {
  if (typeof navigator === 'undefined' || !navigator.clipboard ||
      typeof navigator.clipboard.writeText !== 'function') {
    return 0;
  }

  const value = UTF8ToString(text);
  const body = typeof document !== 'undefined' ? document.body : null;
  if (body) {
    body.dataset.puglClipboardWrite = 'pending';
  }

  navigator.clipboard.writeText(value).then(
    () => {
      if (body) {
        body.dataset.puglClipboardWrite = 'success';
      }
    },
    () => {
      if (body) {
        body.dataset.puglClipboardWrite = 'denied';
      }
    });

  return 1;
});

EM_JS(int, puglBrowserReadClipboardText, (uintptr_t token), {
  if (typeof navigator === 'undefined' || !navigator.clipboard ||
      typeof navigator.clipboard.readText !== 'function') {
    return 0;
  }

  const begin = Module['_puglEmscriptenPasteBegin'];
  const end = Module['_puglEmscriptenPasteEnd'];
  if (typeof begin !== 'function' || typeof end !== 'function') {
    return 0;
  }

  const body = typeof document !== 'undefined' ? document.body : null;
  if (body) {
    body.dataset.puglClipboardRead = 'pending';
  }

  navigator.clipboard.readText().then(
    (text) => {
      const bytes = new TextEncoder().encode(String(text));
      const pointer = begin(token, bytes.length);
      if (!pointer) {
        end(token, 0);
        if (body) {
          body.dataset.puglClipboardRead = 'failed';
        }
        return;
      }

      HEAPU8.set(bytes, pointer);
      const delivered = end(token, 1);
      if (body) {
        body.dataset.puglClipboardRead = delivered ? 'success' : 'failed';
      }
    },
    () => {
      end(token, 0);
      if (body) {
        body.dataset.puglClipboardRead = 'denied';
      }
    });

  return 1;
});

static PuglBrowserTimer*
puglFindBrowserTimer(PuglView* const view, const uintptr_t id)
{
  for (PuglBrowserTimer* timer = puglBrowserTimers; timer; timer = timer->next) {
    if (timer->view == view && timer->id == id) {
      return timer;
    }
  }

  return NULL;
}

static PuglBrowserPasteRequest*
puglFindBrowserPasteRequest(const uintptr_t token)
{
  for (PuglBrowserPasteRequest* request = puglBrowserPasteRequests; request;
       request = request->next) {
    if (request->token == token) {
      return request;
    }
  }

  return NULL;
}

static PuglBrowserPasteRequest*
puglFindBrowserPasteRequestForView(const PuglView* const view)
{
  for (PuglBrowserPasteRequest* request = puglBrowserPasteRequests; request;
       request = request->next) {
    if (request->view == view) {
      return request;
    }
  }

  return NULL;
}

static void
puglRemoveBrowserPasteRequest(PuglBrowserPasteRequest* const request)
{
  PuglBrowserPasteRequest** link = &puglBrowserPasteRequests;
  while (*link && *link != request) {
    link = &(*link)->next;
  }

  if (*link) {
    *link = request->next;
  }

  request->next = NULL;
}

static void
puglClearBrowserPasteRequests(PuglView* const view)
{
  PuglBrowserPasteRequest** link = &puglBrowserPasteRequests;
  while (*link) {
    PuglBrowserPasteRequest* const request = *link;
    if (request->view == view) {
      *link = request->next;
      free(request->data);
      request->data  = NULL;
      request->view  = NULL;
      request->token = 0U;
      free(request);
    } else {
      link = &request->next;
    }
  }
}

static void
puglBrowserTimerCallback(void* const data)
{
  PuglBrowserTimer* const timer = (PuglBrowserTimer*)data;
  PuglView* const         view  = timer ? timer->view : NULL;
  if (!view || !view->impl || !view->impl->id) {
    return;
  }

  PuglEvent event  = {0};
  event.timer.type = PUGL_TIMER;
  event.timer.id   = timer->id;
  (void)puglDispatchEvent(view, &event);
}

static void
puglClearBrowserTimers(PuglView* const view)
{
  PuglBrowserTimer** link = &puglBrowserTimers;
  while (*link) {
    PuglBrowserTimer* const timer = *link;
    if (timer->view == view) {
      *link = timer->next;
      emscripten_clear_interval(timer->intervalId);
      free(timer);
    } else {
      link = &timer->next;
    }
  }
}

EMSCRIPTEN_KEEPALIVE PUGL_BROWSER_EXPORT uintptr_t
puglEmscriptenPasteBegin(const uintptr_t token, const size_t len)
{
  PuglBrowserPasteRequest* const request = puglFindBrowserPasteRequest(token);
  PuglView* const view = request ? request->view : NULL;
  if (!request || !view || !view->impl || !view->impl->id) {
    return 0U;
  }

  unsigned char* const data = (unsigned char*)malloc(len + 1U);
  if (!data) {
    return 0U;
  }

  free(request->data);
  request->data = data;
  request->len  = len;
  return (uintptr_t)data;
}

EMSCRIPTEN_KEEPALIVE PUGL_BROWSER_EXPORT int
puglEmscriptenPasteEnd(const uintptr_t token, const int success)
{
  PuglBrowserPasteRequest* const request = puglFindBrowserPasteRequest(token);
  if (!request) {
    return 0;
  }

  PuglView* const      view = request->view;
  unsigned char* const data = request->data;
  const size_t         len  = request->len;
  request->data              = NULL;
  request->view              = NULL;
  puglRemoveBrowserPasteRequest(request);
  free(request);

  if (!success || !data || !view || !view->impl || !view->impl->id) {
    free(data);
    return success ? 0 : 1;
  }

  data[len] = '\0';
  static const char textType[] = "text/plain";
  char* const newType = (char*)malloc(sizeof(textType));
  if (!newType) {
    free(data);
    return 0;
  }

  memcpy(newType, textType, sizeof(textType));
  free(puglBrowserClipboard.type);
  free(puglBrowserClipboard.data);
  puglBrowserClipboard.type = newType;
  puglBrowserClipboard.data = data;
  puglBrowserClipboard.len  = len;

  PuglEvent event         = {0};
  event.offer.type        = PUGL_DATA_OFFER;
  event.offer.time        = puglGetTime(view->world);
  event.offer.x           = view->impl->pointerX;
  event.offer.y           = view->impl->pointerY;
  event.offer.clipboard   = PUGL_CLIPBOARD_GENERAL;

  puglBrowserOfferView    = view;
  puglBrowserOffer        = &event.offer;
  puglBrowserOfferHandled = false;
  const PuglStatus st     = puglDispatchEvent(view, &event);
  puglBrowserOfferView    = NULL;
  puglBrowserOffer        = NULL;
  puglBrowserOfferHandled = false;
  return st == PUGL_SUCCESS;
}

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

    puglClearBrowserTimers(view);
    puglClearBrowserPasteRequests(view);
    puglEmscriptenFreeInput(view);
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

  if ((st = puglEmscriptenRegisterInput(view))) {
    view->backend->destroy(view);
    puglDestroyDomView(impl->id);
    impl->id = 0U;
    return st;
  }

  st = puglDispatchSimpleEvent(view, PUGL_REALIZE);
  if (st) {
    puglEmscriptenUnregisterInput(view);
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
  puglClearBrowserTimers(view);
  puglClearBrowserPasteRequests(view);
  puglEmscriptenUnregisterInput(view);
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
  return puglEmscriptenGrabFocus(view);
}

bool
puglHasFocus(const PuglView* const view)
{
  return puglEmscriptenHasFocus(view);
}

PuglStatus
puglStartTimer(PuglView* const view, const uintptr_t id, const double timeout)
{
  if (!view || !view->impl || !view->impl->id || timeout <= 0.0) {
    return PUGL_BAD_PARAMETER;
  }

  PuglBrowserTimer* timer = puglFindBrowserTimer(view, id);
  if (timer) {
    emscripten_clear_interval(timer->intervalId);
  } else {
    timer = (PuglBrowserTimer*)calloc(1U, sizeof(PuglBrowserTimer));
    if (!timer) {
      return PUGL_NO_MEMORY;
    }

    timer->next = puglBrowserTimers;
    timer->view = view;
    timer->id   = id;
    puglBrowserTimers = timer;
  }

  const long intervalId =
    emscripten_set_interval(puglBrowserTimerCallback, timeout * 1000.0, timer);
  if (intervalId <= 0) {
    PuglBrowserTimer** link = &puglBrowserTimers;
    while (*link && *link != timer) {
      link = &(*link)->next;
    }
    if (*link == timer) {
      *link = timer->next;
    }
    free(timer);
    return PUGL_UNKNOWN_ERROR;
  }

  timer->intervalId = intervalId;
  return PUGL_SUCCESS;
}

PuglStatus
puglStopTimer(PuglView* const view, const uintptr_t id)
{
  if (!view) {
    return PUGL_BAD_PARAMETER;
  }

  PuglBrowserTimer** link = &puglBrowserTimers;
  while (*link) {
    PuglBrowserTimer* const timer = *link;
    if (timer->view == view && timer->id == id) {
      *link = timer->next;
      emscripten_clear_interval(timer->intervalId);
      free(timer);
      return PUGL_SUCCESS;
    }
    link = &timer->next;
  }

  return PUGL_FAILURE;
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
  if (!view || !offer || !view->impl || !view->impl->id ||
      !puglIsValidPosition(regionX, regionY) ||
      !puglIsValidSize(regionWidth, regionHeight) ||
      action > PUGL_DATA_ACTION_PRIVATE || typeIndex != 0U ||
      offer->clipboard != PUGL_CLIPBOARD_GENERAL) {
    return PUGL_BAD_PARAMETER;
  }

  if (view != puglBrowserOfferView || offer != puglBrowserOffer ||
      puglBrowserOfferHandled || !puglBrowserClipboard.type) {
    return PUGL_BAD_CALL;
  }

  puglBrowserOfferHandled = true;
  PuglEvent event         = {0};
  event.data.type         = PUGL_DATA;
  event.data.time         = offer->time;
  event.data.x            = offer->x;
  event.data.y            = offer->y;
  event.data.clipboard    = offer->clipboard;
  event.data.typeIndex    = typeIndex;
  return puglDispatchEvent(view, &event);
}

PuglStatus
puglRejectOffer(PuglView* const                 view,
                const PuglDataOfferEvent* const offer,
                const int                       regionX,
                const int                       regionY,
                const unsigned                  regionWidth,
                const unsigned                  regionHeight)
{
  if (!view || !offer || !view->impl || !view->impl->id ||
      !puglIsValidPosition(regionX, regionY) ||
      !puglIsValidSize(regionWidth, regionHeight) ||
      offer->clipboard != PUGL_CLIPBOARD_GENERAL) {
    return PUGL_BAD_PARAMETER;
  }

  if (view != puglBrowserOfferView || offer != puglBrowserOffer ||
      puglBrowserOfferHandled) {
    return PUGL_BAD_CALL;
  }

  puglBrowserOfferHandled = true;
  return PUGL_SUCCESS;
}

PuglStatus
puglPaste(PuglView* const view)
{
  if (!view || !view->impl || !view->impl->id) {
    return PUGL_BAD_PARAMETER;
  }

  if (puglFindBrowserPasteRequestForView(view)) {
    return PUGL_FAILURE;
  }

  PuglBrowserPasteRequest* const request =
    (PuglBrowserPasteRequest*)calloc(1U, sizeof(PuglBrowserPasteRequest));
  if (!request) {
    return PUGL_NO_MEMORY;
  }

  uintptr_t token = puglNextPasteToken++;
  if (!token) {
    token = puglNextPasteToken++;
  }

  request->view  = view;
  request->token = token;
  request->next  = puglBrowserPasteRequests;
  puglBrowserPasteRequests = request;

  if (!puglBrowserReadClipboardText(token)) {
    puglRemoveBrowserPasteRequest(request);
    free(request);
    return PUGL_UNSUPPORTED;
  }

  return PUGL_SUCCESS;
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
  return clipboard == PUGL_CLIPBOARD_GENERAL && puglBrowserClipboard.type
           ? 1U
           : 0U;
}

const char*
puglGetClipboardType(const PuglView* const view,
                     const PuglClipboard   clipboard,
                     const uint32_t        typeIndex)
{
  (void)view;
  return clipboard == PUGL_CLIPBOARD_GENERAL && typeIndex == 0U
           ? puglBrowserClipboard.type
           : NULL;
}

PuglStatus
puglSetClipboard(PuglView* const     view,
                 const PuglClipboard clipboard,
                 const char* const   type,
                 const void* const   data,
                 const size_t        len)
{
  if (!view || !view->impl || !view->impl->id || !type || (!data && len)) {
    return PUGL_BAD_PARAMETER;
  }

  if (clipboard != PUGL_CLIPBOARD_GENERAL || strcmp(type, "text/plain")) {
    return PUGL_UNSUPPORTED;
  }

  const size_t typeLen = strlen(type);
  char* const newType = (char*)malloc(typeLen + 1U);
  unsigned char* const newData = (unsigned char*)malloc(len + 1U);
  if (!newType || !newData) {
    free(newType);
    free(newData);
    return PUGL_NO_MEMORY;
  }

  memcpy(newType, type, typeLen + 1U);
  if (len) {
    memcpy(newData, data, len);
  }
  newData[len] = '\0';

  if (!puglBrowserWriteClipboardText((const char*)newData)) {
    free(newType);
    free(newData);
    return PUGL_UNSUPPORTED;
  }

  free(puglBrowserClipboard.type);
  free(puglBrowserClipboard.data);
  puglBrowserClipboard.type = newType;
  puglBrowserClipboard.data = newData;
  puglBrowserClipboard.len  = len;
  return PUGL_SUCCESS;
}

const void*
puglGetClipboard(PuglView* const     view,
                 const PuglClipboard clipboard,
                 const uint32_t      typeIndex,
                 size_t* const       len)
{
  (void)view;
  if (len) {
    *len = 0U;
  }

  if (clipboard != PUGL_CLIPBOARD_GENERAL || typeIndex != 0U ||
      !puglBrowserClipboard.data) {
    return NULL;
  }

  if (len) {
    *len = puglBrowserClipboard.len;
  }
  return puglBrowserClipboard.data;
}

PuglStatus
puglSetCursor(PuglView* const view, const PuglCursor cursor)
{
  (void)view;
  (void)cursor;
  return PUGL_UNSUPPORTED;
}
