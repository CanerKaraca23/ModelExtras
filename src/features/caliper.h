#pragma once
#include <plugin.h>
#include "core/base.h"
#include <vector>

struct CaliperEntry {
    RwFrame *frame = nullptr;
    bool isRear = false;
    float curSteerAngle = 0.0f;
};

struct CaliperData {
    FeatureFrameState frameState;
    std::vector<CaliperEntry> calipers;
    bool bInitialized = false;

    CaliperData(CVehicle *pVeh) {}
    ~CaliperData() {}
};

class BrakeCaliperFeature : public CVehFeature<CaliperData>
{
protected:
    void Init() override;
    void OnToggle(CVehicle *vehicle, bool enabled) override;

public:
    BrakeCaliperFeature() : CVehFeature<CaliperData>("BrakeCaliper", "FEATURES", eFeatureMatrix::BrakeCaliper) {}
};
