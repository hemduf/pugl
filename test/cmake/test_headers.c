// SPDX-License-Identifier: ISC

// CMake smoke test for the dependency-free public API surface.

#include <pugl/pugl.h> // IWYU pragma: keep
#include <pugl/stub.h> // IWYU pragma: keep

int
main(void)
{
  return puglStubBackend() ? 0 : 1;
}
