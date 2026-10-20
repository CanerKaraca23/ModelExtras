#pragma once
#include <plugin.h>
#include "core/base.h"

struct CraneData {
    FeatureFrameState frameState;
    RwFrame *baseTurntable = nullptr;   // osnova
    RwFrame *mainBoom = nullptr;        // strela1
    RwFrame *telescopicBoom = nullptr;  // strela2
    RwFrame *outriggerLeft = nullptr;   // opora_l
    RwFrame *outriggerRight = nullptr;  // opora_r
    RwFrame *hookFrame = nullptr;       // hook_kr_dummy

    float curBaseRot = 0.0f;
    float curBoomPitch = 0.0f;
    float maxBoomPitch = 60.0f;
    float curTelescopicPos = 0.0f;
    float maxTelescopicDist = 4.5f;
    float curOutriggerPos = 0.0f;
    float maxOutriggerDist = 0.8f;
    float curHookPos = 0.0f;
    float maxHookDist = 6.0f;

    bool bInitialized = false;

    CraneData(CVehicle *pVeh) {}
    ~CraneData() {}
};

class MobileCraneFeature : public CVehFeature<CraneData>
{
protected:
    void Init() override;
    void OnToggle(CVehicle *vehicle, bool enabled) override;

public:
    MobileCraneFeature() : CVehFeature<CraneData>("MobileCrane", "FEATURES", eFeatureMatrix::MobileCrane) {}
};
