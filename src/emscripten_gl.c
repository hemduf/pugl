// Copyright 2026 Fabrizio Duhem
// SPDX-License-Identifier: ISC

#include "attributes.h"
#include "emscripten_platform.h"
#include "stub.h"
#include "types.h"

#include <pugl/gl.h>
#include <pugl/pugl.h>

#include <emscripten/html5_webgl.h>

#include <GLES3/gl3.h>

#include <stdbool.h>
#include <stdlib.h>

typedef struct {
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context;
  int                              webglVersion;
} PuglEmscriptenGlSurface;

static int
puglEmscriptenBoolHint(const int value, const int fallback)
{
  return value == PUGL_DONT_CARE ? fallback : value > 0;
}

static int
puglEmscriptenRequestedWebGlVersion(const PuglView* const view)
{
  const int api   = view->hints[PUGL_CONTEXT_API];
  const int major = view->hints[PUGL_CONTEXT_VERSION_MAJOR];

  if (api != PUGL_OPENGL_API && api != PUGL_OPENGL_ES_API) {
    return 0;
  }

  if (major == PUGL_DONT_CARE || major <= 2) {
    return 1;
  }

  return major == 3 ? 2 : 0;
}

static PuglStatus
puglEmscriptenGlConfigure(PuglView* const view)
{
  const int webglVersion = puglEmscriptenRequestedWebGlVersion(view);
  if (!webglVersion) {
    return PUGL_UNSUPPORTED;
  }

  PuglEmscriptenGlSurface* const surface =
    (PuglEmscriptenGlSurface*)calloc(1U, sizeof(PuglEmscriptenGlSurface));
  if (!surface) {
    return PUGL_NO_MEMORY;
  }

  surface->webglVersion = webglVersion;
  view->impl->surface   = surface;
  return PUGL_SUCCESS;
}

static void
puglEmscriptenGlUpdateHints(PuglView* const view,
                            const int       webglVersion)
{
  GLint value = 0;

  glGetIntegerv(GL_RED_BITS, &value);
  view->hints[PUGL_RED_BITS] = value;
  glGetIntegerv(GL_GREEN_BITS, &value);
  view->hints[PUGL_GREEN_BITS] = value;
  glGetIntegerv(GL_BLUE_BITS, &value);
  view->hints[PUGL_BLUE_BITS] = value;
  glGetIntegerv(GL_ALPHA_BITS, &value);
  view->hints[PUGL_ALPHA_BITS] = value;
  glGetIntegerv(GL_DEPTH_BITS, &value);
  view->hints[PUGL_DEPTH_BITS] = value;
  glGetIntegerv(GL_STENCIL_BITS, &value);
  view->hints[PUGL_STENCIL_BITS] = value;
  glGetIntegerv(GL_SAMPLE_BUFFERS, &value);
  view->hints[PUGL_SAMPLE_BUFFERS] = value;
  glGetIntegerv(GL_SAMPLES, &value);
  view->hints[PUGL_SAMPLES] = value;

  view->hints[PUGL_CONTEXT_API]           = PUGL_OPENGL_ES_API;
  view->hints[PUGL_CONTEXT_VERSION_MAJOR] = webglVersion == 2 ? 3 : 2;
  view->hints[PUGL_CONTEXT_VERSION_MINOR] = 0;
  view->hints[PUGL_CONTEXT_PROFILE]       = PUGL_OPENGL_CORE_PROFILE;
  view->hints[PUGL_DOUBLE_BUFFER]         = PUGL_TRUE;
  view->hints[PUGL_SWAP_INTERVAL]         = 1;
}

static PuglStatus
puglEmscriptenGlCreate(PuglView* const view)
{
  PuglEmscriptenGlSurface* const surface =
    (PuglEmscriptenGlSurface*)view->impl->surface;
  if (!surface || !view->impl->canvasSelector[0]) {
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);

  attrs.alpha = puglEmscriptenBoolHint(view->hints[PUGL_ALPHA_BITS], true);
  attrs.depth = puglEmscriptenBoolHint(view->hints[PUGL_DEPTH_BITS], false);
  attrs.stencil = puglEmscriptenBoolHint(view->hints[PUGL_STENCIL_BITS], false);
  attrs.antialias =
    (view->hints[PUGL_SAMPLE_BUFFERS] > 0 || view->hints[PUGL_SAMPLES] > 0);
  attrs.premultipliedAlpha = true;
  attrs.preserveDrawingBuffer = false;
  attrs.enableExtensionsByDefault = true;
  attrs.majorVersion = surface->webglVersion;
  attrs.minorVersion = 0;

  surface->context =
    emscripten_webgl_create_context(view->impl->canvasSelector, &attrs);
  if (surface->context <= 0) {
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  if (emscripten_webgl_make_context_current(surface->context) !=
      EMSCRIPTEN_RESULT_SUCCESS) {
    emscripten_webgl_destroy_context(surface->context);
    surface->context = 0;
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  puglEmscriptenGlUpdateHints(view, surface->webglVersion);
  (void)emscripten_webgl_make_context_current(0);
  return PUGL_SUCCESS;
}

static void
puglEmscriptenGlDestroy(PuglView* const view)
{
  PuglEmscriptenGlSurface* const surface =
    (PuglEmscriptenGlSurface*)view->impl->surface;

  if (surface) {
    if (surface->context > 0) {
      if (emscripten_webgl_get_current_context() == surface->context) {
        (void)emscripten_webgl_make_context_current(0);
      }
      emscripten_webgl_destroy_context(surface->context);
    }

    free(surface);
    view->impl->surface = NULL;
  }
}

PUGL_WARN_UNUSED_RESULT static PuglStatus
puglEmscriptenGlEnter(PuglView* const view,
                      const PuglExposeEvent* PUGL_UNUSED(expose))
{
  const PuglEmscriptenGlSurface* const surface =
    (const PuglEmscriptenGlSurface*)view->impl->surface;

  return surface && surface->context > 0 &&
             emscripten_webgl_make_context_current(surface->context) ==
               EMSCRIPTEN_RESULT_SUCCESS
           ? PUGL_SUCCESS
           : PUGL_FAILURE;
}

PUGL_WARN_UNUSED_RESULT static PuglStatus
puglEmscriptenGlLeave(PuglView* const view,
                      const PuglExposeEvent* PUGL_UNUSED(expose))
{
  (void)view;
  return emscripten_webgl_make_context_current(0) == EMSCRIPTEN_RESULT_SUCCESS
           ? PUGL_SUCCESS
           : PUGL_FAILURE;
}

PuglGlFunc
puglGetProcAddress(const char* const name)
{
  return (PuglGlFunc)emscripten_webgl_get_proc_address(name);
}

PuglStatus
puglEnterContext(PuglView* const view)
{
  return view->backend->enter(view, NULL);
}

PuglStatus
puglLeaveContext(PuglView* const view)
{
  return view->backend->leave(view, NULL);
}

const PuglBackend*
puglGlBackend(void)
{
  static const PuglBackend backend = {puglEmscriptenGlConfigure,
                                      puglEmscriptenGlCreate,
                                      puglEmscriptenGlDestroy,
                                      puglEmscriptenGlEnter,
                                      puglEmscriptenGlLeave,
                                      puglStubGetContext};
  return &backend;
}
