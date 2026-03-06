#include "L2DModel.hpp"

#include "CubismFramework.hpp"
#include "CubismDefaultParameterId.hpp"
#include "Id/CubismIdManager.hpp"
#include "Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp"
#include "Effect/CubismEyeBlink.hpp"
#include "Effect/CubismBreath.hpp"
#include "Motion/CubismMotion.hpp"
#include "Motion/CubismMotionManager.hpp"
#include "Motion/CubismExpressionMotion.hpp"
#include "Motion/CubismExpressionMotionManager.hpp"

#include <cstring>
#include <string>

using namespace Live2D::Cubism::Framework::Rendering;

// ============================================================
// Path helpers
// ============================================================

static std::string GetBaseName(const std::string& path) {
    // Get last path component
    std::string name = path;
    auto slashPos = name.find_last_of('/');
    if (slashPos != std::string::npos) name = name.substr(slashPos + 1);
    // Remove last extension (e.g. ".json")
    auto dotPos = name.find_last_of('.');
    if (dotPos != std::string::npos) name = name.substr(0, dotPos);
    // Remove ".motion3" suffix if present (e.g. "foo.motion3" -> "foo")
    std::string motion3 = ".motion3";
    auto m3Pos = name.find(motion3);
    if (m3Pos != std::string::npos) name.erase(m3Pos, motion3.length());
    return name;
}

// ============================================================
// Construction / Destruction
// ============================================================

L2DModel::L2DModel(IPlatform* platform)
    : CubismUserModel()
    , _platform(platform)
    , _readyToDraw(false)
    , _setting(NULL)
    , _modelDir("")
    , _lastFrameTime(0)
    , _mouthOpenY(0.0f)
    , _idParamAngleX(NULL), _idParamAngleY(NULL), _idParamAngleZ(NULL)
    , _idParamEyeBallX(NULL), _idParamEyeBallY(NULL)
    , _idParamBodyAngleX(NULL), _idParamMouthOpenY(NULL)
    , _idParamA(NULL), _idParamI(NULL), _idParamU(NULL), _idParamE(NULL), _idParamO(NULL)
    , _vowelA(0), _vowelI(0), _vowelU(0), _vowelE(0), _vowelO(0)
    , _expressionManager(NULL)
{}

L2DModel::~L2DModel() {
    for (auto iter = _motions.Begin(); iter != _motions.End(); ++iter) {
        ACubismMotion::Delete(iter->Second);
    }
    _motions.Clear();
    for (auto iter = _expressions.Begin(); iter != _expressions.End(); ++iter) {
        ACubismMotion::Delete(iter->Second);
    }
    _expressions.Clear();
    if (_expressionManager) { delete _expressionManager; _expressionManager = NULL; }
    if (_setting) { delete _setting; _setting = NULL; }
}

// ============================================================
// Phase 1: Load model data (no GL context needed)
// ============================================================

bool L2DModel::LoadModelData(const csmByte* settingBuf, csmSizeInt settingSize, const std::string& modelDir) {
    _setting = new CubismModelSettingJson(settingBuf, settingSize);
    _modelDir = modelDir;

    // Load MOC
    const char* mocFile = _setting->GetModelFileName();
    if (mocFile && strlen(mocFile) > 0) {
        std::string mocPath = _modelDir + "/" + mocFile;
        auto mocData = _platform->LoadFile(mocPath);
        if (!mocData.empty()) {
            LoadModel(mocData.data(), static_cast<csmSizeInt>(mocData.size()));
            _platform->Log("[Live2D] MOC loaded: " + std::string(mocFile));
        }
    }

    if (_model == NULL) {
        _platform->Log("[Live2D] ERROR: Model is NULL!");
        delete _setting; _setting = NULL;
        return false;
    }

    // Layout
    csmMap<csmString, csmFloat32> layout;
    _setting->GetLayoutMap(layout);
    _modelMatrix->SetupFromLayout(layout);

    // Physics
    const char* physicsFile = _setting->GetPhysicsFileName();
    if (physicsFile && strlen(physicsFile) > 0) {
        std::string physicsPath = _modelDir + "/" + physicsFile;
        auto physicsData = _platform->LoadFile(physicsPath);
        if (!physicsData.empty()) {
            LoadPhysics(physicsData.data(), static_cast<csmSizeInt>(physicsData.size()));
        }
    }

    // EyeBlink
    if (_setting->GetEyeBlinkParameterCount() > 0) {
        _eyeBlink = CubismEyeBlink::Create(_setting);
        _platform->Log("[Live2D] EyeBlink enabled");
    }

    // Breath
    _breath = CubismBreath::Create();
    {
        csmVector<CubismBreath::BreathParameterData> p;
        CubismIdManager* ids = CubismFramework::GetIdManager();
        p.PushBack(CubismBreath::BreathParameterData(ids->GetId(DefaultParameterId::ParamAngleX), 0, 15, 6.5345f, 0.5f));
        p.PushBack(CubismBreath::BreathParameterData(ids->GetId(DefaultParameterId::ParamAngleY), 0, 8, 3.5345f, 0.5f));
        p.PushBack(CubismBreath::BreathParameterData(ids->GetId(DefaultParameterId::ParamAngleZ), 0, 10, 5.5345f, 0.5f));
        p.PushBack(CubismBreath::BreathParameterData(ids->GetId(DefaultParameterId::ParamBodyAngleX), 0, 4, 15.5345f, 0.5f));
        p.PushBack(CubismBreath::BreathParameterData(ids->GetId(DefaultParameterId::ParamBreath), 0.5f, 0.5f, 3.2345f, 0.5f));
        _breath->SetParameters(p);
    }

    // Motions
    LoadAllMotions();

    // Cache parameter IDs
    CubismIdManager* idMgr = CubismFramework::GetIdManager();
    _idParamAngleX      = idMgr->GetId(DefaultParameterId::ParamAngleX);
    _idParamAngleY      = idMgr->GetId(DefaultParameterId::ParamAngleY);
    _idParamAngleZ      = idMgr->GetId(DefaultParameterId::ParamAngleZ);
    _idParamEyeBallX    = idMgr->GetId(DefaultParameterId::ParamEyeBallX);
    _idParamEyeBallY    = idMgr->GetId(DefaultParameterId::ParamEyeBallY);
    _idParamBodyAngleX  = idMgr->GetId(DefaultParameterId::ParamBodyAngleX);
    _idParamMouthOpenY  = idMgr->GetId(DefaultParameterId::ParamMouthOpenY);
    _idParamA           = idMgr->GetId("ParamA");
    _idParamI           = idMgr->GetId("ParamI");
    _idParamU           = idMgr->GetId("ParamU");
    _idParamE           = idMgr->GetId("ParamE");
    _idParamO           = idMgr->GetId("ParamO");

    // Load expressions from exp3.json files
    LoadExpressions();

    _model->SaveParameters();
    IsInitialized(true);
    _lastFrameTime = _platform->GetTimeSeconds();
    _platform->Log("[Live2D] Model data loaded (GL setup pending)");
    return true;
}

// ============================================================
// Motion loading
// ============================================================

void L2DModel::LoadAllMotions() {
    csmInt32 groupCount = _setting->GetMotionGroupCount();
    for (csmInt32 g = 0; g < groupCount; g++) {
        const csmChar* groupName = _setting->GetMotionGroupName(g);
        csmInt32 count = _setting->GetMotionCount(groupName);
        for (csmInt32 i = 0; i < count; i++) {
            const csmChar* motionFile = _setting->GetMotionFileName(groupName, i);
            if (!motionFile || strlen(motionFile) == 0) continue;

            std::string motionPath = _modelDir + "/" + motionFile;
            auto motionData = _platform->LoadFile(motionPath);
            if (motionData.empty()) {
                _platform->Log("[Live2D] Failed to load motion: " + std::string(motionFile));
                continue;
            }

            csmString key;
            key.Append(groupName, (csmInt32)strlen(groupName));
            key.Append("_", 1);
            char indexStr[16];
            snprintf(indexStr, sizeof(indexStr), "%d", i);
            key.Append(indexStr, (csmInt32)strlen(indexStr));

            ACubismMotion* motion = LoadMotion(
                motionData.data(),
                static_cast<csmSizeInt>(motionData.size()),
                key.GetRawString(),
                NULL, NULL,
                _setting, groupName, i
            );

            if (motion) {
                _motions[key] = motion;
                _platform->Log("[Live2D] Motion loaded: " + std::string(motionFile) +
                              " (key=" + key.GetRawString() + ")");
            }
        }
    }
    _platform->Log("[Live2D] Total motions loaded: " + std::to_string(_motions.GetSize()));
}

std::string L2DModel::StartMotionByGroupIndex(const csmChar* group, csmInt32 index) {
    csmString key;
    key.Append(group, (csmInt32)strlen(group));
    key.Append("_", 1);
    char indexStr[16];
    snprintf(indexStr, sizeof(indexStr), "%d", index);
    key.Append(indexStr, (csmInt32)strlen(indexStr));

    ACubismMotion* targetMotion = NULL;
    for (auto iter = _motions.Begin(); iter != _motions.End(); ++iter) {
        if (iter->First == key) {
            targetMotion = iter->Second;
            break;
        }
    }

    if (!targetMotion) {
        _platform->Log("[Live2D] Motion not found: " + std::string(key.GetRawString()));
        return "";
    }

    _motionManager->StartMotionPriority(targetMotion, false, 2);
    char msg[256];
    snprintf(msg, sizeof(msg), "[Live2D][%p] Playing motion: %s", this, key.GetRawString());
    _platform->Log(msg);

    // Return sound file path if available in model settings
    if (_setting) {
        const csmChar* soundFile = _setting->GetMotionSoundFileName(group, index);
        if (soundFile && strlen(soundFile) > 0) {
            return _modelDir + "/" + soundFile;
        }
    }

    // Fallback: naming convention — look for sounds/<baseName>.wav
    const char* motionFile = _setting ? _setting->GetMotionFileName(group, index) : nullptr;
    if (motionFile && strlen(motionFile) > 0) {
        std::string baseName = GetBaseName(motionFile);
        std::string soundPath = _modelDir + "/sounds/" + baseName + ".wav";
        if (_platform->FileExists(soundPath)) {
            return soundPath;
        }
    }

    return "";
}

csmInt32 L2DModel::GetMotionCount(const csmChar* group) {
    if (_setting) return _setting->GetMotionCount(group);
    return 0;
}

// ============================================================
// Phase 2: GL resource setup
// ============================================================

void L2DModel::SetupGLResources() {
    if (_readyToDraw || _model == NULL || _setting == NULL) return;
    _platform->Log("[Live2D] Setting up GL resources...");

    CreateRenderer();
    CubismRenderer_OpenGLES2* renderer = GetRenderer<CubismRenderer_OpenGLES2>();
    if (!renderer) {
        _platform->Log("[Live2D] ERROR: CreateRenderer returned NULL!");
        return;
    }
    renderer->UseHighPrecisionMask(true);

    for (csmInt32 i = 0; i < _setting->GetTextureCount(); i++) {
        std::string texPath = _modelDir + "/" + _setting->GetTextureFileName(i);
        uint32_t texId = _platform->LoadTexture(texPath);
        if (texId == 0) {
            _platform->Log("[Live2D] Texture load failed: " + texPath);
            continue;
        }
        renderer->BindTexture(i, texId);
        _platform->Log("[Live2D] Texture " + std::to_string(i) +
                       " bound (id=" + std::to_string(texId) + ")");
    }

    renderer->IsPremultipliedAlpha(false);
    _readyToDraw = true;
    _platform->Log("[Live2D] GL setup complete, ready to draw!");
}

// ============================================================
// Draw (called every frame)
// ============================================================

void L2DModel::Draw(CubismMatrix44& projection) {
    if (!_model || !_readyToDraw) return;

    // Delta time
    double now = _platform->GetTimeSeconds();
    csmFloat32 delta = (csmFloat32)(now - _lastFrameTime);
    _lastFrameTime = now;
    if (delta > 0.1f) delta = 0.1f;

    // Update drag manager
    _dragManager->Update(delta);
    csmFloat32 dragX = _dragManager->GetX();
    csmFloat32 dragY = _dragManager->GetY();

    // Apply base parameters first
    _model->LoadParameters();

    // Update motions on top of base parameters
    _motionManager->UpdateMotion(_model, delta);
    
    // Save parameters state so that physics and next frame's motion overlap correctly
    _model->SaveParameters();

    // Drag → head/eye/body
    _model->AddParameterValue(_idParamAngleX,     dragX * 30.0f);
    _model->AddParameterValue(_idParamAngleY,     dragY * 30.0f);
    _model->AddParameterValue(_idParamAngleZ,     dragX * dragY * -30.0f);
    _model->AddParameterValue(_idParamBodyAngleX, dragX * 10.0f);
    _model->AddParameterValue(_idParamEyeBallX,   dragX);
    _model->AddParameterValue(_idParamEyeBallY,   dragY);

    // Lip sync — vowel params
    if (_idParamMouthOpenY) {
        _model->AddParameterValue(_idParamMouthOpenY, _mouthOpenY);
    }
    if (_idParamA) _model->AddParameterValue(_idParamA, _vowelA);
    if (_idParamI) _model->AddParameterValue(_idParamI, _vowelI);
    if (_idParamU) _model->AddParameterValue(_idParamU, _vowelU);
    if (_idParamE) _model->AddParameterValue(_idParamE, _vowelE);
    if (_idParamO) _model->AddParameterValue(_idParamO, _vowelO);

    // EyeBlink
    if (_eyeBlink) _eyeBlink->UpdateParameters(_model, delta);

    // Breath
    if (_breath) _breath->UpdateParameters(_model, delta);

    // Physics
    if (_physics) _physics->Evaluate(_model, delta);

    // Expression
    if (_expressionManager) _expressionManager->UpdateMotion(_model, delta);

    _model->Update();

    // Render
    CubismRenderer_OpenGLES2* renderer = GetRenderer<CubismRenderer_OpenGLES2>();
    if (renderer) {
        renderer->SetMvpMatrix(&projection);
        renderer->DrawModel();
    }
}

// ============================================================
// Accessors
// ============================================================

void L2DModel::SetMouthOpenY(csmFloat32 v) { _mouthOpenY = v; }
void L2DModel::SetVowelValues(csmFloat32 a, csmFloat32 i, csmFloat32 u, csmFloat32 e, csmFloat32 o) {
    _vowelA = a; _vowelI = i; _vowelU = u; _vowelE = e; _vowelO = o;
}
bool L2DModel::IsModelLoaded() const { return _model != NULL; }
bool L2DModel::IsReadyToDraw() const { return _readyToDraw; }

csmInt32 L2DModel::GetExpressionCount() const {
    return _expressions.GetSize();
}

std::vector<std::string> L2DModel::GetMotionGroupNames() const {
    std::vector<std::string> groups;
    if (!_setting) return groups;
    
    csmInt32 count = _setting->GetMotionGroupCount();
    for (csmInt32 i = 0; i < count; i++) {
        const csmChar* groupName = _setting->GetMotionGroupName(i);
        if (groupName) {
            groups.push_back(std::string(groupName));
        }
    }
    return groups;
}

void L2DModel::LoadExpressions() {
    if (!_setting) return;
    _expressionManager = new CubismExpressionMotionManager();

    csmInt32 count = _setting->GetExpressionCount();
    for (csmInt32 i = 0; i < count; i++) {
        const csmChar* name = _setting->GetExpressionName(i);
        const csmChar* file = _setting->GetExpressionFileName(i);
        if (!file || strlen(file) == 0) continue;

        std::string expPath = _modelDir + "/" + file;
        auto expData = _platform->LoadFile(expPath);
        if (expData.empty()) {
            _platform->Log("[Live2D] Failed to load expression: " + std::string(file));
            continue;
        }

        ACubismMotion* motion = CubismExpressionMotion::Create(
            expData.data(), static_cast<csmSizeInt>(expData.size())
        );
        if (motion) {
            csmString key(name);
            _expressions[key] = motion;
            _platform->Log("[Live2D] Expression loaded: " + std::string(name));
        }
    }
    _platform->Log("[Live2D] Total expressions loaded: " + std::to_string(_expressions.GetSize()));
}

void L2DModel::SetExpression(const char* name) {
    if (!_expressionManager) return;

    csmString key(name);
    ACubismMotion* targetMotion = NULL;
    for (auto iter = _expressions.Begin(); iter != _expressions.End(); ++iter) {
        if (iter->First == key) {
            targetMotion = iter->Second;
            break;
        }
    }

    if (targetMotion) {
        _expressionManager->StartMotion(targetMotion, false);
        _platform->Log("[Live2D] Expression set: " + std::string(name));
    } else {
        _platform->Log("[Live2D] Expression not found: " + std::string(name));
    }
}
