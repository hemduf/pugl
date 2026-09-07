// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include "attributes.h"
#include "emscripten_platform.h"
#include "types.h"

#include <pugl/gl.h>
#include <pugl/pugl.h>

#include <emscripten/html5_webgl.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context;
} PuglEmscriptenGlSurface;

static PuglStatus
puglEmscriptenGlVersion(const PuglView* const view, int* const webglVersion)
{
  const int api   = view->hints[PUGL_CONTEXT_API];
  const int major = view->hints[PUGL_CONTEXT_VERSION_MAJOR];
  const int minor = view->hints[PUGL_CONTEXT_VERSION_MINOR];

  if (api != PUGL_OPENGL_API && api != PUGL_OPENGL_ES_API) {
    return PUGL_BAD_CONFIGURATION;
  }

  if (major == 2 && minor == 0) {
    *webglVersion = 1;
    return PUGL_SUCCESS;
  }

  if (major == 3 && minor == 0) {
    *webglVersion = 2;
    return PUGL_SUCCESS;
  }

  return PUGL_BAD_CONFIGURATION;
}

static PuglStatus
puglEmscriptenGlConfigure(PuglView* const view)
{
  if (!view || !view->impl) {
    return PUGL_BAD_PARAMETER;
  }

  int              webglVersion = 0;
  const PuglStatus versionStatus =
    puglEmscriptenGlVersion(view, &webglVersion);
  if (versionStatus) {
    return versionStatus;
  }

  (void)webglVersion;

  if (view->hints[PUGL_CONTEXT_DEBUG] == PUGL_TRUE) {
    return PUGL_UNSUPPORTED;
  }

  const int swapInterval = view->hints[PUGL_SWAP_INTERVAL];
  if (swapInterval != PUGL_DONT_CARE && swapInterval != 1) {
    return PUGL_UNSUPPORTED;
  }

  return PUGL_SUCCESS;
}

static PuglStatus
puglEmscriptenGlCreate(PuglView* const view)
{
  int webglVersion = 0;
  if (puglEmscriptenGlVersion(view, &webglVersion)) {
    return PUGL_BAD_CONFIGURATION;
  }

  PuglEmscriptenGlSurface* const surface =
    (PuglEmscriptenGlSurface*)calloc(1U, sizeof(PuglEmscriptenGlSurface));
  if (!surface) {
    return PUGL_NO_MEMORY;
  }

  EmscriptenWebGLContextAttributes attributes;
  emscripten_webgl_init_context_attributes(&attributes);
  attributes.alpha = view->hints[PUGL_ALPHA_BITS] != 0;
  attributes.depth = view->hints[PUGL_DEPTH_BITS] != 0;
  attributes.stencil = view->hints[PUGL_STENCIL_BITS] != 0;
  attributes.antialias = view->hints[PUGL_SAMPLE_BUFFERS] > 0 ||
                         view->hints[PUGL_SAMPLES] > 0;
  attributes.premultipliedAlpha = false;
  attributes.preserveDrawingBuffer = false;
  attributes.majorVersion = webglVersion;
  attributes.minorVersion = 0;
  attributes.enableExtensionsByDefault = true;
  attributes.explicitSwapControl = false;

  surface->context =
    emscripten_webgl_create_context(view->impl->canvasSelector, &attributes);
  if (!surface->context) {
    free(surface);
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  if (emscripten_webgl_make_context_current(surface->context) !=
      EMSCRIPTEN_RESULT_SUCCESS) {
    emscripten_webgl_destroy_context(surface->context);
    free(surface);
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  EmscriptenWebGLContextAttributes actual;
  if (emscripten_webgl_get_context_attributes(surface->context, &actual) ==
      EMSCRIPTEN_RESULT_SUCCESS) {
    view->hints[PUGL_CONTEXT_API] = PUGL_OPENGL_ES_API;
    view->hints[PUGL_CONTEXT_VERSION_MAJOR] = actual.majorVersion == 2 ? 3 : 2;
    view->hints[PUGL_CONTEXT_VERSION_MINOR] = 0;
    view->hints[PUGL_ALPHA_BITS] = actual.alpha ? view->hints[PUGL_ALPHA_BITS] : 0;
    view->hints[PUGL_DEPTH_BITS] = actual.depth ? view->hints[PUGL_DEPTH_BITS] : 0;
    view->hints[PUGL_STENCIL_BITS] =
      actual.stencil ? view->hints[PUGL_STENCIL_BITS] : 0;
    view->hints[PUGL_SAMPLE_BUFFERS] = actual.antialias ? 1 : 0;
    if (!actual.antialias) {
      view->hints[PUGL_SAMPLES] = 0;
    }
  }

  view->hints[PUGL_DOUBLE_BUFFER] = PUGL_TRUE;
  view->hints[PUGL_SWAP_INTERVAL] = 1;
  view->impl->surface = (PuglSurface*)surface;

  if (emscripten_webgl_make_context_current(0) != EMSCRIPTEN_RESULT_SUCCESS) {
    view->impl->surface = NULL;
    emscripten_webgl_destroy_context(surface->context);
    free(surface);
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  return PUGL_SUCCESS;
}

static void
puglEmscriptenGlDestroy(PuglView* const view)
{
  PuglEmscriptenGlSurface* const surface =
    view && view->impl ? (PuglEmscriptenGlSurface*)view->impl->surface : NULL;
  if (!surface) {
    return;
  }

  if (emscripten_webgl_get_current_context() == surface->context) {
    (void)emscripten_webgl_make_context_current(0);
  }

  (void)emscripten_webgl_destroy_context(surface->context);
  free(surface);
  view->impl->surface = NULL;
}

PUGL_WARN_UNUSED_RESULT static PuglStatus
puglEmscriptenGlEnter(PuglView* const view,
                      const PuglExposeEvent* const PUGL_UNUSED(expose))
{
  PuglEmscriptenGlSurface* const surface =
    view && view->impl ? (PuglEmscriptenGlSurface*)view->impl->surface : NULL;

  return surface && surface->context &&
           emscripten_webgl_make_context_current(surface->context) ==
             EMSCRIPTEN_RESULT_SUCCESS
           ? PUGL_SUCCESS
           : PUGL_FAILURE;
}

PUGL_WARN_UNUSED_RESULT static PuglStatus
puglEmscriptenGlLeave(PuglView* const view,
                      const PuglExposeEvent* const PUGL_UNUSED(expose))
{
  PuglEmscriptenGlSurface* const surface =
    view && view->impl ? (PuglEmscriptenGlSurface*)view->impl->surface : NULL;
  if (!surface || emscripten_webgl_get_current_context() != surface->context) {
    return PUGL_FAILURE;
  }

  return emscripten_webgl_make_context_current(0) == EMSCRIPTEN_RESULT_SUCCESS
           ? PUGL_SUCCESS
           : PUGL_FAILURE;
}

static void*
puglEmscriptenGlGetContext(PuglView* const view)
{
  PuglEmscriptenGlSurface* const surface =
    view && view->impl ? (PuglEmscriptenGlSurface*)view->impl->surface : NULL;
  return surface ? (void*)(uintptr_t)surface->context : NULL;
}

PuglGlFunc
puglGetProcAddress(const char* const name)
{
  if (!name) {
    return NULL;
  }

  void* const address = emscripten_webgl_get_proc_address(name);
  PuglGlFunc  function = NULL;
  if (sizeof(function) == sizeof(address)) {
    memcpy(&function, &address, sizeof(function));
  }

  return function;
}

PuglStatus
puglEnterContext(PuglView* const view)
{
  return view && view->backend ? view->backend->enter(view, NULL)
                               : PUGL_BAD_PARAMETER;
}

PuglStatus
puglLeaveContext(PuglView* const view)
{
  return view && view->backend ? view->backend->leave(view, NULL)
                               : PUGL_BAD_PARAMETER;
}

const PuglBackend*
puglGlBackend(void)
{
  static const PuglBackend backend = {puglEmscriptenGlConfigure,
                                      puglEmscriptenGlCreate,
                                      puglEmscriptenGlDestroy,
                                      puglEmscriptenGlEnter,
                                      puglEmscriptenGlLeave,
                                      puglEmscriptenGlGetContext};

  return &backend;
}
