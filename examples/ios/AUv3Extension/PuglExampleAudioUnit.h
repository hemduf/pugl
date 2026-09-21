// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: 0BSD OR ISC

#ifndef PUGL_EXAMPLES_IOS_PUGL_EXAMPLE_AUDIO_UNIT_H
#define PUGL_EXAMPLES_IOS_PUGL_EXAMPLE_AUDIO_UNIT_H

#import <AudioToolbox/AudioToolbox.h>

@interface PuglExampleAudioUnit : AUAudioUnit {
@private
  AUAudioUnitBus*      _inputBus;
  AUAudioUnitBus*      _outputBus;
  AUAudioUnitBusArray* _inputBusses;
  AUAudioUnitBusArray* _outputBusses;
}
@end

#endif // PUGL_EXAMPLES_IOS_PUGL_EXAMPLE_AUDIO_UNIT_H
