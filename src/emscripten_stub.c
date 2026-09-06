// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include "stub.h"
#include "types.h"

#include <pugl/pugl.h>
#include <pugl/stub.h>

const PuglBackend*
puglStubBackend(void)
{
  static const PuglBackend backend = {
    puglStubConfigure,
    puglStubCreate,
    puglStubDestroy,
    puglStubEnter,
    puglStubLeave,
    puglStubGetContext,
  };

  return &backend;
}
