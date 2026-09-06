// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

EM_JS(int,
      puglTestCursorEquals,
      (uintptr_t nativeView, const char* expected), {
  const element = document.getElementById(`pugl-view-${nativeView}`);
  return !!element &&
         getComputedStyle(element).cursor === UTF8ToString(expected);
});

EM_JS(int,
      puglTestCanvasIsViewportCentered,
      (uintptr_t nativeView, unsigned width, unsigned height), {
  const element = document.getElementById(`pugl-view-${nativeView}`);
  if (!element || typeof window === 'undefined') {
    return 0;
  }

  const left = Number.parseInt(element.style.left, 10);
  const top = Number.parseInt(element.style.top, 10);
  const expectedLeft = Math.trunc(window.innerWidth / 2) - Math.trunc(width / 2);
  const expectedTop = Math.trunc(window.innerHeight / 2) - Math.trunc(height / 2);
  return left === expectedLeft && top === expectedTop;
});

EM_JS(int, puglTestCanvasIsResizable, (uintptr_t nativeView), {
  const element = document.getElementById(`pugl-view-${nativeView}`);
  if (!element) {
    return 0;
  }

  const style = getComputedStyle(element);
  return style.resize === 'both' && style.overflow !== 'visible';
});

EM_JS(int,
      puglTestCanvasHasSizeConstraints,
      (uintptr_t nativeView,
       unsigned minWidth,
       unsigned minHeight,
       unsigned maxWidth,
       unsigned maxHeight), {
  const element = document.getElementById(`pugl-view-${nativeView}`);
  if (!element) {
    return 0;
  }

  const style = getComputedStyle(element);
  return style.minWidth === `${minWidth}px` &&
         style.minHeight === `${minHeight}px` &&
         style.maxWidth === `${maxWidth}px` &&
         style.maxHeight === `${maxHeight}px`;
});

EM_JS(int, puglTestCreateHost, (uintptr_t nativeView), {
  const selector = `[data-pugl-native-view="${nativeView}"]`;
  if (!nativeView || document.querySelector(selector)) {
    return 0;
  }

  const host = document.createElement('div');
  host.id = `pugl-host-${nativeView}`;
  host.dataset.puglNativeView = String(nativeView);
  host.style.position = 'relative';
  document.body.appendChild(host);
  return 1;
});

EM_JS(int,
      puglTestNativeViewHasParent,
      (uintptr_t nativeView, uintptr_t parentView), {
  const element = document.getElementById(`pugl-view-${nativeView}`);
  const parent = document.querySelector(
    `[data-pugl-native-view="${parentView}"]`);
  return !!element && !!parent && element.parentElement === parent;
});

EM_JS(int, puglTestHostExists, (uintptr_t nativeView), {
  return !!document.querySelector(
    `[data-pugl-native-view="${nativeView}"]`);
});

EM_JS(void, puglTestDestroyHost, (uintptr_t nativeView), {
  const host = document.querySelector(
    `[data-pugl-native-view="${nativeView}"]`);
  if (host) {
    host.remove();
  }
});

static PuglStatus
puglTestIgnoreEvent(PuglView* const view, const PuglEvent* const event)
{
  (void)view;
  (void)event;
  return PUGL_SUCCESS;
}

static bool
puglTestBrowserCursorContract(void)
{
  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglView* const  view  = world ? puglNewView(world) : NULL;
  if (!world || !view) {
    fprintf(stderr, "Browser cursor test setup failed\n");
    if (view) {
      puglFreeView(view);
    }
    if (world) {
      puglFreeWorld(world);
    }
    return false;
  }

  puglSetBackend(view, puglStubBackend());
  puglSetEventFunc(view, puglTestIgnoreEvent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 32U, 32U);

  const bool realized = puglShow(view, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS;
  const PuglStatus cursorStatus =
    realized ? puglSetCursor(view, PUGL_CURSOR_CROSSHAIR) : PUGL_FAILURE;
  const bool cursorMatches =
    realized && cursorStatus == PUGL_SUCCESS &&
    puglTestCursorEquals(puglGetNativeView(view), "crosshair");

  if (realized) {
    (void)puglUnrealize(view);
  }
  puglFreeView(view);
  puglFreeWorld(world);

  if (!cursorMatches) {
    fprintf(stderr,
            "Browser cursor mapping contract is not implemented: status=%d\n",
            (int)cursorStatus);
  }

  return cursorMatches;
}

static bool
puglTestUnsupportedDesktopWindowContract(void)
{
  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglView* const  view  = world ? puglNewView(world) : NULL;
  if (!world || !view) {
    fprintf(stderr, "Browser desktop-window semantics test setup failed\n");
    if (view) {
      puglFreeView(view);
    }
    if (world) {
      puglFreeWorld(world);
    }
    return false;
  }

  puglSetBackend(view, puglStubBackend());
  puglSetEventFunc(view, puglTestIgnoreEvent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 32U, 32U);

  const PuglStatus transientStatus =
    puglSetTransientParent(view, (PuglNativeView)1234U);
  const bool realized = puglShow(view, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS;
  const PuglStatus positionStatus =
    realized
      ? puglSetPositionHint(view, PUGL_CURRENT_POSITION, 12, 34)
      : PUGL_FAILURE;

  if (realized) {
    (void)puglUnrealize(view);
  }
  puglFreeView(view);
  puglFreeWorld(world);

  const bool explicitSemantics =
    positionStatus == PUGL_UNSUPPORTED && transientStatus == PUGL_UNSUPPORTED;
  if (!explicitSemantics) {
    fprintf(stderr,
            "Browser desktop-only APIs must be explicit: position=%d transient=%d\n",
            (int)positionStatus,
            (int)transientStatus);
  }

  return explicitSemantics;
}

static bool
puglTestNativeViewIdentityContract(void)
{
  PuglWorld* const world0 = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglWorld* const world1 = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglView* const view0 = world0 ? puglNewView(world0) : NULL;
  PuglView* const view1 = world1 ? puglNewView(world1) : NULL;

  if (!world0 || !world1 || !view0 || !view1) {
    if (view0) {
      puglFreeView(view0);
    }
    if (view1) {
      puglFreeView(view1);
    }
    if (world0) {
      puglFreeWorld(world0);
    }
    if (world1) {
      puglFreeWorld(world1);
    }
    return false;
  }

  puglSetBackend(view0, puglStubBackend());
  puglSetBackend(view1, puglStubBackend());
  puglSetEventFunc(view0, puglTestIgnoreEvent);
  puglSetEventFunc(view1, puglTestIgnoreEvent);
  puglSetSizeHint(view0, PUGL_DEFAULT_SIZE, 16U, 16U);
  puglSetSizeHint(view1, PUGL_DEFAULT_SIZE, 16U, 16U);

  const bool realized0 = puglShow(view0, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS;
  const bool realized1 = puglShow(view1, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS;
  const PuglNativeView native0 = puglGetNativeView(view0);
  const PuglNativeView native1 = puglGetNativeView(view1);
  const bool unique = realized0 && realized1 && native0 && native1 &&
                      native0 != native1 &&
                      puglGetNativeView(view0) == native0 &&
                      puglGetNativeView(view1) == native1;

  if (realized0) {
    (void)puglUnrealize(view0);
  }
  if (realized1) {
    (void)puglUnrealize(view1);
  }
  puglFreeView(view0);
  puglFreeView(view1);
  puglFreeWorld(world0);
  puglFreeWorld(world1);

  if (!unique) {
    fprintf(stderr,
            "Browser native view handles must be globally unique: %lu vs %lu\n",
            (unsigned long)native0,
            (unsigned long)native1);
  }

  return unique;
}

static bool
puglTestBrowserEmbeddingContract(void)
{
  const PuglNativeView parent = (PuglNativeView)0x1F00DU;
  if (!puglTestCreateHost(parent)) {
    fprintf(stderr, "Failed to create browser embedding host\n");
    return false;
  }

  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglView* const  view  = world ? puglNewView(world) : NULL;
  if (!world || !view) {
    if (view) {
      puglFreeView(view);
    }
    if (world) {
      puglFreeWorld(world);
    }
    puglTestDestroyHost(parent);
    return false;
  }

  puglSetBackend(view, puglStubBackend());
  puglSetEventFunc(view, puglTestIgnoreEvent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 48U, 24U);
  const PuglStatus parentStatus = puglSetParent(view, parent);
  const PuglStatus realizeStatus = puglShow(view, PUGL_SHOW_PASSIVE);
  const PuglNativeView nativeView = puglGetNativeView(view);

  const bool embedded =
    parentStatus == PUGL_SUCCESS && realizeStatus == PUGL_SUCCESS && nativeView &&
    puglGetParent(view) == parent &&
    puglTestNativeViewHasParent(nativeView, parent);

  const PuglStatus unrealizeStatus =
    nativeView ? puglUnrealize(view) : PUGL_FAILURE;
  const bool cleanTeardown = unrealizeStatus == PUGL_SUCCESS &&
                             puglGetNativeView(view) == 0U &&
                             puglTestHostExists(parent);

  puglFreeView(view);
  puglFreeWorld(world);
  puglTestDestroyHost(parent);

  PuglWorld* const missingWorld = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglView* const missingView =
    missingWorld ? puglNewView(missingWorld) : NULL;
  bool missingParentRejected = false;
  if (missingWorld && missingView) {
    puglSetBackend(missingView, puglStubBackend());
    puglSetEventFunc(missingView, puglTestIgnoreEvent);
    puglSetSizeHint(missingView, PUGL_DEFAULT_SIZE, 16U, 16U);
    (void)puglSetParent(missingView, (PuglNativeView)0xDEADU);
    missingParentRejected =
      puglShow(missingView, PUGL_SHOW_PASSIVE) == PUGL_REALIZE_FAILED &&
      puglGetNativeView(missingView) == 0U;
  }

  if (missingView) {
    puglFreeView(missingView);
  }
  if (missingWorld) {
    puglFreeWorld(missingWorld);
  }

  if (!embedded || !cleanTeardown || !missingParentRejected) {
    fprintf(stderr,
            "Browser embedding contract failed: embedded=%d teardown=%d missing=%d\n",
            embedded,
            cleanTeardown,
            missingParentRejected);
  }

  return embedded && cleanTeardown && missingParentRejected;
}

static bool
puglTestAncestorCenterContract(void)
{
  static const unsigned width = 40U;
  static const unsigned height = 20U;

  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglView* const view = world ? puglNewView(world) : NULL;
  if (!world || !view) {
    if (view) {
      puglFreeView(view);
    }
    if (world) {
      puglFreeWorld(world);
    }
    return false;
  }

  puglSetBackend(view, puglStubBackend());
  puglSetEventFunc(view, puglTestIgnoreEvent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, width, height);

  const bool realized = puglShow(view, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS;
  const PuglNativeView nativeView = puglGetNativeView(view);
  const bool centered = realized && nativeView &&
                        puglTestCanvasIsViewportCentered(nativeView, width, height);

  if (realized) {
    (void)puglUnrealize(view);
  }
  puglFreeView(view);
  puglFreeWorld(world);

  if (!centered) {
    fprintf(stderr, "Browser ancestor center must be the viewport center point\n");
  }

  return centered;
}

static bool
puglTestResizableViewContract(void)
{
  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglView* const view = world ? puglNewView(world) : NULL;
  if (!world || !view) {
    if (view) {
      puglFreeView(view);
    }
    if (world) {
      puglFreeWorld(world);
    }
    return false;
  }

  puglSetBackend(view, puglStubBackend());
  puglSetEventFunc(view, puglTestIgnoreEvent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 44U, 28U);
  const PuglStatus hintStatus =
    puglSetViewHint(view, PUGL_RESIZABLE, PUGL_TRUE);
  const bool realized = puglShow(view, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS;
  const PuglNativeView nativeView = puglGetNativeView(view);
  const bool resizable = hintStatus == PUGL_SUCCESS && realized && nativeView &&
                         puglGetViewHint(view, PUGL_RESIZABLE) == PUGL_TRUE &&
                         puglTestCanvasIsResizable(nativeView);

  if (realized) {
    (void)puglUnrealize(view);
  }
  puglFreeView(view);
  puglFreeWorld(world);

  if (!resizable) {
    fprintf(stderr,
            "Browser PUGL_RESIZABLE views must expose a native CSS resize affordance\n");
  }

  return resizable;
}

static bool
puglTestSizeConstraintContract(void)
{
  PuglWorld* const world = puglNewWorld(PUGL_PROGRAM, 0U);
  PuglView* const view = world ? puglNewView(world) : NULL;
  if (!world || !view) {
    if (view) {
      puglFreeView(view);
    }
    if (world) {
      puglFreeWorld(world);
    }
    return false;
  }

  puglSetBackend(view, puglStubBackend());
  puglSetEventFunc(view, puglTestIgnoreEvent);
  puglSetSizeHint(view, PUGL_DEFAULT_SIZE, 64U, 48U);
  const PuglStatus minStatus =
    puglSetSizeHint(view, PUGL_MIN_SIZE, 32U, 24U);
  const PuglStatus maxStatus =
    puglSetSizeHint(view, PUGL_MAX_SIZE, 96U, 72U);
  const PuglStatus resizableStatus =
    puglSetViewHint(view, PUGL_RESIZABLE, PUGL_TRUE);
  const bool realized = puglShow(view, PUGL_SHOW_PASSIVE) == PUGL_SUCCESS;
  const PuglNativeView nativeView = puglGetNativeView(view);
  const bool constrained =
    minStatus == PUGL_SUCCESS && maxStatus == PUGL_SUCCESS &&
    resizableStatus == PUGL_SUCCESS && realized && nativeView &&
    puglTestCanvasHasSizeConstraints(nativeView, 32U, 24U, 96U, 72U);

  if (realized) {
    (void)puglUnrealize(view);
  }
  puglFreeView(view);
  puglFreeWorld(world);

  if (!constrained) {
    fprintf(stderr,
            "Browser size hints must constrain native CSS resizing\n");
  }

  return constrained;
}

static void
puglRunBrowserSemantics(void* const data)
{
  (void)data;

  if (!puglTestBrowserCursorContract() ||
      !puglTestUnsupportedDesktopWindowContract() ||
      !puglTestNativeViewIdentityContract() ||
      !puglTestBrowserEmbeddingContract() ||
      !puglTestAncestorCenterContract() ||
      !puglTestResizableViewContract() ||
      !puglTestSizeConstraintContract()) {
    emscripten_run_script(
      "throw new Error('Browser platform semantics contract failed')");
  }
}

__attribute__((constructor)) static void
puglScheduleBrowserSemantics(void)
{
  // C constructors run before Emscripten enters the page event loop, so defer
  // DOM-dependent view realization until after main() and the document body
  // are available.  Any failure is still surfaced as an uncaught page error by
  // the existing headless browser runner.
  emscripten_set_timeout(puglRunBrowserSemantics, 0.0, NULL);
}