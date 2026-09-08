#!/usr/bin/env python3
from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"{label}: expected text not found")
    return text.replace(old, new, 1)


win_path = Path("src/win.c")
win = win_path.read_text()

old_uri = r'''#ifdef UNICODE
            const int len =
              WideCharToMultiByte(CP_UTF8, 0, url, -1, NULL, 0, NULL, NULL);

            impl->droppedUris = (char*)realloc(
              impl->droppedUris, impl->droppedUrisLen + (size_t)len + 2U);

            char* const end = impl->droppedUris + impl->droppedUrisLen;
            WideCharToMultiByte(CP_UTF8, 0, url, -1, end, len, NULL, NULL);
#else
            impl->droppedUris = (char*)realloc(
              impl->droppedUris, impl->droppedUrisLen + urlLen + 2);

            memcpy(impl->droppedUris + impl->droppedUrisLen, url, urlLen + 1);
#endif

            impl->droppedUrisLen += urlLen;
            impl->droppedUris[impl->droppedUrisLen++] = '\n';
            impl->droppedUris[impl->droppedUrisLen]   = 0;
'''

new_uri = r'''#ifdef UNICODE
            const int utf8Len =
              WideCharToMultiByte(CP_UTF8, 0, url, -1, NULL, 0, NULL, NULL);
            if (utf8Len <= 0) {
              continue;
            }

            const size_t uriLen = (size_t)utf8Len - 1U;
            char* const resized = (char*)realloc(
              impl->droppedUris, impl->droppedUrisLen + uriLen + 2U);
            if (!resized) {
              continue;
            }

            impl->droppedUris = resized;
            char* const end   = impl->droppedUris + impl->droppedUrisLen;
            if (!WideCharToMultiByte(
                  CP_UTF8, 0, url, -1, end, utf8Len, NULL, NULL)) {
              continue;
            }
#else
            const size_t uriLen = (size_t)urlLen;
            char* const resized = (char*)realloc(
              impl->droppedUris, impl->droppedUrisLen + uriLen + 2U);
            if (!resized) {
              continue;
            }

            impl->droppedUris = resized;
            memcpy(impl->droppedUris + impl->droppedUrisLen, url, uriLen + 1U);
#endif

            impl->droppedUrisLen += uriLen;
            impl->droppedUris[impl->droppedUrisLen++] = '\n';
            impl->droppedUris[impl->droppedUrisLen]   = 0;
'''

win = replace_once(win, old_uri, new_uri, "WM_DROPFILES UTF-8 accounting")

anchor = '''  PuglEvent dataEvent;\n  dataEvent.data = data;\n  return puglDispatchEvent(view, &dataEvent);\n}\n\nconst void*\npuglGetClipboard'''
replacement = '''  PuglEvent dataEvent;\n  dataEvent.data = data;\n  return puglDispatchEvent(view, &dataEvent);\n}\n\nPuglStatus\npuglRejectOffer(PuglView* const                 view,\n                const PuglDataOfferEvent* const offer,\n                const int                       regionX,\n                const int                       regionY,\n                const unsigned                  regionWidth,\n                const unsigned                  regionHeight)\n{\n  (void)view;\n  (void)regionX;\n  (void)regionY;\n  (void)regionWidth;\n  (void)regionHeight;\n\n  if (!offer) {\n    return PUGL_BAD_PARAMETER;\n  }\n\n  switch (offer->clipboard) {\n  case PUGL_CLIPBOARD_GENERAL:\n  case PUGL_CLIPBOARD_DRAG:\n    // WM_DROPFILES has no rejectable pre-drop offer.  Explicit rejection is\n    // therefore a successful no-op, matching the portable consumer behavior.\n    return PUGL_SUCCESS;\n  }\n\n  return PUGL_BAD_PARAMETER;\n}\n\nconst void*\npuglGetClipboard'''
win = replace_once(win, anchor, replacement, "Windows puglRejectOffer insertion")
win_path.write_text(win)

bad_path = Path("test/test_bad_call.c")
bad = bad_path.read_text()
bad = replace_once(
    bad,
    '''  // Win32 uses WM_DROPFILES, so only general clipboard offers can currently be\n  // explicitly rejected before data delivery.  Drag offer rejection is not\n  // supported until the backend gains a negotiable pre-drop protocol.\n''',
    '''  // Win32 uses WM_DROPFILES, so explicit drag rejection is a successful\n  // no-op.  Rejection must never dispatch data.\n''',
    "bad_call Windows comment",
)
bad = replace_once(
    bad,
    '''  const PuglDataOfferEvent drag_offer = {\n    PUGL_DATA_OFFER, 0U, 0.0, 0.0, 0.0, PUGL_CLIPBOARD_DRAG};\n''',
    '''  const PuglDataOfferEvent drag_offer = {\n    PUGL_DATA_OFFER, 0U, 0.0, 0.0, 0.0, PUGL_CLIPBOARD_DRAG};\n  const PuglDataOfferEvent invalid_offer = {\n    PUGL_DATA_OFFER, 0U, 0.0, 0.0, 0.0, (PuglClipboard)-1};\n''',
    "bad_call invalid clipboard fixture",
)
bad = replace_once(
    bad,
    '''  assert(puglRejectOffer(test.view, &drag_offer, 0, 0, 1U, 1U) ==\n         PUGL_UNSUPPORTED);\n  assert(test.dataEvents == dataEventsBeforeReject);\n''',
    '''  assert(puglRejectOffer(test.view, &drag_offer, 0, 0, 1U, 1U) ==\n         PUGL_SUCCESS);\n  assert(test.dataEvents == dataEventsBeforeReject);\n  assert(puglRejectOffer(test.view, &invalid_offer, 0, 0, 1U, 1U) ==\n         PUGL_BAD_PARAMETER);\n  assert(test.dataEvents == dataEventsBeforeReject);\n''',
    "bad_call drag rejection result",
)
bad_path.write_text(bad)

win_test = r'''// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

// Tests native Win32 WM_DROPFILES URI-list delivery

#undef NDEBUG

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <shellapi.h>
#include <shlwapi.h>
#include <windows.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct {
  unsigned dataEvents;
  double   x;
  double   y;
  char*    payload;
  size_t   payloadLen;
} TestState;

static PuglStatus
onEvent(PuglView* const view, const PuglEvent* const event)
{
  TestState* const state = (TestState*)puglGetHandle(view);
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
  const size_t bytes     = sizeof(DROPFILES) + chars * sizeof(wchar_t);

  HGLOBAL const memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
  assert(memory);

  DROPFILES* const drop = (DROPFILES*)GlobalLock(memory);
  assert(drop);
  drop->pFiles = sizeof(DROPFILES);
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

  TestState state = {0U, 0.0, 0.0, NULL, 0U};
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

  assert(state.dataEvents == 1U);
  assert(state.x == 17.0);
  assert(state.y == 29.0);
  assert(state.payload);
  assert(state.payloadLen == expectedLen);
  assert(strlen(state.payload) == expectedLen);
  assert(!memcmp(state.payload, expected, expectedLen));

  free(expected);
  free(firstUrl);
  free(secondUrl);
  free(state.payload);
  puglFreeView(view);
  puglFreeWorld(world);
  return 0;
}
'''
Path("test/test_win_drop.c").write_text(win_test)

meson_path = Path("test/meson.build")
meson = meson_path.read_text()
anchor = "# Tests that need an OpenGL backend\n"
win_block = '''# Win32 native drag-and-drop regression
if platform == 'win'
  test(
    'win_drop',
    executable(
      'test_win_drop',
      'test_win_drop.c',
      c_args: test_c_args + platform_args,
      dependencies: [pugl_dep, pugl_stub_dep, shlwapi_dep],
      implicit_include_directories: false,
    ),
    suite: 'unit',
  )
endif

'''
meson = replace_once(meson, anchor, win_block + anchor, "Meson Windows test insertion")
meson_path.write_text(meson)
