// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char clipboardText[] = "pugl-browser-clipboard";

typedef struct {
  PuglWorld* world;
  PuglView*  view;
  unsigned   clientEvents;
  unsigned   timer7Events;
  unsigned   timer9Events;
  unsigned   teardownTimerEvents;
  unsigned   timer7AtFailedRestart;
  unsigned   timer7AtStop;
  unsigned   timer9AtStop;
  unsigned   clipboardOffers;
  unsigned   clipboardDataEvents;
  unsigned   clipboardWritePolls;
  bool       clientOrderOk;
  bool       timer7ReplacementFailureOk;
  bool       timer7Restarted;
  bool       clipboardOfferOk;
  bool       clipboardDataOk;
  bool       clipboardSecurityOk;
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

static bool
exerciseClipboardSecurityPaths(PuglView* const view)
{
  const int unavailable = emscripten_run_script_int(
    "(()=>{const c=typeof navigator!=='undefined'?navigator.clipboard:null;"
    "if(!c)return 0;try{"
    "Object.defineProperty(c,'writeText',{configurable:true,value:undefined});"
    "Object.defineProperty(c,'readText',{configurable:true,value:undefined});"
    "}catch(_){return 0;}"
    "return typeof c.writeText==='undefined'&&typeof c.readText==='undefined';"
    "})()");

  if (!unavailable ||
      puglSetClipboard(view,
                       PUGL_CLIPBOARD_GENERAL,
                       "text/plain",
                       clipboardText,
                       strlen(clipboardText)) != PUGL_UNSUPPORTED ||
      puglPaste(view) != PUGL_UNSUPPORTED) {
    return false;
  }

  size_t      len  = 0U;
  const void* data =
    puglGetClipboard(view, PUGL_CLIPBOARD_GENERAL, 0U, &len);
  if (!data || len != strlen(clipboardText) ||
      memcmp(data, clipboardText, len)) {
    return false;
  }

  const int denied = emscripten_run_script_int(
    "(()=>{const c=typeof navigator!=='undefined'?navigator.clipboard:null;"
    "if(!c)return 0;try{"
    "Object.defineProperty(c,'writeText',{configurable:true,value:()=>"
    "Promise.reject(new Error('clipboard write denied'))});"
    "Object.defineProperty(c,'readText',{configurable:true,value:()=>"
    "Promise.reject(new Error('clipboard read denied'))});"
    "}catch(_){return 0;}"
    "return typeof c.writeText==='function'&&typeof c.readText==='function';"
    "})()");

  return denied &&
         puglSetClipboard(view,
                          PUGL_CLIPBOARD_GENERAL,
                          "text/plain",
                          clipboardText,
                          strlen(clipboardText)) == PUGL_SUCCESS &&
         puglPaste(view) == PUGL_SUCCESS;
}

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
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
    } else if (event->timer.id == 13U) {
      ++state.teardownTimerEvents;
    } else {
      state.clientOrderOk = false;
    }
  } else if (event->type == PUGL_DATA_OFFER) {
    ++state.clipboardOffers;
    const char* const type =
      puglGetClipboardType(view, PUGL_CLIPBOARD_GENERAL, 0U);
    state.clipboardOfferOk =
      state.clipboardOffers == 1U &&
      event->offer.clipboard == PUGL_CLIPBOARD_GENERAL &&
      puglGetNumClipboardTypes(view, PUGL_CLIPBOARD_GENERAL) == 1U && type &&
      !strcmp(type, "text/plain") &&
      !puglAcceptOffer(view,
                       &event->offer,
                       0U,
                       PUGL_DATA_ACTION_COPY,
                       0,
                       0,
                       64U,
                       64U);
  } else if (event->type == PUGL_DATA) {
    ++state.clipboardDataEvents;
    size_t      len  = 0U;
    const void* data =
      puglGetClipboard(view, PUGL_CLIPBOARD_GENERAL, 0U, &len);
    state.clipboardDataOk =
      state.clipboardDataEvents == 1U &&
      event->data.clipboard == PUGL_CLIPBOARD_GENERAL &&
      event->data.typeIndex == 0U && data && len == strlen(clipboardText) &&
      !memcmp(data, clipboardText, len);

    if (state.clipboardDataEvents == 1U) {
      state.clipboardSecurityOk = exerciseClipboardSecurityPaths(view);
    }
  }

  return PUGL_SUCCESS;
}

static bool
startAndDestroyTimerView(void)
{
  PuglView* const view = puglNewView(state.world);
  if (!view) {
    return false;
  }

  puglSetBackend(view, puglStubBackend());
  puglSetEventFunc(view, onEvent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 32U, 32U);

  const bool started = !puglShow(view, PUGL_SHOW_PASSIVE) &&
                       !puglStartTimer(view, 13U, 0.010);
  if (!started || puglUnrealize(view)) {
    puglFreeView(view);
    return false;
  }

  puglFreeView(view);
  return true;
}

static bool
startAndDestroyPasteView(void)
{
  PuglView* const view = puglNewView(state.world);
  if (!view) {
    return false;
  }

  puglSetBackend(view, puglStubBackend());
  puglSetEventFunc(view, onEvent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 32U, 32U);

  const bool started = !puglShow(view, PUGL_SHOW_PASSIVE) && !puglPaste(view);
  if (!started || puglUnrealize(view)) {
    puglFreeView(view);
    return false;
  }

  puglFreeView(view);
  return true;
}

static bool
verifyStoredClipboard(const char* const expected, const size_t expectedLen)
{
  if (puglGetNumClipboardTypes(state.view, PUGL_CLIPBOARD_GENERAL) != 1U ||
      puglGetNumClipboardTypes(state.view, PUGL_CLIPBOARD_DRAG) != 0U) {
    return false;
  }

  const char* const type =
    puglGetClipboardType(state.view, PUGL_CLIPBOARD_GENERAL, 0U);
  if (!type || strcmp(type, "text/plain") ||
      puglGetClipboardType(state.view, PUGL_CLIPBOARD_GENERAL, 1U) ||
      puglGetClipboardType(state.view, PUGL_CLIPBOARD_DRAG, 0U)) {
    return false;
  }

  size_t      len  = 123U;
  const void* data =
    puglGetClipboard(state.view, PUGL_CLIPBOARD_GENERAL, 0U, &len);
  if (!data || len != expectedLen || memcmp(data, expected, expectedLen)) {
    return false;
  }

  len = 123U;
  if (puglGetClipboard(state.view, PUGL_CLIPBOARD_GENERAL, 1U, &len) || len) {
    return false;
  }

  len = 123U;
  return !puglGetClipboard(state.view, PUGL_CLIPBOARD_DRAG, 0U, &len) && !len;
}

static void
requestPaste(void* const data)
{
  (void)data;

  if (!state.view) {
    return;
  }

  const int writeState = emscripten_run_script_int(
    "document.body.dataset.puglClipboardWrite==='success'?1:"
    "document.body.dataset.puglClipboardWrite==='denied'?-1:0");

  if (writeState == 1) {
    if (puglPaste(state.view) != PUGL_SUCCESS) {
      fprintf(stderr, "Browser clipboard paste contract is not implemented\n");
      finish(false);
    }
    return;
  }

  if (writeState < 0 || ++state.clipboardWritePolls >= 50U) {
    fprintf(stderr, "Browser clipboard write did not become readable\n");
    finish(false);
    return;
  }

  emscripten_set_timeout(requestPaste, 10.0, NULL);
}

static void
failTimer7Replacement(void* const data)
{
  (void)data;

  if (!state.view || state.timer7Events < 2U) {
    finish(false);
    return;
  }

  state.timer7AtFailedRestart = state.timer7Events;
  const int injected = emscripten_run_script_int(
    "(()=>{if(typeof globalThis==='undefined'||"
    "typeof globalThis.setInterval!=='function')return 0;"
    "globalThis.__puglOriginalSetInterval=globalThis.setInterval;"
    "globalThis.setInterval=()=>0;return 1;})()");

  const PuglStatus status =
    injected ? puglStartTimer(state.view, 7U, 0.005) : PUGL_FAILURE;

  emscripten_run_script(
    "(()=>{if(globalThis.__puglOriginalSetInterval){"
    "globalThis.setInterval=globalThis.__puglOriginalSetInterval;"
    "delete globalThis.__puglOriginalSetInterval;}})()");

  state.timer7ReplacementFailureOk =
    injected && status == PUGL_UNKNOWN_ERROR;
  if (!state.timer7ReplacementFailureOk) {
    fprintf(stderr, "Failed timer replacement was not reported correctly\n");
    finish(false);
  }
}

static void
restartTimer7(void* const data)
{
  (void)data;

  state.timer7AtStop = state.timer7Events;
  if (!state.view || !state.timer7ReplacementFailureOk ||
      state.timer7AtStop <= state.timer7AtFailedRestart ||
      puglStopTimer(state.view, 7U) || puglStartTimer(state.view, 7U, 0.015)) {
    fprintf(stderr,
            "Existing timer did not survive failed replacement: before=%u after=%u\n",
            state.timer7AtFailedRestart,
            state.timer7AtStop);
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

  const int writeDenied = emscripten_run_script_int(
    "document.body.dataset.puglClipboardWrite==='denied'");
  const int readDenied = emscripten_run_script_int(
    "document.body.dataset.puglClipboardRead==='denied'");

  const bool pass =
    state.view && state.clientEvents == 1U && state.clientOrderOk &&
    state.timer7ReplacementFailureOk && state.timer7Restarted &&
    state.timer7Events > state.timer7AtStop && state.timer9AtStop >= 3U &&
    state.timer9Events == state.timer9AtStop &&
    state.teardownTimerEvents == 0U && state.clipboardOffers == 1U &&
    state.clipboardDataEvents == 1U && state.clipboardOfferOk &&
    state.clipboardDataOk && state.clipboardSecurityOk && writeDenied &&
    readDenied;

  if (!pass) {
    fprintf(stderr,
            "Service checks failed: client=%u order=%d timer7=%u failed7=%u "
            "stop7=%u failOk=%d restart=%d timer9=%u stop9=%u teardown=%u "
            "offers=%u data=%u offerOk=%d dataOk=%d securityOk=%d "
            "writeDenied=%d readDenied=%d\n",
            state.clientEvents,
            state.clientOrderOk,
            state.timer7Events,
            state.timer7AtFailedRestart,
            state.timer7AtStop,
            state.timer7ReplacementFailureOk,
            state.timer7Restarted,
            state.timer9Events,
            state.timer9AtStop,
            state.teardownTimerEvents,
            state.clipboardOffers,
            state.clipboardDataEvents,
            state.clipboardOfferOk,
            state.clipboardDataOk,
            state.clipboardSecurityOk,
            writeDenied,
            readDenied);
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

  if (puglSetClipboard(state.view,
                       PUGL_CLIPBOARD_GENERAL,
                       NULL,
                       clipboardText,
                       strlen(clipboardText)) != PUGL_SUCCESS) {
    fprintf(stderr, "Browser clipboard default MIME contract is not implemented\n");
    finish(false);
    return 0;
  }

  if (!verifyStoredClipboard(clipboardText, strlen(clipboardText))) {
    fprintf(stderr, "Browser clipboard query contract is not implemented\n");
    finish(false);
    return 0;
  }

  PuglEvent client = {0};
  client.client.type  = PUGL_CLIENT;
  client.client.flags = PUGL_IS_SEND_EVENT;
  client.client.data1 = 0x1234U;
  client.client.data2 = 0x5678U;

  if (puglSendEvent(state.view, &client) || state.clientEvents != 1U ||
      !state.clientOrderOk || !puglStopTimer(state.view, 99U) ||
      puglStartTimer(state.view, 7U, 0.020) ||
      puglStartTimer(state.view, 9U, 0.025) || !startAndDestroyTimerView() ||
      !startAndDestroyPasteView()) {
    finish(false);
    return 0;
  }

  emscripten_set_timeout(requestPaste, 0.0, NULL);
  emscripten_set_timeout(failTimer7Replacement, 70.0, NULL);
  emscripten_set_timeout(stopTimer9, 100.0, NULL);
  emscripten_set_timeout(restartTimer7, 150.0, NULL);
  emscripten_set_timeout(finishServices, 600.0, NULL);
  return 0;
}
