# Copyright 2026 Pugl contributors
# SPDX-License-Identifier: 0BSD OR ISC

wasm_link_args = [
  '-sASSERTIONS=1',
  '-sENVIRONMENT=web',
  '-sEXIT_RUNTIME=1',
]

wasm_live_link_args = [
  '-sASSERTIONS=1',
  '-sENVIRONMENT=web',
  '-sEXIT_RUNTIME=0',
]

executable(
  'pugl_wasm_harness',
  'harness.c',
  c_args: test_c_args,
  install: false,
  link_args: wasm_link_args,
  name_suffix: 'html',
)

executable(
  'pugl_wasm_lifecycle',
  'test_lifecycle.c',
  c_args: test_c_args,
  dependencies: [pugl_dep, pugl_stub_dep],
  install: false,
  link_args: wasm_link_args,
  name_suffix: 'html',
)

executable(
  'pugl_wasm_input',
  'test_input.c',
  c_args: test_c_args,
  dependencies: [pugl_dep, pugl_stub_dep],
  install: false,
  link_args: wasm_live_link_args,
  name_suffix: 'html',
)

executable(
  'pugl_wasm_services',
  'test_services.c',
  'test_browser_semantics.c',
  'test_browser_aspect_semantics.c',
  'test_browser_string_semantics.c',
  'test_browser_fullscreen_semantics.c',
  c_args: test_c_args,
  dependencies: [pugl_dep, pugl_stub_dep],
  install: false,
  link_args: wasm_live_link_args,
  name_suffix: 'html',
)
