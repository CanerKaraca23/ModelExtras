#pragma once
#include <plugin.h>
#include "core/base.h"
#include <vector>

struct LiftAxleEntry {
    RwFrame *axleFrame = nullptr;
    std::vector<RwFrame*> wheels;
    float curLift = 0.0f; // 0.0 = lowered, 1.0 = raised
    float maxTravel = 0.18f; // meters
    float lastWheelRot = 0.0f;
};

struct LiftAxleData {
    FeatureFrameState frameState;
    std::vector<LiftAxleEntry> axles;
    bool bRaised = false;
    bool bInitialized = false;

    LiftAxleData(CVehicle *pVeh) {}
    ~LiftAxleData() {}
};

class LiftAxleFeature : public CVehFeature<LiftAxleData>
{
protected:
    void Init() override;
    void OnToggle(CVehicle *vehicle, bool enabled) override;

public:
    LiftAxleFeature() : CVehFeature<LiftAxleData>("LiftAxle", "FEATURES", eFeatureMatrix::LiftAxle) {}
};
