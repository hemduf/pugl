// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include "internal.h"
#include "ios.h"
#include "stub.h"

#include <pugl/gl.h>

#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/glext.h>
#import <QuartzCore/CAEAGLLayer.h>
#import <UIKit/UIKit.h>

#include <dlfcn.h>
#include <math.h>
#include <stdint.h>

static void
puglIosGlEnsureHint(PuglView* const view,
                    const PuglViewHint hint,
                    const int value)
{
  if (view->hints[hint] == PUGL_DONT_CARE) {
    view->hints[hint] = value;
  }
}

@interface PuglOpenGLView : UIView {
@public
  PuglView*      puglview;
  EAGLContext*   context;
  EAGLContext*   previousContext;
  unsigned       contextDepth;
  GLuint         framebuffer;
  GLuint         colorRenderbuffer;
  GLuint         depthStencilRenderbuffer;
  GLint          drawableWidth;
  GLint          drawableHeight;
}
- (BOOL)resizeDrawable;
- (BOOL)pushContext;
- (BOOL)popContext;
@end

@implementation PuglOpenGLView

+ (Class)layerClass
{
  return [CAEAGLLayer class];
}

- (id)initWithFrame:(CGRect)frame context:(EAGLContext*)newContext
{
  self = [super initWithFrame:frame];
  if (self) {
    context = [newContext retain];
    self.contentScaleFactor = [UIScreen mainScreen].scale;
    self.opaque = NO;

    CAEAGLLayer* const layer = (CAEAGLLayer*)self.layer;
    layer.opaque = NO;
    layer.drawableProperties = @{
      kEAGLDrawablePropertyRetainedBacking : @NO,
      kEAGLDrawablePropertyColorFormat : kEAGLColorFormatRGBA8,
    };
  }
  return self;
}

- (BOOL)pushContext
{
  EAGLContext* const current = [EAGLContext currentContext];

  if (contextDepth == 0U) {
    previousContext = [current retain];
  } else if (current != context) {
    // Cross-view recursive entry would overwrite the restoration chain.
    // Fail closed instead of corrupting the host/other instance context.
    return NO;
  }

  if (![EAGLContext setCurrentContext:context]) {
    if (contextDepth == 0U) {
      [previousContext release];
      previousContext = nil;
    }
    return NO;
  }

  ++contextDepth;
  return YES;
}

- (BOOL)popContext
{
  if (!contextDepth) {
    return NO;
  }

  --contextDepth;
  if (contextDepth) {
    return YES;
  }

  EAGLContext* const restore = previousContext;
  previousContext = nil;
  const BOOL restored = [EAGLContext setCurrentContext:restore];
  [restore release];
  return restored;
}

- (void)destroyDrawable
{
  EAGLContext* const previous = [[EAGLContext currentContext] retain];
  if (![EAGLContext setCurrentContext:context]) {
    [previous release];
    return;
  }

  if (depthStencilRenderbuffer) {
    glDeleteRenderbuffers(1, &depthStencilRenderbuffer);
    depthStencilRenderbuffer = 0U;
  }
  if (colorRenderbuffer) {
    glDeleteRenderbuffers(1, &colorRenderbuffer);
    colorRenderbuffer = 0U;
  }
  if (framebuffer) {
    glDeleteFramebuffers(1, &framebuffer);
    framebuffer = 0U;
  }

  drawableWidth = 0;
  drawableHeight = 0;

  (void)[EAGLContext setCurrentContext:previous];
  [previous release];
}

- (void)dealloc
{
  [self destroyDrawable];

  // Destruction while explicitly entered is misuse, but restore the captured
  // outer context defensively rather than leaving this instance current.
  if (contextDepth) {
    contextDepth = 0U;
    EAGLContext* const restore = previousContext;
    previousContext = nil;
    (void)[EAGLContext setCurrentContext:restore];
    [restore release];
  } else if ([EAGLContext currentContext] == context) {
    [EAGLContext setCurrentContext:nil];
  }

  [context release];
  [super dealloc];
}

- (BOOL)resizeDrawable
{
  if (![EAGLContext setCurrentContext:context]) {
    return NO;
  }

  const GLint expectedWidth =
    MAX(1, (GLint)lrint(self.bounds.size.width * self.contentScaleFactor));
  const GLint expectedHeight =
    MAX(1, (GLint)lrint(self.bounds.size.height * self.contentScaleFactor));

  if (framebuffer && drawableWidth == expectedWidth &&
      drawableHeight == expectedHeight) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    return YES;
  }

  [self destroyDrawable];

  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

  glGenRenderbuffers(1, &colorRenderbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, colorRenderbuffer);
  if (![context renderbufferStorage:GL_RENDERBUFFER
                       fromDrawable:(CAEAGLLayer*)self.layer]) {
    [self destroyDrawable];
    return NO;
  }

  glGetRenderbufferParameteriv(
    GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &drawableWidth);
  glGetRenderbufferParameteriv(
    GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &drawableHeight);
  glFramebufferRenderbuffer(
    GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRenderbuffer);

  const bool needDepth = puglview->hints[PUGL_DEPTH_BITS] > 0;
  const bool needStencil = puglview->hints[PUGL_STENCIL_BITS] > 0;
  if (needDepth || needStencil) {
    glGenRenderbuffers(1, &depthStencilRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRenderbuffer);

    if (needDepth && needStencil) {
      glRenderbufferStorage(GL_RENDERBUFFER,
                            GL_DEPTH24_STENCIL8_OES,
                            drawableWidth,
                            drawableHeight);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                GL_DEPTH_ATTACHMENT,
                                GL_RENDERBUFFER,
                                depthStencilRenderbuffer);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                GL_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER,
                                depthStencilRenderbuffer);
    } else if (needDepth) {
      glRenderbufferStorage(GL_RENDERBUFFER,
                            GL_DEPTH_COMPONENT16,
                            drawableWidth,
                            drawableHeight);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                GL_DEPTH_ATTACHMENT,
                                GL_RENDERBUFFER,
                                depthStencilRenderbuffer);
    } else {
      glRenderbufferStorage(GL_RENDERBUFFER,
                            GL_STENCIL_INDEX8,
                            drawableWidth,
                            drawableHeight);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                GL_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER,
                                depthStencilRenderbuffer);
    }
  }

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    [self destroyDrawable];
    return NO;
  }

  glBindRenderbuffer(GL_RENDERBUFFER, colorRenderbuffer);
  return YES;
}

- (void)layoutSubviews
{
  [super layoutSubviews];

  UIScreen* const screen = self.window.screen;
  self.contentScaleFactor = screen ? screen.scale : [UIScreen mainScreen].scale;

  if ([EAGLContext currentContext] == context) {
    (void)[self resizeDrawable];
  }
}

- (void)displayLayer:(CALayer*)layer
{
  (void)layer;
  PuglWrapperView* const wrapper = (PuglWrapperView*)self.superview;
  [wrapper dispatchExpose:self.bounds];
}

@end

static PuglStatus
puglIosGlConfigure(PuglView* view)
{
  if (view->hints[PUGL_CONTEXT_API] == PUGL_OPENGL_API) {
    view->hints[PUGL_CONTEXT_API] = PUGL_OPENGL_ES_API;
  }

  if (view->hints[PUGL_CONTEXT_API] != PUGL_OPENGL_ES_API) {
    return PUGL_BAD_CONFIGURATION;
  }

  puglIosGlEnsureHint(view, PUGL_DEPTH_BITS, 0);
  puglIosGlEnsureHint(view, PUGL_STENCIL_BITS, 0);
  puglIosGlEnsureHint(view, PUGL_SAMPLES, 0);
  puglIosGlEnsureHint(view, PUGL_SAMPLE_BUFFERS, 0);
  puglIosGlEnsureHint(view, PUGL_DOUBLE_BUFFER, 1);
  puglIosGlEnsureHint(view, PUGL_SWAP_INTERVAL, 1);

  if (view->hints[PUGL_CONTEXT_VERSION_MAJOR] < 2) {
    view->hints[PUGL_CONTEXT_VERSION_MAJOR] = 2;
  }
  if (view->hints[PUGL_CONTEXT_VERSION_MAJOR] > 3 ||
      view->hints[PUGL_CONTEXT_VERSION_MINOR] != 0) {
    return PUGL_BAD_CONFIGURATION;
  }

  if (view->hints[PUGL_CONTEXT_DEBUG] ||
      view->hints[PUGL_DOUBLE_BUFFER] != 1 ||
      view->hints[PUGL_SWAP_INTERVAL] != 1 ||
      view->hints[PUGL_SAMPLES] > 0 ||
      view->hints[PUGL_SAMPLE_BUFFERS] > 0) {
    return PUGL_UNSUPPORTED;
  }

  // EAGL exposes fixed drawable formats.  Publish the actual values rather
  // than leaving requested hints that the backend cannot satisfy.
  const bool needDepth = view->hints[PUGL_DEPTH_BITS] > 0;
  const bool needStencil = view->hints[PUGL_STENCIL_BITS] > 0;
  view->hints[PUGL_RED_BITS] = 8;
  view->hints[PUGL_GREEN_BITS] = 8;
  view->hints[PUGL_BLUE_BITS] = 8;
  view->hints[PUGL_ALPHA_BITS] = 8;
  view->hints[PUGL_DEPTH_BITS] = needDepth ? (needStencil ? 24 : 16) : 0;
  view->hints[PUGL_STENCIL_BITS] = needStencil ? 8 : 0;
  view->hints[PUGL_SAMPLES] = 0;
  view->hints[PUGL_SAMPLE_BUFFERS] = 0;
  view->hints[PUGL_DOUBLE_BUFFER] = 1;
  view->hints[PUGL_SWAP_INTERVAL] = 1;

  return PUGL_SUCCESS;
}

static PuglStatus
puglIosGlCreate(PuglView* view)
{
  const EAGLRenderingAPI api =
    view->hints[PUGL_CONTEXT_VERSION_MAJOR] >= 3
      ? kEAGLRenderingAPIOpenGLES3
      : kEAGLRenderingAPIOpenGLES2;

  EAGLContext* const context = [[EAGLContext alloc] initWithAPI:api];
  if (!context) {
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  PuglOpenGLView* const drawView =
    [[PuglOpenGLView alloc] initWithFrame:view->impl->wrapperView.bounds
                                  context:context];
  [context release];

  if (!drawView) {
    return PUGL_NO_MEMORY;
  }

  drawView->puglview = view;
  view->impl->drawView = drawView;
  return PUGL_SUCCESS;
}

static void
puglIosGlDestroy(PuglView* view)
{
  PuglOpenGLView* const drawView = (PuglOpenGLView*)view->impl->drawView;
  if (!drawView) {
    return;
  }

  [drawView removeFromSuperview];
  drawView->puglview = NULL;
  [drawView release];
  view->impl->drawView = nil;
}

static PuglStatus
puglIosGlEnter(PuglView* view, const PuglExposeEvent* expose)
{
  PuglOpenGLView* const drawView = (PuglOpenGLView*)view->impl->drawView;
  if (!drawView || ![drawView pushContext]) {
    return PUGL_FAILURE;
  }

  // REALIZE/UNREALIZE only require a current context.  An embedded plug-in
  // view may legitimately be realized before the host attaches its UIView to
  // a window, in which case CAEAGLLayer has no drawable yet.  Defer framebuffer
  // creation until an actual expose.
  if (!expose) {
    return PUGL_SUCCESS;
  }

  UIScreen* const screen = drawView.window.screen;
  drawView.contentScaleFactor =
    screen ? screen.scale : [UIScreen mainScreen].scale;

  if (![drawView resizeDrawable]) {
    (void)[drawView popContext];
    return PUGL_BACKEND_FAILED;
  }

  return PUGL_SUCCESS;
}

static PuglStatus
puglIosGlLeave(PuglView* view, const PuglExposeEvent* expose)
{
  PuglOpenGLView* const drawView = (PuglOpenGLView*)view->impl->drawView;
  if (!drawView) {
    return PUGL_FAILURE;
  }

  PuglStatus status = PUGL_SUCCESS;
  if (expose) {
    glBindRenderbuffer(GL_RENDERBUFFER, drawView->colorRenderbuffer);
    status = [drawView->context presentRenderbuffer:GL_RENDERBUFFER]
               ? PUGL_SUCCESS
               : PUGL_FAILURE;
  }

  if (![drawView popContext] && status == PUGL_SUCCESS) {
    status = PUGL_FAILURE;
  }
  return status;
}

static void*
puglIosGlGetContext(PuglView* view)
{
  PuglOpenGLView* const drawView = (PuglOpenGLView*)view->impl->drawView;
  return drawView ? drawView->context : NULL;
}

PuglGlFunc
puglGetProcAddress(const char* name)
{
  return name ? (PuglGlFunc)dlsym(RTLD_DEFAULT, name) : NULL;
}

PuglStatus
puglEnterContext(PuglView* view)
{
  return view && view->backend ? view->backend->enter(view, NULL)
                               : PUGL_BAD_BACKEND;
}

PuglStatus
puglLeaveContext(PuglView* view)
{
  return view && view->backend ? view->backend->leave(view, NULL)
                               : PUGL_BAD_BACKEND;
}

const PuglBackend*
puglGlBackend(void)
{
  static const PuglBackend backend = {
    puglIosGlConfigure,
    puglIosGlCreate,
    puglIosGlDestroy,
    puglIosGlEnter,
    puglIosGlLeave,
    puglIosGlGetContext,
  };

  return &backend;
}
