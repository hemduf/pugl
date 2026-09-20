Pugl
====

Pugl (PlUgin Graphics Library) is a minimal portability layer for GUIs which is
suitable for use in plugins and applications.  It works on X11, MacOS,
iOS/iPadOS, Windows, and WebAssembly/Emscripten, and includes optional support
for drawing with Vulkan, OpenGL/OpenGL ES/WebGL, and Cairo where available.

Pugl is vaguely similar to libraries like GLUT and GLFW, but has different
goals and priorities:

 * Minimal in scope, providing only a thin interface to isolate
   platform-specific details from applications.

 * Zero dependencies, aside from standard system libraries.

 * Support for embedding in native windows, for example as a plugin or
   component within a larger application that is not based on Pugl.

 * Explicit context and no static data, so that several instances can be used
   within a single program at once.

 * Consistent event-based API that makes dispatching in application or toolkit
   code easy with minimal boilerplate.

 * Suitable for both continuously rendering applications like games, and
   event-driven applications that only draw when necessary.

 * Well-integrated with windowing systems, with support for tracking and
   manipulating special window types, states, and styles.

 * Small, liberally licensed implementation that is suitable for vendoring
   and/or static linking.  Pugl can be installed as a library, or used by
   simply copying the implementation into a project.

iOS and iPadOS
---------------

The iOS port uses UIKit and is designed primarily for embedding in a
host-provided `UIView`.  `puglGetNativeView()` returns that wrapper
`UIView`, and `puglGetNativeWorld()` returns the world's default `UIScreen`.

The initial iOS backend supports view lifecycle/configure/expose events,
raw multi-touch pointer input with stable contact IDs, pressure/contact size,
coalesced motion samples, hardware keyboard input, focus, timers, general
clipboard operations, invalidation, and OpenGL ES 2/3 through
`puglGlBackend()`.  Drag-and-drop, desktop cursor shapes, Cairo, and Vulkan are
not provided by this backend.  A top-level `PUGL_PROGRAM` view is available as
a lightweight `UIWindow`, but scene-managed applications and app extensions
should embed a Pugl view in an application-owned `UIView`.

iOS 14 or newer is the supported deployment baseline.

Objective-C class names are process-global.  Plug-ins that statically embed
Pugl and may coexist with another embedded copy must use a consumer-unique
class prefix.  iOS Meson builds require
`-Dios_objc_class_prefix=MyPluginPugl`; other build systems must define
`PUGL_OBJC_CLASS_PREFIX` while compiling the iOS backend.  The iOS build
fails if no prefix is provided.

WebAssembly
-----------

The Emscripten port is built with Meson using `build-aux/emscripten.ini` and
provides a real browser platform: DOM canvas lifecycle and embedding,
non-blocking event-loop integration, keyboard/text/pointer input, timers,
clipboard/drag-and-drop mappings, fullscreen semantics, and WebGL 1/2 through
`puglGlBackend()`.

Browser security rules apply to permission- or gesture-sensitive APIs such as
clipboard and fullscreen. Desktop-only operations that cannot be represented
faithfully return explicit error/unsupported statuses. See
[WebAssembly and Emscripten](doc/emscripten.rst) for build commands, the API
mapping table, browser limitations, and the Meson-built browser demo.

Stability
---------

Pugl is currently being developed towards a long-term stable API.  For the time
being, however, the API may break occasionally.  Please report any relevant
feedback, or file feature requests, so that we can ensure that the released API
will be stable for as long as possible.

Documentation
-------------

Pugl is a C library that includes C++ bindings.
The reference documentation refers to the C API:

 * [C Documentation (single page)](https://lv2.gitlab.io/pugl/c/singlehtml/)
 * [C Documentation (paginated)](https://lv2.gitlab.io/pugl/c/html/)

The documentation will also be built from the source if the `docs`
configuration option is enabled, and both Doxygen and Sphinx are available.

The C++ documentation is currently a work in progress, for now you will have to
refer to the examples or headers for guidance on using the C++ bindings.

Testing
-------

Some unit tests are included, but unfortunately manual testing is still
required.  The tests and example programs are built by default.  You can run
all the tests at once via ninja:

    meson setup build
    cd build
    ninja test

The [examples](examples) directory contains several demonstration programs that
can be used for manual testing. Emscripten builds with examples and OpenGL
enabled produce `build-wasm/examples/pugl_wasm_demo.html` plus its JavaScript
and WebAssembly payloads.

 -- David Robillard <d@drobilla.net>
