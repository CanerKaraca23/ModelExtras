#pragma once
#include <plugin.h>
#include "core/base.h"
#include <vector>

struct SpringEntry {
    RwFrame *frame = nullptr;
    int wheelIndex = 0; // 0: LF, 1: RF, 2: LR, 3: RR
    float curScaleZ = 1.0f;
};

struct SpringData {
    FeatureFrameState frameState;
    std::vector<SpringEntry> springs;
    bool bInitialized = false;

    SpringData(CVehicle *pVeh) {}
    ~SpringData() {}
};

class SuspensionSpringsFeature : public CVehFeature<SpringData>
{
protected:
    void Init() override;
    void OnToggle(CVehicle *vehicle, bool enabled) override;

public:
    SuspensionSpringsFeature() : CVehFeature<SpringData>("SuspensionSprings", "FEATURES", eFeatureMatrix::SuspensionSprings) {}
};
