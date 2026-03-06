#import "Live2DObjCAdapter.h"
#import "L2DSDKManager.h"
#import "L2DAudioEngine.h"

#include "L2DModel.hpp"
#include "L2DSDKInit.hpp"
#include "Rendering/OpenGL/CubismShader_OpenGLES2.hpp"

using namespace Live2D::Cubism::Framework::Rendering;

// ============================================================
// Objective-C Adapter — thin bridge between Swift and C++ model
// ============================================================

@implementation Live2DObjCAdapter {
    L2DModel* _model;
    L2DAudioEngine* _audioEngine;
    float _scale;
}

- (instancetype)initWithModelPath:(NSString *)path {
    self = [super init];
    if (!self) return nil;

    L2DEnsureSDKInitialized();
    _model = NULL;
    _audioEngine = [[L2DAudioEngine alloc] init];
    _scale = 1.0f;

    // Resolve path: priority to Resource Bundle, then Main Bundle
    NSString *absolutePath = path;
    if (![path hasPrefix:@"/"]) {
        NSBundle *bundle = L2DGetResourceBundle();
        if (bundle) {
            absolutePath = [[bundle resourcePath] stringByAppendingPathComponent:path];
        }
        
        // Fallback to Main Bundle if file not found in resource bundle (or bundle is nil)
        if (bundle == nil || ![[NSFileManager defaultManager] fileExistsAtPath:absolutePath]) {
            absolutePath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:path];
        }
    }
    NSLog(@"[Live2D] Final resolved model path: %@", absolutePath);

    NSData *settingData = [NSData dataWithContentsOfFile:absolutePath];
    if (!settingData) {
        NSLog(@"[Live2D] ERROR: Cannot read settings file!");
        return self;
    }

    NSString *modelDir = [absolutePath stringByDeletingLastPathComponent];

    // Create model with platform instance from shared SDK
    IPlatform* platform = L2DGetPlatform();
    _model = new L2DModel(platform);
    if (!_model->LoadModelData((const csmByte*)settingData.bytes,
                               (csmSizeInt)settingData.length,
                               std::string([modelDir UTF8String]))) {
        delete _model;
        _model = NULL;
    }

    return self;
}

// ============================================================
// Drawing (called from GLKView delegate)
// ============================================================

- (void)drawInRect:(CGRect)rect {
    if (!_model || !_model->IsModelLoaded()) return;

    if (!_model->IsReadyToDraw()) {
        _model->SetupGLResources();
        if (!_model->IsReadyToDraw()) return;
    }

    CubismMatrix44 projection;
    if (rect.size.width > 0 && rect.size.height > 0) {
        float aspect = (float)rect.size.width / (float)rect.size.height;
        if (aspect > 1.0f) {
            projection.Scale(_scale / aspect, _scale);
        } else {
            projection.Scale(_scale, _scale * aspect);
        }
    }

    // Update lip sync from FFT vowel analysis
    [_audioEngine updateAnalysis];
    _model->SetMouthOpenY(_audioEngine.mouthOpenY);
    _model->SetVowelValues(_audioEngine.vowelA,
                           _audioEngine.vowelI,
                           _audioEngine.vowelU,
                           _audioEngine.vowelE,
                           _audioEngine.vowelO);

    _model->Draw(projection);
}

// ============================================================
// Interaction
// ============================================================

- (void)setDragX:(float)x y:(float)y {
    if (_model) _model->SetDragging(x, y);
}

- (void)playMotionGroup:(NSString *)group index:(NSInteger)index {
    if (!_model) return;

    std::string soundPath = _model->StartMotionByGroupIndex(
        [group UTF8String], (csmInt32)index
    );

    // Play associated audio with FFT analysis for vowel lip sync
    if (!soundPath.empty()) {
        NSString *nsSoundPath = [NSString stringWithUTF8String:soundPath.c_str()];
        if ([[NSFileManager defaultManager] fileExistsAtPath:nsSoundPath]) {
            [_audioEngine playAudioFile:nsSoundPath];
            NSLog(@"[Live2D] Playing audio with FFT: %@", [nsSoundPath lastPathComponent]);
        }
    }
}

- (NSInteger)motionCountForGroup:(NSString *)group {
    if (!_model) return 0;
    return _model->GetMotionCount([group UTF8String]);
}

- (void)setScale:(float)scale {
    _scale = scale;
}

- (void)setExpression:(NSString *)name {
    if (_model) _model->SetExpression([name UTF8String]);
}

// ============================================================
// Cleanup
// ============================================================

- (void)releaseModel {
    [_audioEngine stop];
    _audioEngine = nil;
    if (_model) {
        delete _model;
        _model = NULL;
    }
    CubismShader_OpenGLES2::DeleteInstance();
    NSLog(@"[Live2D] Model released, shader cache cleared");
}

- (void)dealloc {
    [self releaseModel];
}

@end
