// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

// Tests native Win32 WM_DROPFILES URI-list delivery

#undef NDEBUG

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct {
  DWORD pFiles;
  POINT pt;
  BOOL  fNC;
  BOOL  fWide;
} TestDropFiles;

typedef struct {
  unsigned offers;
  unsigned dataEvents;
  bool     accept;
  double   x;
  double   y;
  char*    payload;
  size_t   payloadLen;
} TestState;

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
  TestState* const state = (TestState*)puglGetHandle(view);

  if (event->type == PUGL_DATA_OFFER &&
      event->offer.clipboard == PUGL_CLIPBOARD_DRAG) {
    ++state->offers;

    assert(event->offer.x == 17.0);
    assert(event->offer.y == 29.0);
    assert(puglGetNumClipboardTypes(view, event->offer.clipboard) == 1U);
    const char* const type = puglGetClipboardType(view, event->offer.clipboard, 0U);
    assert(type);
    assert(!strcmp(type, "text/uri-list"));

    const unsigned dataEventsBeforeDecision = state->dataEvents;
    const PuglStatus status =
      state->accept
        ? puglAcceptOffer(view,
                          &event->offer,
                          0U,
                          PUGL_DATA_ACTION_COPY,
                          101,
                          203,
                          7U,
                          11U)
        : puglRejectOffer(view, &event->offer, 101, 203, 7U, 11U);

    // Accepting an offer only records the decision.  PUGL_DATA must be
    // dispatched after this callback returns, not re-entrantly from accept.
    assert(state->dataEvents == dataEventsBeforeDecision);
    return status;
  }

  if (event->type != PUGL_DATA || event->data.clipboard != PUGL_CLIPBOARD_DRAG) {
    return PUGL_SUCCESS;
  }

  ++state->dataEvents;
  state->x = event->data.x;
  state->y = event->data.y;

  const char* const type =
    puglGetClipboardType(view, event->data.clipboard, event->data.typeIndex);
  assert(type);
  assert(!strcmp(type, "text/uri-list"));

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
  return PUGL_SUCCESS;
}

static char*
fileUrlUtf8(const wchar_t* const path)
{
  wchar_t url[2048] = {0};
  DWORD   urlLen    = (DWORD)(sizeof(url) / sizeof(url[0]));
  assert(SUCCEEDED(UrlCreateFromPathW(path, url, &urlLen, 0)));

  const int utf8Len =
    WideCharToMultiByte(CP_UTF8, 0, url, -1, NULL, 0, NULL, NULL);
  assert(utf8Len > 0);

  char* const result = (char*)calloc((size_t)utf8Len, 1U);
  assert(result);
  assert(WideCharToMultiByte(
           CP_UTF8, 0, url, -1, result, utf8Len, NULL, NULL) == utf8Len);
  return result;
}

static HDROP
makeDrop(const wchar_t* const first, const wchar_t* const second)
{
  const size_t firstLen  = wcslen(first) + 1U;
  const size_t secondLen = wcslen(second) + 1U;
  const size_t chars     = firstLen + secondLen + 1U;
  const size_t bytes     = sizeof(TestDropFiles) + chars * sizeof(wchar_t);

  HGLOBAL const memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
  assert(memory);

  TestDropFiles* const drop = (TestDropFiles*)GlobalLock(memory);
  assert(drop);
  drop->pFiles = sizeof(TestDropFiles);
  drop->pt.x   = 17;
  drop->pt.y   = 29;
  drop->fNC    = FALSE;
  drop->fWide  = TRUE;

  wchar_t* const files = (wchar_t*)((unsigned char*)drop + drop->pFiles);
  memcpy(files, first, firstLen * sizeof(wchar_t));
  memcpy(files + firstLen, second, secondLen * sizeof(wchar_t));
  files[firstLen + secondLen] = L'\0';

  GlobalUnlock(memory);
  return (HDROP)memory;
}

int
main(void)
{
  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglView* const  view  = puglNewView(world);
  assert(world);
  assert(view);

  TestState state = {0U, 0U, true, 0.0, 0.0, NULL, 0U};
  puglSetWorldString(world, PUGL_CLASS_NAME, "PuglWinDropTest");
  puglSetViewString(view, PUGL_WINDOW_TITLE, "Pugl Win Drop Test");
  puglSetBackend(view, puglStubBackend());
  puglSetHandle(view, &state);
  puglSetEventFunc(view, onEvent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 256U, 256U);
  puglSetViewHint(view, PUGL_ACCEPT_DROP, PUGL_TRUE);
  assert(!puglRegisterDropType(view, "text/uri-list"));
  assert(!puglRealize(view));

  const wchar_t first[]  = L"C:\\tmp\\alpha.wav";
  const wchar_t second[] = L"C:\\tmp\\caf\x00E9.wav";
  char* const firstUrl   = fileUrlUtf8(first);
  char* const secondUrl  = fileUrlUtf8(second);

  const size_t firstLen    = strlen(firstUrl);
  const size_t secondLen   = strlen(secondUrl);
  const size_t expectedLen = firstLen + 1U + secondLen + 1U;
  char* const  expected    = (char*)calloc(expectedLen + 1U, 1U);
  assert(expected);
  memcpy(expected, firstUrl, firstLen);
  expected[firstLen] = '\n';
  memcpy(expected + firstLen + 1U, secondUrl, secondLen);
  expected[expectedLen - 1U] = '\n';

  HWND const hwnd = (HWND)puglGetNativeView(view);
  assert(hwnd);

  SendMessage(hwnd, WM_DROPFILES, (WPARAM)makeDrop(first, second), 0);

  assert(state.offers == 1U);
  assert(state.dataEvents == 1U);
  assert(state.x == 17.0);
  assert(state.y == 29.0);
  assert(state.payload);
  assert(state.payloadLen == expectedLen);
  assert(strlen(state.payload) == expectedLen);
  assert(!memcmp(state.payload, expected, expectedLen));

  state.accept = false;
  SendMessage(hwnd, WM_DROPFILES, (WPARAM)makeDrop(first, second), 0);

  assert(state.offers == 2U);
  assert(state.dataEvents == 1U);
  assert(state.payloadLen == expectedLen);
  assert(!memcmp(state.payload, expected, expectedLen));

  free(expected);
  free(firstUrl);
  free(secondUrl);
  free(state.payload);
  puglFreeView(view);
  puglFreeWorld(world);
  return 0;
}
