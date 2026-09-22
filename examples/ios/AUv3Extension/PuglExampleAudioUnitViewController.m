// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: 0BSD OR ISC

#include "../Shared/PuglIOSExample.h"
#include "PuglExampleAudioUnit.h"

#import <AudioToolbox/AudioToolbox.h>
#import <CoreAudioKit/CoreAudioKit.h>
#import <UIKit/UIKit.h>

@interface PuglExampleAudioUnitViewController
  : AUViewController <AUAudioUnitFactory> {
@private
  PuglIOSExample       _example;
  CADisplayLink*       _displayLink;
  PuglExampleAudioUnit* _audioUnit;
}
@end

@implementation PuglExampleAudioUnitViewController

- (void)viewDidLoad
{
  [super viewDidLoad];

  self.preferredContentSize = CGSizeMake(640.0, 360.0);
  self.view.backgroundColor = [UIColor blackColor];

  if (puglIOSExampleInit(&_example, PUGL_MODULE, self.view)) {
    _displayLink =
      [[CADisplayLink displayLinkWithTarget:self selector:@selector(step:)] retain];
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop]
                       forMode:NSRunLoopCommonModes];
  }
}

- (void)viewDidLayoutSubviews
{
  [super viewDidLayoutSubviews];
  puglIOSExampleLayoutInParent(&_example, self.view);
}

- (void)step:(CADisplayLink*)displayLink
{
  (void)displayLink;
  puglIOSExampleUpdate(&_example);
}

- (AUAudioUnit*)createAudioUnitWithComponentDescription:
                   (AudioComponentDescription)componentDescription
                                                error:(NSError**)error
{
  [_audioUnit release];
  _audioUnit =
    [[PuglExampleAudioUnit alloc] initWithComponentDescription:componentDescription
                                                       options:0U
                                                         error:error];

  return _audioUnit ? [[_audioUnit retain] autorelease] : nil;
}

- (void)dealloc
{
  if (_displayLink) {
    [_displayLink invalidate];
    [_displayLink release];
    _displayLink = nil;
  }

  puglIOSExampleDestroy(&_example);
  [_audioUnit release];

  [super dealloc];
}

@end
