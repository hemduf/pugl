<!-- Copyright 2026 Fabrizio Duhem -->
<!-- SPDX-License-Identifier: 0BSD OR ISC -->

# WebAssembly/Emscripten and CMake delivery plan

GitHub Issues are currently disabled for this repository, so the planned tickets
are tracked here with stable IDs.  They are intentionally small enough to map
one-to-one to pull requests and can be mirrored to GitHub Issues later without
changing the plan.

## Architecture summary

Pugl's public API stays unchanged.  WebAssembly support is implemented as a
real `emscripten` platform alongside `mac`, `win`, and `x11`.

The browser implementation owns a DOM canvas per realized Pugl view, registers
Emscripten HTML5 callbacks, translates browser events to Pugl events, and uses a
non-blocking `puglUpdate()` suitable for the browser event loop.  WebGL is the
first supported browser graphics backend.  The stub backend remains available
for API/build tests.  Cairo and Vulkan are out of scope for the first WASM
milestone.

CMake is additive: the existing Meson build remains supported and is used as a
compatibility reference.

## Work items

### PUGL-WASM-001 — Add native CMake build/install support

**Scope**

- Build the core library on macOS, Windows, and X11.
- Build the stub backend and optional OpenGL/Cairo/Vulkan backends when their
  dependencies are available.
- Export namespaced CMake targets for `find_package(Pugl CONFIG)` consumers.
- Install headers under the same versioned include layout used by Meson.
- Add CMake build and installed-package smoke tests.

**Acceptance**

- Linux, macOS, and Windows CMake CI is green.
- A clean external consumer can use the installed package.
- Meson files are untouched except where documentation requires it.

### PUGL-WASM-002 — Implement the Emscripten platform lifecycle

**Scope**

- Add `src/emscripten.h` and `src/emscripten.c`.
- Implement world/view allocation, realization, unrealization, show/hide,
  sizing, native handles, focus, time, redisplay, and non-blocking updates.
- Represent a view with a deterministic DOM canvas selector/handle.
- Define unsupported desktop-only operations explicitly rather than silently
  succeeding.

**Acceptance**

- Core + stub backend links under `emcc`.
- Realize/configure/show/hide/update/expose lifecycle has automated coverage.
- No blocking sleep/select loop is used on the browser main thread.

### PUGL-WASM-003 — Translate browser input, timers, and clipboard

**Scope**

- Keyboard press/release and text input.
- Pointer enter/leave/move, button, wheel/scroll.
- Focus and resize handling.
- Pugl timers backed by browser timing APIs.
- Clipboard support where browser security rules permit it, with explicit
  `PUGL_UNSUPPORTED` behavior otherwise.

**Acceptance**

- Event payloads use Pugl coordinates/modifier semantics.
- Callback lifetime is safe across unrealize/free.
- Automated browser tests cover representative input/event translation.

### PUGL-WASM-004 — Add the WebGL backend and browser demo

**Scope**

- Add `src/emscripten_gl.c` implementing `puglGlBackend()`.
- Map Pugl context hints to Emscripten WebGL context attributes.
- Make context current for realize/expose and release it on teardown.
- Add a minimal browser demo based on the existing OpenGL examples.

**Acceptance**

- WebGL 1 works; WebGL 2 is selected when requested/supported.
- Expose rendering is visible in a browser smoke test.
- CI produces a runnable HTML/JS/WASM demo artifact.

### PUGL-WASM-005 — Add Emscripten CMake and CI coverage

**Scope**

- Teach CMake to select the `emscripten` platform when `EMSCRIPTEN` is true.
- Build core, stub, and WebGL targets with `emcmake`/Emscripten's CMake
  toolchain.
- Add GitHub Actions using a pinned Emscripten SDK.
- Run browser tests headlessly where practical.

**Acceptance**

- CMake configure/build succeeds with the pinned SDK.
- WASM CI is required by the implementation PRs.
- Build artifacts include `.html`, `.js`, and `.wasm` for the demo.

### PUGL-WASM-006 — Documentation and compatibility QA

**Scope**

- Document CMake subdirectory, installed-package, and Emscripten workflows.
- Document browser-specific limitations and event-loop integration.
- Compare public headers/ABI-facing behavior against the Meson/native build.
- Run a final regression/code-review pass across all WASM/CMake changes.

**Acceptance**

- Documentation contains copy-pasteable build commands.
- Native Meson behavior is not regressed.
- Native CMake and WASM CI are all green.
- Project is ready for manual browser validation.

## Dependency order

`PUGL-WASM-001` can land independently.

`PUGL-WASM-002` -> `PUGL-WASM-003` -> `PUGL-WASM-004` -> `PUGL-WASM-005`

`PUGL-WASM-006` closes the milestone after both the native CMake and WASM paths
are green.
