// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: 0BSD OR ISC

#ifndef PUGL_EXAMPLES_IOS_PUGLIOS_EXAMPLE_H
#define PUGL_EXAMPLES_IOS_PUGLIOS_EXAMPLE_H

#include <pugl/pugl.h>

#import <UIKit/UIKit.h>

#include <stdbool.h>

typedef struct {
  PuglWorld* world;
  PuglView*  view;
  UILabel*   statusLabel; // Non-owning; retained by the native Pugl view.
} PuglIOSExample;

bool
puglIOSExampleInit(PuglIOSExample* example,
                   PuglWorldType   worldType,
                   UIView*         parent);

void
puglIOSExampleLayoutInParent(PuglIOSExample* example, UIView* parent);

void
puglIOSExampleUpdate(PuglIOSExample* example);

void
puglIOSExampleDestroy(PuglIOSExample* example);

#endif // PUGL_EXAMPLES_IOS_PUGLIOS_EXAMPLE_H
