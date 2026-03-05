#pragma once

#include "Platform.hpp"
#include "Model/CubismUserModel.hpp"
#include "CubismModelSettingJson.hpp"
#include "Id/CubismId.hpp"
#include "Math/CubismMatrix44.hpp"
#include "Motion/CubismExpressionMotionManager.hpp"

using namespace Csm;
using namespace Live2D::Cubism::Framework;

/// C++ model class that extends CubismUserModel with interactive features:
/// eye blink, breathing, drag-following, motion playback, and lip sync.
/// Platform-agnostic: all platform-specific operations go through IPlatform.
class L2DModel : public CubismUserModel {
public:
    explicit L2DModel(IPlatform* platform);
    virtual ~L2DModel();

    /// Phase 1: Load model data from settings JSON (no GL context needed)
    bool LoadModelData(const csmByte* settingBuf, csmSizeInt settingSize, const std::string& modelDir);

    /// Phase 2: Setup GL resources — textures, renderer (requires active GL context)
    void SetupGLResources();

    /// Update and render the model
    void Draw(CubismMatrix44& projection);

    /// Start a motion and return the associated sound file path (or empty string)
    std::string StartMotionByGroupIndex(const csmChar* group, csmInt32 index);

    /// Get count of motions in a group
    csmInt32 GetMotionCount(const csmChar* group);

    /// Set the lip sync mouth open value (0.0 - 1.0)
    void SetMouthOpenY(csmFloat32 v);

    /// Set vowel values for lip sync (0.0 - 1.0 each)
    void SetVowelValues(csmFloat32 a, csmFloat32 i, csmFloat32 u, csmFloat32 e, csmFloat32 o);

    /// Set expression by name (loads from exp3.json files)
    void SetExpression(const char* name);

    /// Get available expression count
    csmInt32 GetExpressionCount() const;

    bool IsModelLoaded() const;
    bool IsReadyToDraw() const;

private:
    void LoadAllMotions();
    void LoadExpressions();

    IPlatform* _platform;
    bool _readyToDraw;
    CubismModelSettingJson* _setting;
    std::string _modelDir;
    double _lastFrameTime;
    csmFloat32 _mouthOpenY;

    // Cached parameter IDs
    CubismIdHandle _idParamAngleX;
    CubismIdHandle _idParamAngleY;
    CubismIdHandle _idParamAngleZ;
    CubismIdHandle _idParamEyeBallX;
    CubismIdHandle _idParamEyeBallY;
    CubismIdHandle _idParamBodyAngleX;
    CubismIdHandle _idParamMouthOpenY;
    CubismIdHandle _idParamA;
    CubismIdHandle _idParamI;
    CubismIdHandle _idParamU;
    CubismIdHandle _idParamE;
    CubismIdHandle _idParamO;

    csmFloat32 _vowelA, _vowelI, _vowelU, _vowelE, _vowelO;

    csmMap<csmString, ACubismMotion*> _motions;
    csmMap<csmString, ACubismMotion*> _expressions;
    CubismExpressionMotionManager* _expressionManager;
};
