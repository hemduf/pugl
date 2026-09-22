These programs serve as demonstrations, and as utilities for manual testing.

 * `ios/` contains native iOS examples for a standalone application and an
   Audio Unit v3 extension.  The Xcode project is generated from `ios/project.yml`
   and both examples are compiled by the iOS CI jobs.

 * `pugl_embed_demo` shows a view embedded in another, and also tests
   requesting attention (which happens after 5 seconds), keyboard focus
   (switched by pressing tab), view moving (with the arrow keys), and view
   resizing (with the arrow keys while shift is held).  This program uses only
   very old OpenGL and should work on any system.

 * `pugl_window_demo` demonstrates multiple top-level windows.

 * `pugl_shader_demo` demonstrates using more modern OpenGL (version 3 or 4)
   where dynamic loading and shaders are required.  It can also be used to test
   performance by passing the number of rectangles to draw on the command line.

 * `pugl_cairo_demo` demonstrates using Cairo on top of the native windowing
   system (without OpenGL), and partial redrawing.

 * `pugl_print_events` is a utility that prints all received events to the
   console in a human readable format.

 * `pugl_cpp_demo` is a simple cube demo that uses the C++ API.

 * `pugl_vulkan_demo` is a simple example of using Vulkan in C that simply
   clears the window.

 * `pugl_vulkan_cpp_demo` is a more advanced Vulkan demo in C++ that draws many
   animated rectangles like `pugl_shader_demo`.

 * `pugl_wasm_demo` demonstrates the Emscripten/WebAssembly platform in a web
   browser using the real WebGL backend.  It exercises keyboard and pointer
   input, resize/configure events, redraw, and rendered pixel validation.

Native example programs support several command line options to control various
behaviours, see the output of `--help` for details.  Please file an issue if
any of these programs do not work as expected on your system.

WebAssembly Example
-------------------

The WebAssembly example is built only for Emscripten.  Activate the Emscripten
SDK so its tools are available on `PATH`, then configure and build with:

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

This produces `build-wasm/examples/pugl_wasm_demo.html` together with its
JavaScript and WebAssembly payloads.  Serve the generated files over HTTP rather
than opening the HTML through `file://`.  For example:

    python3 -m http.server 8000 --directory build-wasm/examples

Then open `http://localhost:8000/pugl_wasm_demo.html` in a browser.

See `doc/emscripten.rst` for the complete browser platform contract and
limitations.
