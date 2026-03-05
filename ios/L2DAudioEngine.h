#pragma once

#import <Foundation/Foundation.h>

/// Audio engine with FFT-based vowel detection for Live2D lip sync.
/// Analyzes formant frequencies to output A/I/U/E/O vowel weights.
@interface L2DAudioEngine : NSObject

/// Vowel weights (0.0 - 1.0), updated each frame
@property (nonatomic, readonly) float vowelA;
@property (nonatomic, readonly) float vowelI;
@property (nonatomic, readonly) float vowelU;
@property (nonatomic, readonly) float vowelE;
@property (nonatomic, readonly) float vowelO;
@property (nonatomic, readonly) float mouthOpenY;
@property (nonatomic, readonly) BOOL isPlaying;

/// Play audio file and start FFT analysis
- (void)playAudioFile:(NSString *)path;

/// Stop playback
- (void)stop;

/// Call each frame to update vowel values
- (void)updateAnalysis;

@end
