#pragma once
#include <plugin.h>
#include "core/base.h"

struct CabTiltData {
    FeatureFrameState frameState;
    RwFrame *cabFrame = nullptr; // kabina_dummy
    float curAngle = 0.0f;
    float maxAngle = 48.0f;
    bool bInitialized = false;

    CabTiltData(CVehicle *pVeh) {}
    ~CabTiltData() {}
};

class CabTiltFeature : public CVehFeature<CabTiltData>
{
protected:
    void Init() override;
    void OnToggle(CVehicle *vehicle, bool enabled) override;

public:
    CabTiltFeature() : CVehFeature<CabTiltData>("CabTilt", "FEATURES", eFeatureMatrix::CabTilt) {}
};
