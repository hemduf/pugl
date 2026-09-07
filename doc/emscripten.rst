.. default-domain:: c

###########################
WebAssembly and Emscripten
###########################

Pugl can run in a web browser when cross-compiled with Emscripten.
The browser port uses the same public Pugl API as native builds,
with explicit browser mappings for concepts that do not have a direct desktop
window-system equivalent.
The build system remains Meson-only.

Toolchain
=========

The continuously qualified Emscripten version is **6.0.5**.
The machine file ``build-aux/emscripten.ini`` uses ``emcc``, ``em++``,
``emar``, and ``emstrip`` and identifies the host as ``wasm32`` Emscripten.
It intentionally contains only toolchain and host settings so it can also be
used by Meson superprojects.

Activate the Emscripten SDK so these tools are on ``PATH``, then configure a
standalone build, for example:

.. code-block:: console

   meson setup build-wasm \
     --cross-file build-aux/emscripten.ini \
     -Dbindings_cpp=disabled \
     -Dcairo=disabled \
     -Ddocs=disabled \
     -Dexamples=enabled \
     -Dinstall=disabled \
     -Dopengl=enabled \
     -Dstub=true \
     -Dtests=enabled \
     -Dvulkan=disabled
   meson compile -C build-wasm

Cairo and Vulkan are not browser backends.
The stub backend and WebGL-backed OpenGL backend are available for Emscripten.

Meson subprojects
=================

The same cross file is suitable for a Meson superproject that embeds Pugl as
``subprojects/pugl``.
Project-specific options belong on the Pugl subproject rather than in the
machine file, for example:

.. code-block:: meson

   pugl_dep = dependency('pugl-0', fallback: ['pugl', 'pugl_dep'])
   pugl_stub_dep = dependency(
     'pugl-stub-0',
     fallback: ['pugl', 'pugl_stub_dep'],
   )

Configure the superproject with ``--cross-file`` and use ``-Dpugl:<option>``
for Pugl-specific options.
This is qualified in CI by compiling and linking a minimal Emscripten consumer.

Browser event loop
==================

A browser main thread must not be blocked.
Accordingly, :func:`puglUpdate` is always non-blocking on Emscripten and ignores
its timeout argument.
It dispatches pending Pugl update and expose work and then returns immediately.
Applications should call it from a browser-compatible loop such as
``emscripten_set_main_loop_arg()`` or another host-driven animation/event loop.
Do not implement a desktop-style wait loop around :func:`puglUpdate` in the
browser.

:func:`puglGetTime` uses Emscripten's monotonic clock.
Pugl timers use browser/Emscripten interval callbacks and are detached when a
view is unrealized or freed.
Client events are dispatched without a native window-system message primitive.

DOM views and embedding
=======================

Each realized Pugl view owns a deterministic ``canvas`` element whose ID is of
the form ``pugl-view-N``.
The value returned by :func:`puglGetNativeView` is a stable, non-zero synthetic
integer for the lifetime of the realized view; it is not a JavaScript pointer
or DOM object address.

Top-level views are appended to ``document.body``.
To embed a Pugl view in an existing DOM host, give that host a unique numeric
browser-native handle and expose it with ``data-pugl-native-view``:

.. code-block:: html

   <div data-pugl-native-view="42"></div>

Then set that value as the Pugl parent before realization:

.. code-block:: c

   puglSetParent(view, (PuglNativeView)42);

The host element remains owned by the application; unrealizing or freeing the
Pugl view removes only the Pugl canvas.
If a non-zero parent handle cannot be resolved, realization fails explicitly.

Canvas CSS dimensions are logical Pugl dimensions.
The backing buffer is scaled by ``window.devicePixelRatio`` and
:func:`puglGetScaleFactor` reports the current browser device-pixel ratio, with
a sane fallback if the browser does not provide one.
Browser resize observation produces Pugl configure events.

Input and focus
===============

Keyboard press/release, Unicode text input, focus, pointer motion/enter/leave,
pointer buttons, and wheel scrolling are translated to normal Pugl events.
Pointer Events are used where browser pointer input can be represented by the
existing Pugl event model.

Text input is intentionally separate from physical key handling.
A hidden browser editing control owns text/IME-capable input while Pugl retains
logical view focus.
This allows composition and Unicode text to use browser text semantics instead
of attempting to infer text from key codes.

:func:`puglGrabFocus` and :func:`puglHasFocus` map to browser focus state.
As with all browser focus operations, the browser may restrict focus changes
according to user-activation and embedding policy.

WebGL
=====

The Emscripten implementation of :func:`puglGlBackend` maps Pugl's OpenGL-style
backend API to WebGL.
Requests for OpenGL/OpenGL ES 2.0 use WebGL 1, and requests for version 3.0 use
WebGL 2.
Other requested context versions return :enumerator:`PUGL_BAD_CONFIGURATION`.
The effective context API is reported as ``PUGL_OPENGL_ES_API`` after creation.

Alpha, depth, stencil, and multisampling hints are mapped to
``EmscriptenWebGLContextAttributes``.
Debug contexts are unsupported and the only supported swap interval is 1 (or
``PUGL_DONT_CARE`` before realization).
Contexts are made current for expose and explicit enter/leave operations.
:func:`puglGetContext` exposes the Emscripten WebGL context handle and
:func:`puglGetProcAddress` uses Emscripten's WebGL procedure lookup.

Browser services and security
=============================

Clipboard, drag/drop, and fullscreen APIs are constrained by browser security
rules.
A synchronous Pugl success means that the browser operation was successfully
requested; an asynchronous browser permission denial can still occur later.
Applications must not assume that a browser permission prompt or user-gesture
requirement can be bypassed.

Clipboard
---------

Only the general ``text/plain`` clipboard mapping is supported.
:func:`puglSetClipboard` uses ``navigator.clipboard.writeText()`` and
:func:`puglPaste` uses ``navigator.clipboard.readText()``.
If the Clipboard API is unavailable, these operations return
:enumerator:`PUGL_UNSUPPORTED` rather than pretending success.
Paste data is delivered through the normal ``PUGL_DATA_OFFER`` / ``PUGL_DATA``
flow when the asynchronous read completes.
Pending requests are detached during teardown.

Drag and drop
-------------

Browser drag/drop supports registered ``text/plain`` data with the COPY action.
Other MIME types and unrepresentable data actions return
:enumerator:`PUGL_UNSUPPORTED`.
DOM drag listeners are removed when the view is unrealized.

Fullscreen
----------

``PUGL_VIEW_STYLE_FULLSCREEN`` maps to the browser Fullscreen API.
The actual Pugl style follows ``fullscreenchange`` rather than assuming that a
request succeeded.
Entering fullscreen may require a user gesture, and browsers or embedding
policies can deny the request.
Hiding a fullscreen Pugl view requests browser fullscreen exit before reporting
the view hidden.

Platform API mapping
====================

The following table summarizes browser-specific platform behavior.
Normal argument/lifecycle errors can still return the usual Pugl error statuses.

.. list-table::
   :header-rows: 1
   :widths: 26 46 28

   * - Area
     - Browser mapping
     - Status/limitations
   * - World/view lifecycle
     - Real DOM canvas, realize/unrealize, show/hide, update/expose
     - Supported
   * - Native view
     - Stable synthetic integer handle
     - Supported; not a DOM pointer
   * - Parent embedding
     - ``data-pugl-native-view`` host lookup
     - Supported before realization
   * - Window position
     - No browser top-level window-position equivalent
     - ``PUGL_UNSUPPORTED``
   * - Window size
     - Canvas CSS size plus DPR-scaled backing buffer
     - Supported for realized views
   * - Ancestor centering
     - Browser viewport center
     - Mapped browser semantics
   * - Scale factor
     - ``window.devicePixelRatio``
     - Supported with fallback
   * - Show/hide
     - Canvas visibility
     - Passive show supported; raise/force-raise are not representable
   * - View styles
     - MAPPED, HIDDEN, FULLSCREEN
     - Other desktop stacking/window styles are unsupported
   * - Resizable hint
     - CSS resize plus resize observation
     - Supported
   * - Minimum/maximum size
     - CSS min/max dimensions
     - Supported
   * - Fixed aspect
     - CSS ``aspect-ratio``
     - Supported
   * - Minimum/maximum aspect range
     - No faithful mapping in the current contract
     - ``PUGL_UNSUPPORTED``
   * - Transient parent
     - Zero/no transient relationship only
     - Non-zero transient parent is unsupported
   * - Cursor
     - CSS cursor values for all ``PuglCursor`` values
     - Supported
   * - Window title
     - Browser document/window title
     - Supported
   * - Application/class name
     - No corresponding per-view browser concept
     - ``PUGL_UNSUPPORTED``
   * - Focus
     - Browser focus through the hidden text-input control
     - Supported subject to browser policy
   * - Keyboard/text/pointer/wheel
     - DOM keyboard/text/Pointer Events and wheel translation
     - Supported where representable by Pugl
   * - Timers
     - Emscripten/browser interval callbacks
     - Supported
   * - Client events
     - Direct Pugl event dispatch
     - Supported
   * - Clipboard
     - Async Clipboard API, ``text/plain``
     - Permission/user-gesture constrained
   * - Drag/drop
     - DOM drag/drop, ``text/plain`` COPY
     - Other types/actions unsupported
   * - Fullscreen
     - Browser Fullscreen API
     - User-gesture/policy constrained
   * - Stub graphics backend
     - Existing Pugl stub backend
     - Supported
   * - OpenGL backend
     - WebGL 1/2 through ``puglGlBackend()``
     - Supported for 2.0/3.0 requests as described above
   * - Cairo/Vulkan browser backends
     - None
     - Unsupported/not built for Emscripten

Browser demo
============

With ``-Dexamples=enabled -Dopengl=enabled``, Meson builds
``build-wasm/examples/pugl_wasm_demo.html`` together with its JavaScript and
WebAssembly files.
Serve all three files over HTTP; do not open the HTML from a local ``file://``
URL.
The demo uses the real Pugl WebGL backend and exercises keyboard, pointer,
resize/configure, redraw, and pixel rendering.

The CI workflow uploads the three generated files as the
``pugl-wasm-browser-demo`` artifact and loads the exact generated page in
headless Chromium before publication.
The artifact is intended for final manual checks in current Chromium/Chrome,
Firefox, and Safari.

Qualification
=============

The GitHub Meson workflow qualifies four targets:

* Linux: native Meson configure, build, and tests under Xvfb.
* macOS: native Meson configure, build, and tests on ``macos-15``.
* Windows: native Meson configure, build, and tests with MSVC.
* WebAssembly: Emscripten cross-configure/build, a Meson subproject consumer,
  headless browser lifecycle/input/services/WebGL tests, and the browser demo.

For native local reproduction, use the normal upstream-style Meson setup/build
and test commands for the platform.
For WebAssembly, use the cross-file command above and serve generated browser
outputs over HTTP for interactive testing.
