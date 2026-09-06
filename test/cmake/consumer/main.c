// SPDX-License-Identifier: ISC

#include <pugl/pugl.h>
#include <pugl/stub.h>

int
main(void)
{
  const PuglBackend* const backend = puglStubBackend();
  return backend ? 0 : 1;
}
