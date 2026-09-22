// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: 0BSD OR ISC

#include "PuglExampleAudioUnit.h"

#import <AVFoundation/AVFoundation.h>

@implementation PuglExampleAudioUnit

- (instancetype)initWithComponentDescription:(AudioComponentDescription)componentDescription
                                      options:(AudioComponentInstantiationOptions)options
                                        error:(NSError**)outError
{
  self = [super initWithComponentDescription:componentDescription
                                     options:options
                                       error:outError];
  if (!self) {
    return nil;
  }

  AVAudioFormat* const format =
    [[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100.0 channels:2U];
  if (!format) {
    [self release];
    return nil;
  }

  _inputBus = [[AUAudioUnitBus alloc] initWithFormat:format error:outError];
  _outputBus = [[AUAudioUnitBus alloc] initWithFormat:format error:outError];
  [format release];

  if (!_inputBus || !_outputBus) {
    [self release];
    return nil;
  }

  _inputBusses = [[AUAudioUnitBusArray alloc] initWithAudioUnit:self
                                                       busType:AUAudioUnitBusTypeInput
                                                        busses:@[_inputBus]];
  _outputBusses = [[AUAudioUnitBusArray alloc] initWithAudioUnit:self
                                                        busType:AUAudioUnitBusTypeOutput
                                                         busses:@[_outputBus]];
  if (!_inputBusses || !_outputBusses) {
    [self release];
    return nil;
  }

  return self;
}

- (void)dealloc
{
  [_outputBusses release];
  [_inputBusses release];
  [_outputBus release];
  [_inputBus release];
  [super dealloc];
}

- (AUAudioUnitBusArray*)inputBusses
{
  return _inputBusses;
}

- (AUAudioUnitBusArray*)outputBusses
{
  return _outputBusses;
}

- (BOOL)canProcessInPlace
{
  return YES;
}

- (AUInternalRenderBlock)internalRenderBlock
{
  AUInternalRenderBlock const renderBlock =
    ^AUAudioUnitStatus(AudioUnitRenderActionFlags* actionFlags,
                       const AudioTimeStamp*       timestamp,
                       AUAudioFrameCount           frameCount,
                       NSInteger                   outputBusNumber,
                       AudioBufferList*             outputData,
                       const AURenderEvent*         realtimeEventListHead,
                       AURenderPullInputBlock       pullInputBlock) {
      (void)outputBusNumber;
      (void)realtimeEventListHead;

      if (!pullInputBlock) {
        return (AUAudioUnitStatus)kAudioUnitErr_NoConnection;
      }

      return pullInputBlock(actionFlags, timestamp, frameCount, 0, outputData);
    };

  return [[renderBlock copy] autorelease];
}

@end
