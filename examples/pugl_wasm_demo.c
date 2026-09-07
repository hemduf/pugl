// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include <pugl/gl.h>
#include <pugl/pugl.h>

#include <emscripten.h>

#include <stdbool.h>
#include <stdlib.h>

typedef struct {
  PuglWorld* world;
  PuglView*  view;
  unsigned   keyPress;
  unsigned   buttonPress;
  unsigned   buttonRelease;
  unsigned   motion;
  unsigned   configure;
  unsigned   expose;
  bool       pixelOk;
  bool       failed;
} Demo;

static Demo demo;

EM_JS(void,
      publishDemoState,
      (unsigned keyPress,
       unsigned buttonPress,
       unsigned buttonRelease,
       unsigned motion,
       unsigned configure,
       unsigned expose,
       int pixelOk,
       int failed),
      {
        window.puglDemo = {
          keyPress,
          buttonPress,
          buttonRelease,
          motion,
          configure,
          expose,
          pixelOk: !!pixelOk,
        };

        if (document.body) {
          document.body.dataset.puglReady = 'true';
          document.body.dataset.puglTest = failed
            ? 'fail'
            : (expose > 0 && pixelOk ? 'pass' : 'pending');
        }
      });

static bool
nearByte(const unsigned char actual, const float expected)
{
  const int target = (int)(expected * 255.0F + 0.5F);
  return abs((int)actual - target) <= 2;
}

static void
requestRedraw(PuglView* const view)
{
  if (puglObscureView(view)) {
    demo.failed = true;
  }
}

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
  switch (event->type) {
  case PUGL_KEY_PRESS:
    ++demo.keyPress;
    requestRedraw(view);
    break;
  case PUGL_BUTTON_PRESS:
    ++demo.buttonPress;
    requestRedraw(view);
    break;
  case PUGL_BUTTON_RELEASE:
    ++demo.buttonRelease;
    requestRedraw(view);
    break;
  case PUGL_MOTION:
    ++demo.motion;
    requestRedraw(view);
    break;
  case PUGL_CONFIGURE:
    ++demo.configure;
    requestRedraw(view);
    break;
  case PUGL_EXPOSE: {
    ++demo.expose;

    const float red = 0.15F + 0.05F * (float)(demo.keyPress % 5U);
    const float green = 0.25F + 0.05F * (float)(demo.buttonPress % 5U);
    const float blue = 0.45F + 0.03F * (float)(demo.motion % 5U);

    glViewport(0, 0, event->expose.width, event->expose.height);
    glClearColor(red, green, blue, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    unsigned char pixel[4] = {0U, 0U, 0U, 0U};
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    demo.pixelOk = nearByte(pixel[0], red) && nearByte(pixel[1], green) &&
                   nearByte(pixel[2], blue) && pixel[3] == 255U &&
                   glGetError() == GL_NO_ERROR;
    if (!demo.pixelOk) {
      demo.failed = true;
      return PUGL_FAILURE;
    }
    break;
  }
  default:
    break;
  }

  return PUGL_SUCCESS;
}

static void
runFrame(void* const data)
{
  Demo* const state = (Demo*)data;
  if (!state || !state->world || puglUpdate(state->world, 0.0)) {
    demo.failed = true;
  }

  publishDemoState(demo.keyPress,
                   demo.buttonPress,
                   demo.buttonRelease,
                   demo.motion,
                   demo.configure,
                   demo.expose,
                   demo.pixelOk,
                   demo.failed);
}

int
main(void)
{
  demo.world = puglNewWorld(PUGL_PROGRAM, 0U);
  demo.view  = demo.world ? puglNewView(demo.world) : NULL;
  if (!demo.world || !demo.view) {
    demo.failed = true;
    publishDemoState(0U, 0U, 0U, 0U, 0U, 0U, false, true);
    return 1;
  }

  puglSetBackend(demo.view, puglGlBackend());
  puglSetEventFunc(demo.view, onEvent);
  puglSetViewHint(demo.view, PUGL_CONTEXT_API, PUGL_OPENGL_ES_API);
  puglSetViewHint(demo.view, PUGL_CONTEXT_VERSION_MAJOR, 2);
  puglSetViewHint(demo.view, PUGL_CONTEXT_VERSION_MINOR, 0);
  puglSetViewHint(demo.view, PUGL_ALPHA_BITS, 8);
  puglSetViewHint(demo.view, PUGL_RESIZABLE, PUGL_TRUE);

  if (puglSetSizeHint(demo.view, PUGL_DEFAULT_SIZE, 640U, 360U) ||
      puglShow(demo.view, PUGL_SHOW_PASSIVE)) {
    demo.failed = true;
    publishDemoState(0U, 0U, 0U, 0U, 0U, 0U, false, true);
    return 2;
  }

  publishDemoState(0U, 0U, 0U, 0U, 0U, 0U, false, false);
  emscripten_set_main_loop_arg(runFrame, &demo, 0, true);
  return 0;
}
