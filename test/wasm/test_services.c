// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
  PuglWorld* world;
  PuglView*  view;
  unsigned   clientEvents;
  unsigned   timer7Events;
  unsigned   timer9Events;
  unsigned   timer7AtStop;
  unsigned   timer9AtStop;
  bool       clientOrderOk;
  bool       timer7Restarted;
} TestState;

static TestState state = {0};

static void
setResult(const bool pass)
{
  emscripten_run_script(
    pass ? "document.body.dataset.puglTest='pass';"
           "document.body.dataset.puglReady='true';"
         : "document.body.dataset.puglTest='fail';"
           "document.body.dataset.puglReady='true';");
}

static void
finish(const bool pass)
{
  if (state.view) {
    (void)puglStopTimer(state.view, 7U);
    (void)puglStopTimer(state.view, 9U);
    puglFreeView(state.view);
    state.view = NULL;
  }

  if (state.world) {
    puglFreeWorld(state.world);
    state.world = NULL;
  }

  setResult(pass);
}

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
  (void)view;

  if (event->type == PUGL_CLIENT) {
    ++state.clientEvents;
    state.clientOrderOk =
      state.clientEvents == 1U && state.timer7Events == 0U &&
      state.timer9Events == 0U && event->client.data1 == 0x1234U &&
      event->client.data2 == 0x5678U;
  } else if (event->type == PUGL_TIMER) {
    state.clientOrderOk = state.clientOrderOk && state.clientEvents == 1U;
    if (event->timer.id == 7U) {
      ++state.timer7Events;
    } else if (event->timer.id == 9U) {
      ++state.timer9Events;
    } else {
      state.clientOrderOk = false;
    }
  }

  return PUGL_SUCCESS;
}

static void
restartTimer7(void* const data)
{
  (void)data;

  state.timer7AtStop = state.timer7Events;
  if (!state.view || state.timer7AtStop < 2U || puglStopTimer(state.view, 7U) ||
      puglStartTimer(state.view, 7U, 0.015)) {
    finish(false);
    return;
  }

  state.timer7Restarted = true;
}

static void
stopTimer9(void* const data)
{
  (void)data;

  state.timer9AtStop = state.timer9Events;
  if (!state.view || state.timer9AtStop < 3U || puglStopTimer(state.view, 9U)) {
    finish(false);
  }
}

static void
finishServices(void* const data)
{
  (void)data;

  const bool pass =
    state.view && state.clientEvents == 1U && state.clientOrderOk &&
    state.timer7Restarted && state.timer7Events > state.timer7AtStop &&
    state.timer9AtStop >= 3U && state.timer9Events == state.timer9AtStop;

  if (!pass) {
    fprintf(stderr,
            "Service checks failed: client=%u order=%d timer7=%u stop7=%u "
            "restart=%d timer9=%u stop9=%u\n",
            state.clientEvents,
            state.clientOrderOk,
            state.timer7Events,
            state.timer7AtStop,
            state.timer7Restarted,
            state.timer9Events,
            state.timer9AtStop);
  }

  finish(pass);
}

int
main(void)
{
  state.world = puglNewWorld(PUGL_PROGRAM, 0U);
  state.view  = state.world ? puglNewView(state.world) : NULL;
  if (!state.world || !state.view) {
    finish(false);
    return 0;
  }

  puglSetBackend(state.view, puglStubBackend());
  puglSetEventFunc(state.view, onEvent);
  puglSetSizeHint(state.view, PUGL_DEFAULT_SIZE, 64U, 64U);

  if (puglShow(state.view, PUGL_SHOW_PASSIVE)) {
    finish(false);
    return 0;
  }

  PuglEvent client = {0};
  client.client.type  = PUGL_CLIENT;
  client.client.flags = PUGL_IS_SEND_EVENT;
  client.client.data1 = 0x1234U;
  client.client.data2 = 0x5678U;

  if (puglSendEvent(state.view, &client) || state.clientEvents != 1U ||
      !state.clientOrderOk || puglStartTimer(state.view, 7U, 0.020) ||
      puglStartTimer(state.view, 9U, 0.025)) {
    finish(false);
    return 0;
  }

  emscripten_set_timeout(restartTimer7, 70.0, NULL);
  emscripten_set_timeout(stopTimer9, 100.0, NULL);
  emscripten_set_timeout(finishServices, 170.0, NULL);
  return 0;
}
