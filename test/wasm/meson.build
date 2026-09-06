# Copyright 2026 Pugl contributors
# SPDX-License-Identifier: 0BSD OR ISC

executable(
  'pugl_wasm_harness',
  'harness.c',
  c_args: test_c_args,
  install: false,
  link_args: [
    '-sASSERTIONS=1',
    '-sENVIRONMENT=web',
    '-sEXIT_RUNTIME=1',
  ],
  name_suffix: 'html',
)
